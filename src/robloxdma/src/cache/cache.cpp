#include <cache/cache.h>
#include <globals.h>
#include <game/game.h>
#include <sdk/offsets/offsets.h>
#include <dma_helper.h>
#include "VolkDMA/process.hh"
#include <settings/settings.h>
#include <chrono>
#include <unordered_set>
static void* scatter_handle = nullptr;
static std::unordered_map<std::uint64_t, cache::entity_t*> entity_ptr_map;
static std::unordered_map<std::uint64_t, std::uint64_t> last_model_per_player;
static std::vector<cache::entity_t> cached_entities;
static std::vector<cache::entity_t*> entities_to_update;
static void FetchEntities();
static void FetchEntityData(std::vector<cache::entity_t*>& entities_to_update);
static void UpdatePositions();
void cache::run() {
	sleep(2000);
	scatter_handle = get_process() ? get_process()->create_scatter(0) : nullptr;
	if (!scatter_handle)
	{
		LOG("[!] CRITICAL: Failed to create scatter handle\n");
		return;
	}
	cached_entities.reserve(64);
	entity_ptr_map.reserve(64);
	entities_to_update.reserve(64);
	LOG("[+] Cache system starting...\n");
	while (true)
	{
		try
		{
			int cache_interval_ms = settings::performance::recache_dead_check_seconds * 1000;
			if (cache_interval_ms < 300) cache_interval_ms = 300;
			if (cache_interval_ms > 30000) cache_interval_ms = 30000;
			if (RateLimiter::get_full_cache().should_run_ms(cache_interval_ms))
			{
				FetchEntities();
			}
			if (RateLimiter::get_position_update().should_run())
			{
				UpdatePositions();
			}
			sleep(3);
		}
		catch (...)
		{
			sleep(100);
		}
	}
}
static void FetchEntities()
{
	game::refresh_datamodel();
	if (game::players_service.address == 0 || game::players_service.address <= 0x10000)
	{
		return;
	}
	std::vector<rbx::player_t> players;
	try {
		players = game::players_service.get_children<rbx::player_t>();
	}
	catch (...) {
		return;
	}
	if (players.empty())
	{
		return;
	}
	std::vector<std::uint64_t> model_instances(players.size(), 0);
	std::vector<std::uint64_t> model_parents(players.size(), 0);
	std::uint64_t localplayer_addr = 0;
	if (!scatter_handle)
	{
		return;
	}
	get_process()->add_read_scatter(scatter_handle, game::players_service.address + Offsets::Player::LocalPlayer, &localplayer_addr, sizeof(localplayer_addr));
	for (size_t i = 0; i < players.size(); ++i)
	{
		if (players[i].address > 0x10000)
			get_process()->add_read_scatter(scatter_handle, players[i].address + Offsets::Player::ModelInstance, &model_instances[i], sizeof(std::uint64_t));
	}
	get_process()->execute_scatter(scatter_handle);
	if (localplayer_addr == 0 || localplayer_addr <= 0x10000)
	{
		return;
	}
	rbx::player_t localplayer = rbx::player_t(localplayer_addr);
	for (size_t i = 0; i < players.size(); ++i)
	{
		if (model_instances[i] > 0x10000)
			get_process()->add_read_scatter(scatter_handle, model_instances[i] + Offsets::Instance::Parent, &model_parents[i], sizeof(std::uint64_t));
	}
	get_process()->execute_scatter(scatter_handle);
	entity_ptr_map.clear();
	for (auto& entity : cache::entity_cache)
		entity_ptr_map[entity.instance.address] = &entity;

	cached_entities.clear();
	if (cached_entities.capacity() < players.size())
		cached_entities.reserve(players.size());
	entities_to_update.clear();
	if (entities_to_update.capacity() < players.size())
		entities_to_update.reserve(players.size());

	for (size_t i = 0; i < players.size(); ++i)
	{
		std::uint64_t current_model = model_instances[i];
		std::uint64_t current_model_parent = model_parents[i];
		bool valid_model = (current_model > 0x10000 && current_model < 0x7FFFFFFFFFFF &&
		                    current_model_parent > 0x10000 && current_model_parent < 0x7FFFFFFFFFFF);
		std::uint64_t player_addr = players[i].address;
		std::uint64_t prev_model = 0;
		auto prev_it = last_model_per_player.find(player_addr);
		if (prev_it != last_model_per_player.end())
			prev_model = prev_it->second;
		bool model_changed = (current_model != prev_model);

		if (!valid_model)
		{
			last_model_per_player[player_addr] = 0;
			cached_entities.emplace_back();
			cache::entity_t& entity = cached_entities.back();
			entity.instance = players[i];
			entity.model_instance_addr = 0;
			continue;
		}

		if (model_changed)
		{
			last_model_per_player[player_addr] = current_model;
			cached_entities.emplace_back();
			cache::entity_t& entity = cached_entities.back();
			entity.instance = players[i];
			entity.model_instance_addr = current_model;
			entities_to_update.push_back(&entity);
		}
		else
		{
			auto cache_it = entity_ptr_map.find(player_addr);
			if (cache_it != entity_ptr_map.end())
			{
				cached_entities.push_back(*cache_it->second);
				cache::entity_t& entity = cached_entities.back();
				entity.instance = players[i];
				entity.model_instance_addr = current_model;
			}
			else
			{
				last_model_per_player[player_addr] = current_model;
				cached_entities.emplace_back();
				cache::entity_t& entity = cached_entities.back();
				entity.instance = players[i];
				entity.model_instance_addr = current_model;
				entities_to_update.push_back(&entity);
			}
		}
	}
	if (!entities_to_update.empty())
	{
		FetchEntityData(entities_to_update);
		for (cache::entity_t* entity : entities_to_update)
		{
			if (entity && entity->model_instance_addr == 0)
				last_model_per_player[entity->instance.address] = 0;
		}
	}
	{
		std::vector<cache::entity_t> alive_only;
		alive_only.reserve(cached_entities.size());
		for (auto& entity : cached_entities)
		{
			if (entity.instance.address == localplayer.address)
			{
				cache::frame_data.local_entity = entity;
				cache::local_entity = entity;
				continue;
			}
			if (entity.model_instance_addr == 0 || entity.model_instance_addr <= 0x10000)
				continue;
			if (entity.health <= 0.0f || !std::isfinite(entity.health))
				continue;
			if (entity.parts.empty() || entity.primitive_cache.empty())
				continue;
			alive_only.push_back(entity);
		}
		std::lock_guard<std::mutex> lock(cache::frame_data.mtx);
		cache::frame_data.entities = std::move(alive_only);
	}
	cache::entity_cache = std::move(cached_entities);
	cached_entities = cache::entity_cache;
}
static void FetchEntityData(std::vector<cache::entity_t*>& entities_to_update)
{
	if (entities_to_update.empty() || !scatter_handle)
		return;
	std::vector<std::uint64_t> model_instances(entities_to_update.size(), 0);
	std::vector<std::uint64_t> teams(entities_to_update.size(), 0);
	std::vector<std::uint64_t> userids(entities_to_update.size(), 0);
	for (size_t i = 0; i < entities_to_update.size(); ++i)
	{
		auto* entity = entities_to_update[i];
		if (entity && entity->instance.address > 0x10000)
		{
			get_process()->add_read_scatter(scatter_handle, entity->instance.address + Offsets::Player::ModelInstance, &model_instances[i], sizeof(std::uint64_t));
			get_process()->add_read_scatter(scatter_handle, entity->instance.address + Offsets::Player::Team, &teams[i], sizeof(std::uint64_t));
			get_process()->add_read_scatter(scatter_handle, entity->instance.address + Offsets::Player::UserId, &userids[i], sizeof(std::uint64_t));
		}
	}
	get_process()->execute_scatter(scatter_handle);
	for (size_t i = 0; i < entities_to_update.size(); ++i)
	{
		auto* entity = entities_to_update[i];
		if (entity)
		{
			entity->model_instance_addr = model_instances[i];
			entity->team = teams[i];
			entity->userid = userids[i];
		}
	}
	if (settings::visuals::master && settings::visuals::name)
	{
		for (auto* entity : entities_to_update)
		{
			if (entity && entity->instance.address > 0x10000)
			{
				try {
					entity->name = entity->instance.get_name();
				}
				catch (...) {
					entity->name = "Unknown";
				}
			}
		}
	}
	static const std::unordered_set<std::string> essential_parts = {
		"HumanoidRootPart", "Head", "UpperTorso", "LowerTorso", "Torso"
	};
	const std::unordered_set<std::string>& parts_to_read = essential_parts;
	std::vector<std::uint64_t> model_parents(entities_to_update.size(), 0);
	for (size_t i = 0; i < entities_to_update.size(); ++i)
	{
		cache::entity_t* entity = entities_to_update[i];
		if (entity && entity->model_instance_addr > 0x10000)
			get_process()->add_read_scatter(scatter_handle, entity->model_instance_addr + Offsets::Instance::Parent, &model_parents[i], sizeof(std::uint64_t));
	}
	get_process()->execute_scatter(scatter_handle);
	for (size_t i = 0; i < entities_to_update.size(); ++i)
	{
		cache::entity_t* entity = entities_to_update[i];
		if (!entity || entity->model_instance_addr == 0 || entity->model_instance_addr <= 0x10000)
			continue;
		std::uint64_t model_parent = model_parents[i];
		try {
			if (model_parent == 0 || model_parent <= 0x10000)
			{
				entity->parts.clear();
				entity->primitive_cache.clear();
				entity->model_instance_addr = 0;
				continue;
			}
			rbx::instance_t model_instance(entity->model_instance_addr);
			auto children = model_instance.get_children();
			entity->parts.clear();
			entity->primitive_cache.clear();
			bool found_humanoid = false;
			struct PartToRead {
				std::string name;
				std::uint64_t part_addr;
				std::uint64_t prim_addr;
				math::vector3 position;
				math::vector3 size;
			};
			std::vector<PartToRead> parts_batch;
			parts_batch.reserve(parts_to_read.size());
			for (auto& child : children)
			{
				if (child.address == 0 || child.address <= 0x10000)
					continue;
				try {
					std::string child_name = child.get_name();
					bool is_needed_part = parts_to_read.count(child_name) > 0;
					bool is_humanoid = (child_name == "Humanoid");
					if (is_needed_part)
					{
						PartToRead part;
						part.name = child_name;
						part.part_addr = child.address;
						parts_batch.push_back(part);
						entity->parts[child_name] = rbx::meshpart_t(child.address);
					}
					else if (is_humanoid)
					{
						found_humanoid = true;
						entity->humanoid = rbx::humanoid_t(child.address);
						try {
							entity->health = get_process()->read<float>(child.address + Offsets::Humanoid::Health);
							entity->max_health = get_process()->read<float>(child.address + Offsets::Humanoid::MaxHealth);
							entity->humanoid_state = get_process()->read<std::int16_t>(child.address + Offsets::Humanoid::HumanoidState);
							if (!std::isfinite(entity->health)) entity->health = 0.0f;
							if (!std::isfinite(entity->max_health)) entity->max_health = 100.0f;
						}
						catch (...) {
							entity->health = 0.0f;
							entity->max_health = 100.0f;
						}
					}
				}
				catch (...) {
					continue;
				}
			}
			if (!parts_batch.empty())
			{
				for (auto& part : parts_batch)
				{
					get_process()->add_read_scatter(scatter_handle, part.part_addr + Offsets::BasePart::Primitive, &part.prim_addr, sizeof(std::uint64_t));
				}
				get_process()->execute_scatter(scatter_handle);
				for (auto& part : parts_batch)
				{
					if (part.prim_addr > 0x10000)
					{
						get_process()->add_read_scatter(scatter_handle, part.prim_addr + Offsets::BasePart::Position, &part.position, sizeof(math::vector3));
						get_process()->add_read_scatter(scatter_handle, part.prim_addr + Offsets::BasePart::Size, &part.size, sizeof(math::vector3));
					}
				}
				get_process()->execute_scatter(scatter_handle);
				for (const auto& part : parts_batch)
				{
					if (part.prim_addr > 0x10000 &&
					    std::isfinite(part.position.x) &&
					    std::isfinite(part.position.y) &&
					    std::isfinite(part.position.z))
					{
						auto& prim_data = entity->primitive_cache[part.name];
						prim_data.position = part.position;
						prim_data.size = part.size;
						prim_data.velocity = math::vector3(0, 0, 0);
						if (part.name == "HumanoidRootPart")
						{
							entity->root_position = part.position;
						}
					}
					else
					{
						entity->primitive_cache.erase(part.name);
						entity->parts.erase(part.name);
					}
				}
			}
			if (found_humanoid && (entity->health <= 0.0f || !std::isfinite(entity->health)))
			{
				entity->parts.clear();
				entity->primitive_cache.clear();
				entity->model_instance_addr = 0;
				entity->root_position = math::vector3(0, 0, 0);
			}
			else if (!found_humanoid)
			{
				entity->parts.clear();
				entity->primitive_cache.clear();
				entity->model_instance_addr = 0;
				entity->root_position = math::vector3(0, 0, 0);
			}
		}
		catch (...) {
			entity->parts.clear();
			entity->primitive_cache.clear();
			entity->model_instance_addr = 0;
		}
	}
}
static void UpdatePositions()
{
	if (!scatter_handle || cache::entity_cache.empty())
		return;
	for (size_t i = 0; i < cache::entity_cache.size(); ++i)
	{
		auto& entity = cache::entity_cache[i];
		if (entity.model_instance_addr == 0) continue;
		std::uint64_t cur_model = 0;
		std::uint64_t cur_parent = 0;
		try {
			cur_model = get_process()->read<std::uint64_t>(entity.instance.address + Offsets::Player::ModelInstance);
			if (cur_model > 0x10000)
				cur_parent = get_process()->read<std::uint64_t>(cur_model + Offsets::Instance::Parent);
		} catch (...) { cur_model = 0; cur_parent = 0; }
		if (cur_model == 0 || cur_model <= 0x10000 || cur_parent == 0 || cur_parent <= 0x10000)
		{
			entity.parts.clear();
			entity.primitive_cache.clear();
			entity.model_instance_addr = 0;
			entity.root_position = math::vector3(0, 0, 0);
		}
	}
	bool update_all_parts = false;
	static const std::unordered_set<std::string> essential_update_parts = {
		"HumanoidRootPart", "Head", "UpperTorso", "LowerTorso", "Torso"
	};
	static std::unordered_map<std::uint64_t, std::unordered_map<std::string, std::uint64_t>> cached_prim_addrs;
	static std::unordered_map<std::uint64_t, std::unordered_map<std::string, math::vector3>> positions_map;
	static std::unordered_map<std::uint64_t, std::unordered_map<std::string, math::vector3>> velocities_map;
	static size_t last_cache_size = 0;
	if (cache::entity_cache.size() != last_cache_size)
	{
		last_cache_size = cache::entity_cache.size();
		cached_prim_addrs.clear();
	}
	positions_map.clear();
	velocities_map.clear();
	struct PrimRead { std::uint64_t player_addr; std::string part_name; std::uint64_t part_addr; std::uint64_t prim_addr; };
	std::vector<PrimRead> prim_reads;
	for (size_t i = 0; i < cache::entity_cache.size(); ++i)
	{
		auto& entity = cache::entity_cache[i];
		std::uint64_t player_addr = entity.instance.address;
		auto cached_entity_it = cached_prim_addrs.find(player_addr);
		bool entity_needs_refresh = (cached_entity_it == cached_prim_addrs.end() ||
		                             cached_entity_it->second.size() != entity.parts.size());
		for (const auto& part_pair : entity.parts)
		{
			const std::string& part_name = part_pair.first;
			const rbx::meshpart_t& part = part_pair.second;
			if (!update_all_parts && essential_update_parts.count(part_name) == 0)
				continue;
			if (part.address <= 0x10000)
				continue;
			std::uint64_t prim_addr = 0;
			if (!entity_needs_refresh && cached_entity_it != cached_prim_addrs.end())
			{
				auto part_it = cached_entity_it->second.find(part_name);
				if (part_it != cached_entity_it->second.end())
					prim_addr = part_it->second;
				else
					entity_needs_refresh = true;
			}
			if (entity_needs_refresh || prim_addr == 0)
			{
				auto prim_cache_it = entity.primitive_cache.find(part_name);
				if (prim_cache_it != entity.primitive_cache.end())
					prim_reads.push_back({ player_addr, part_name, part.address, 0 });
			}
		}
	}
	if (!prim_reads.empty())
	{
		for (auto& pr : prim_reads)
			get_process()->add_read_scatter(scatter_handle, pr.part_addr + Offsets::BasePart::Primitive, &pr.prim_addr, sizeof(std::uint64_t));
		get_process()->execute_scatter(scatter_handle);
		for (auto& pr : prim_reads)
			cached_prim_addrs[pr.player_addr][pr.part_name] = pr.prim_addr;
	}
	for (size_t i = 0; i < cache::entity_cache.size(); ++i)
	{
		auto& entity = cache::entity_cache[i];
		std::uint64_t player_addr = entity.instance.address;
		auto cached_entity_it = cached_prim_addrs.find(player_addr);
		for (const auto& part_pair : entity.parts)
		{
			const std::string& part_name = part_pair.first;
			const rbx::meshpart_t& part = part_pair.second;
			if (!update_all_parts && essential_update_parts.count(part_name) == 0)
				continue;
			if (part.address <= 0x10000)
				continue;
			std::uint64_t prim_addr = 0;
			if (cached_entity_it != cached_prim_addrs.end())
			{
				auto part_it = cached_entity_it->second.find(part_name);
				if (part_it != cached_entity_it->second.end())
					prim_addr = part_it->second;
			}
			if (prim_addr > 0x10000)
			{
				get_process()->add_read_scatter(scatter_handle, prim_addr + Offsets::BasePart::Position, &positions_map[player_addr][part_name], sizeof(math::vector3));
				if (part_name == "HumanoidRootPart" || part_name == "Head")
					get_process()->add_read_scatter(scatter_handle, prim_addr + Offsets::BasePart::AssemblyLinearVelocity, &velocities_map[player_addr][part_name], sizeof(math::vector3));
			}
		}
	}
	if (positions_map.empty())
		return;
	get_process()->execute_scatter(scatter_handle);
	for (auto& entity : cache::entity_cache)
	{
		std::uint64_t player_addr = entity.instance.address;
		auto pos_it = positions_map.find(player_addr);
		if (pos_it == positions_map.end())
			continue;
		for (auto& part_pair : pos_it->second)
		{
			const std::string& part_name = part_pair.first;
			const math::vector3& position = part_pair.second;
			entity.primitive_cache[part_name].position = position;
			auto vel_it = velocities_map.find(player_addr);
			if (vel_it != velocities_map.end())
			{
				auto part_vel_it = vel_it->second.find(part_name);
				if (part_vel_it != vel_it->second.end())
					entity.primitive_cache[part_name].velocity = part_vel_it->second;
			}
			if (part_name == "HumanoidRootPart")
				entity.root_position = position;
		}
	}
	{
		std::vector<cache::entity_t> alive_only;
		alive_only.reserve(cache::entity_cache.size());
		for (const auto& entity : cache::entity_cache)
		{
			if (entity.model_instance_addr == 0 || entity.model_instance_addr <= 0x10000)
				continue;
			if (entity.health <= 0.0f || !std::isfinite(entity.health))
				continue;
			if (entity.parts.empty() || entity.primitive_cache.empty())
				continue;
			alive_only.push_back(entity);
		}
		std::lock_guard<std::mutex> lock(cache::frame_data.mtx);
		cache::frame_data.entities = std::move(alive_only);
		for (const auto& entity : cache::entity_cache)
		{
			if (entity.instance.address == cache::frame_data.local_entity.instance.address)
			{
				cache::frame_data.local_entity = entity;
				cache::local_entity = entity;
				break;
			}
		}
	}
}