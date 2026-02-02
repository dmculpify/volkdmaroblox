#include <cache/cache.h>
#include <globals.h>
#include <game/game.h>
#include <sdk/offsets/offsets.h>
#include <Memory/Memory.h>
#include <settings/settings.h>
#include <chrono>
static void* scatter_handle = nullptr;
static std::unordered_map<std::uint64_t, cache::entity_t*> entity_ptr_map;
static std::vector<cache::entity_t> cached_entities;
static void FetchEntities();
static void FetchEntityData(std::vector<cache::entity_t*>& entities_to_update);
static void UpdatePositions();
void cache::run() {
	scatter_handle = memory.CreateScatterHandle();
	cached_entities.reserve(64);
	entity_ptr_map.reserve(64);
	while (true)
	{
		try
		{
			if (RateLimiter::get_full_cache().should_run())
			{
				FetchEntities();
			}
			if (settings::performance::cache_primitives && RateLimiter::get_position_update().should_run())
			{
				UpdatePositions();
			}
			sleep(5);
		}
		catch (...)
		{
		}
	}
}
static void FetchEntities()
{
	if (!scatter_handle)
		return;
	std::vector<rbx::player_t> players = game::players_service.get_children<rbx::player_t>();
	if ((int)players.size() > settings::performance::max_players)
	{
		players.resize(settings::performance::max_players);
	}
	if (players.empty() || game::players_service.address == 0)
	{
		return;
	}
	std::uint64_t localplayer_addr = memory.Read<std::uint64_t>(game::players_service.address + Offsets::Player::LocalPlayer);
	rbx::player_t localplayer = rbx::player_t(localplayer_addr);
	entity_ptr_map.clear();
	for (auto& entity : entity_cache)
	{
		entity_ptr_map[entity.instance.address] = &entity;
	}
	cached_entities.clear();
	cached_entities.reserve(players.size());
	std::vector<cache::entity_t*> entities_to_update;
	entities_to_update.reserve(players.size());
	for (size_t i = 0; i < players.size(); ++i)
	{
		cached_entities.emplace_back();
		cache::entity_t& entity = cached_entities.back();
		entity.instance = players[i];
		auto it = entity_ptr_map.find(players[i].address);
		if (it == entity_ptr_map.end())
		{
			entities_to_update.push_back(&entity);
		}
		else
		{
			entity = *it->second;
		}
	}
	if (!entities_to_update.empty())
	{
		FetchEntityData(entities_to_update);
	}
	{
		std::lock_guard<std::mutex> lock(frame_data.mtx);
		frame_data.entities = cached_entities;
		for (auto& entity : frame_data.entities)
		{
			if (entity.instance.address == localplayer.address)
			{
				frame_data.local_entity = entity;
				local_entity = entity;
				break;
			}
		}
	}
	entity_cache = std::move(cached_entities);
	cached_entities = entity_cache;
}
static void FetchEntityData(std::vector<cache::entity_t*>& entities_to_update)
{
	if (entities_to_update.empty() || !scatter_handle)
		return;
	std::vector<std::uint64_t> player_model_instances(entities_to_update.size(), 0);
	std::vector<std::uint64_t> player_teams(entities_to_update.size(), 0);
	std::vector<std::uint64_t> player_userids(entities_to_update.size(), 0);
	std::vector<std::uint64_t> player_name_addrs(entities_to_update.size(), 0);
	for (size_t i = 0; i < entities_to_update.size(); ++i)
	{
		auto* entity = entities_to_update[i];
		if (entity->instance.address != 0 && entity->instance.address > 0x10000)
		{
			memory.AddScatterReadRequest(scatter_handle, entity->instance.address + Offsets::Player::ModelInstance, &player_model_instances[i], sizeof(std::uint64_t));
			memory.AddScatterReadRequest(scatter_handle, entity->instance.address + Offsets::Player::Team, &player_teams[i], sizeof(std::uint64_t));
			memory.AddScatterReadRequest(scatter_handle, entity->instance.address + Offsets::Player::UserId, &player_userids[i], sizeof(std::uint64_t));
			memory.AddScatterReadRequest(scatter_handle, entity->instance.address + Offsets::Instance::Name, &player_name_addrs[i], sizeof(std::uint64_t));
		}
	}
	memory.ExecuteReadScatter(scatter_handle);
	std::vector<std::int32_t> name_lengths(entities_to_update.size(), 0);
	std::vector<std::uint64_t> name_data_addrs(entities_to_update.size(), 0);
	for (size_t i = 0; i < entities_to_update.size(); ++i)
	{
		if (player_name_addrs[i] != 0 && player_name_addrs[i] > 0x10000)
		{
			memory.Read_string_scatter(scatter_handle, player_name_addrs[i], &name_lengths[i], &name_data_addrs[i]);
		}
	}
	memory.ExecuteReadScatter(scatter_handle);
	std::vector<std::vector<char>> name_buffers(entities_to_update.size());
	std::vector<char*> name_ptrs(entities_to_update.size(), nullptr);
	for (size_t i = 0; i < entities_to_update.size(); ++i)
	{
		if (name_lengths[i] > 0 && name_lengths[i] < 256 && player_name_addrs[i] > 0x10000)
		{
			name_buffers[i].resize(name_lengths[i], 0);
			name_ptrs[i] = name_buffers[i].data();
			std::uint64_t actual_addr = (name_lengths[i] >= 16) ? name_data_addrs[i] : player_name_addrs[i];
			if (actual_addr > 0x10000 && actual_addr < 0x7FFFFFFFFFFF)
			{
				memory.AddScatterReadStringDataRequest(scatter_handle, actual_addr, name_lengths[i], name_ptrs[i]);
			}
		}
	}
	memory.ExecuteReadScatter(scatter_handle);
	for (size_t i = 0; i < entities_to_update.size(); ++i)
	{
		auto* entity = entities_to_update[i];
		entity->team = player_teams[i];
		entity->userid = player_userids[i];
		if (name_ptrs[i] != nullptr && name_lengths[i] > 0 && name_lengths[i] < 256)
		{
			entity->name = std::string(name_ptrs[i], name_lengths[i]);
		}
	}
	for (size_t i = 0; i < entities_to_update.size(); ++i)
	{
		auto* entity = entities_to_update[i];
		if (player_model_instances[i] == 0 || player_model_instances[i] <= 0x10000)
			continue;
		rbx::instance_t model_instance(player_model_instances[i]);
		auto children = model_instance.get_children();
		for (auto& child : children)
		{
			std::string child_name = child.get_name();
			if (child_name == "HumanoidRootPart" || child_name == "Head" || child_name == "Torso" || child_name == "UpperTorso")
			{
				entity->parts[child_name] = rbx::meshpart_t(child.address);
			}
			else if (child_name == "Humanoid")
			{
				entity->humanoid = rbx::humanoid_t(child.address);
				entity->health = memory.Read<float>(child.address + Offsets::Humanoid::Health);
				entity->max_health = memory.Read<float>(child.address + Offsets::Humanoid::MaxHealth);
			}
		}
		if (settings::performance::cache_primitives)
		{
			for (auto& part_pair : entity->parts)
			{
				std::uint64_t prim_addr = memory.Read<std::uint64_t>(part_pair.second.address + Offsets::BasePart::Primitive);
				if (prim_addr != 0 && prim_addr > 0x10000)
				{
					rbx::primitive_t prim(prim_addr);
					auto& prim_data = entity->primitive_cache[part_pair.first];
					prim_data.position = memory.Read<math::vector3>(prim_addr + Offsets::BasePart::Position);
					prim_data.size = memory.Read<math::vector3>(prim_addr + Offsets::BasePart::Size);
					prim_data.rotation = memory.Read<math::matrix3>(prim_addr + Offsets::BasePart::Rotation);
					if (part_pair.first == "HumanoidRootPart")
					{
						entity->root_position = prim_data.position;
					}
				}
			}
		}
	}
}
static void UpdatePositions()
{
	if (!scatter_handle || entity_cache.empty())
		return;
	std::vector<std::uint64_t> prim_addrs;
	std::vector<size_t> entity_indices;
	prim_addrs.reserve(entity_cache.size());
	entity_indices.reserve(entity_cache.size());
	for (size_t i = 0; i < entity_cache.size(); ++i)
	{
		auto& entity = entity_cache[i];
		auto it = entity.parts.find("HumanoidRootPart");
		if (it != entity.parts.end() && it->second.address != 0 && it->second.address > 0x10000)
		{
			prim_addrs.push_back(0);
			entity_indices.push_back(i);
			memory.AddScatterReadRequest(scatter_handle, it->second.address + Offsets::BasePart::Primitive, &prim_addrs.back(), sizeof(std::uint64_t));
		}
	}
	if (prim_addrs.empty())
		return;
	memory.ExecuteReadScatter(scatter_handle);
	std::vector<math::vector3> positions(prim_addrs.size());
	for (size_t i = 0; i < prim_addrs.size(); ++i)
	{
		if (prim_addrs[i] != 0 && prim_addrs[i] > 0x10000)
		{
			memory.AddScatterReadRequest(scatter_handle, prim_addrs[i] + Offsets::BasePart::Position, &positions[i], sizeof(math::vector3));
		}
	}
	memory.ExecuteReadScatter(scatter_handle);
	for (size_t i = 0; i < entity_indices.size(); ++i)
	{
		size_t idx = entity_indices[i];
		if (idx < entity_cache.size())
		{
			entity_cache[idx].root_position = positions[i];
			if (entity_cache[idx].primitive_cache.count("HumanoidRootPart"))
			{
				entity_cache[idx].primitive_cache["HumanoidRootPart"].position = positions[i];
			}
		}
	}
	{
		std::lock_guard<std::mutex> lock(frame_data.mtx);
		for (size_t i = 0; i < entity_cache.size() && i < frame_data.entities.size(); ++i)
		{
			if (entity_cache[i].instance.address == frame_data.entities[i].instance.address)
			{
				frame_data.entities[i].root_position = entity_cache[i].root_position;
				frame_data.entities[i].primitive_cache = entity_cache[i].primitive_cache;
			}
		}
	}
}