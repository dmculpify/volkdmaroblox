/*
 * NO-CACHE MODE: All caching removed. Each cycle does a full fresh read.
 * If this fixes the corpse/ESP bug, add caching back one by one until
 * the bug returns - then remove that last cache.
 *
 * Caches removed:
 * 1. last_model_per_player - was tracking model addr per player
 * 2. entity_cache - was persisting entities between FetchEntities and UpdatePositions
 * 3. cached_prim_addrs - was caching primitive addrs in UpdatePositions
 * 4. Separate FetchEntities (300ms) + UpdatePositions (8ms) - now single FullRefresh each cycle
 */
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
static std::vector<cache::entity_t> entities_scratch;
static void FullRefresh();

void cache::run() {
	sleep(2000);
	scatter_handle = get_process() ? get_process()->create_scatter(0) : nullptr;
	if (!scatter_handle)
	{
		LOG("[!] CRITICAL: Failed to create scatter handle\n");
		return;
	}
	entities_scratch.reserve(64);
	LOG("[+] Cache system starting (no-cache mode)...\n");
	while (true)
	{
		try
		{
			if (RateLimiter::get_full_cache().should_run_ms(50))
			{
				FullRefresh();
			}
			sleep(3);
		}
		catch (...)
		{
			sleep(100);
		}
	}
}

static void clear_frame()
{
	std::lock_guard<std::mutex> lock(cache::frame_data.mtx);
	cache::frame_data.entities.clear();
}

static void FullRefresh()
{
	game::refresh_datamodel();
	if (game::players_service.address == 0 || game::players_service.address <= 0x10000)
	{
		clear_frame();
		return;
	}
	std::vector<rbx::player_t> players;
	try {
		players = game::players_service.get_children<rbx::player_t>();
	}
	catch (...) {
		clear_frame();
		return;
	}
	if (players.empty())
	{
		clear_frame();
		return;
	}
	std::vector<std::uint64_t> model_instances(players.size(), 0);
	std::vector<std::uint64_t> model_parents(players.size(), 0);
	std::uint64_t localplayer_addr = 0;
	if (!scatter_handle)
	{
		clear_frame();
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
		clear_frame();
		return;
	}
	rbx::player_t localplayer = rbx::player_t(localplayer_addr);
	for (size_t i = 0; i < players.size(); ++i)
	{
		if (model_instances[i] > 0x10000)
			get_process()->add_read_scatter(scatter_handle, model_instances[i] + Offsets::Instance::Parent, &model_parents[i], sizeof(std::uint64_t));
	}
	get_process()->execute_scatter(scatter_handle);

	static const std::unordered_set<std::string> essential_parts = {
		"HumanoidRootPart", "Head", "UpperTorso", "LowerTorso", "Torso"
	};
	constexpr std::int16_t HUMANOID_STATE_DEAD = 8;

	entities_scratch.clear();
	if (entities_scratch.capacity() < players.size())
		entities_scratch.reserve(players.size());

	for (size_t i = 0; i < players.size(); ++i)
	{
		std::uint64_t current_model = model_instances[i];
		std::uint64_t current_model_parent = model_parents[i];
		bool valid_model = (current_model > 0x10000 && current_model < 0x7FFFFFFFFFFF &&
		                    current_model_parent > 0x10000 && current_model_parent < 0x7FFFFFFFFFFF);

		entities_scratch.emplace_back();
		cache::entity_t& entity = entities_scratch.back();
		entity.instance = players[i];
		entity.model_instance_addr = valid_model ? current_model : 0;
		entity.team = get_process()->read<std::uint64_t>(players[i].address + Offsets::Player::Team);
		entity.userid = get_process()->read<std::uint64_t>(players[i].address + Offsets::Player::UserId);
		if (settings::visuals::master && settings::visuals::name) {
			try { entity.name = entity.instance.get_name(); } catch (...) { entity.name = "Unknown"; }
		}

		if (!valid_model)
			continue;

		try {
			rbx::instance_t model_instance(current_model);
			auto children = model_instance.get_children();
			bool found_humanoid = false;
			std::vector<std::pair<std::string, std::uint64_t>> parts_to_read;

			for (auto& child : children)
			{
				if (child.address == 0 || child.address <= 0x10000)
					continue;
				try {
					std::string child_name = child.get_name();
					if (essential_parts.count(child_name) > 0)
					{
						parts_to_read.push_back({ child_name, child.address });
						entity.parts[child_name] = rbx::meshpart_t(child.address);
					}
					else if (child_name == "Humanoid")
					{
						found_humanoid = true;
						entity.humanoid = rbx::humanoid_t(child.address);
						try {
							entity.health = get_process()->read<float>(child.address + Offsets::Humanoid::Health);
							entity.max_health = get_process()->read<float>(child.address + Offsets::Humanoid::MaxHealth);
							entity.humanoid_state = get_process()->read<std::int16_t>(child.address + Offsets::Humanoid::HumanoidState);
							if (!std::isfinite(entity.health)) entity.health = 0.0f;
							if (!std::isfinite(entity.max_health)) entity.max_health = 100.0f;
						}
						catch (...) {
							entity.health = 0.0f;
							entity.max_health = 100.0f;
						}
					}
				}
				catch (...) { continue; }
			}

			bool is_dead = !found_humanoid || entity.health <= 0.0f || !std::isfinite(entity.health) || entity.humanoid_state == HUMANOID_STATE_DEAD;
			if (is_dead)
			{
				entity.model_instance_addr = 0;
				entity.parts.clear();
				continue;
			}

			for (auto& p : parts_to_read)
			{
				std::uint64_t prim_addr = 0;
				prim_addr = get_process()->read<std::uint64_t>(p.second + Offsets::BasePart::Primitive);
				if (prim_addr <= 0x10000) continue;
				math::vector3 pos = get_process()->read<math::vector3>(prim_addr + Offsets::BasePart::Position);
				math::vector3 sz = get_process()->read<math::vector3>(prim_addr + Offsets::BasePart::Size);
				if (std::isfinite(pos.x) && std::isfinite(pos.y) && std::isfinite(pos.z))
				{
					auto& prim_data = entity.primitive_cache[p.first];
					prim_data.position = pos;
					prim_data.size = sz;
					prim_data.velocity = math::vector3(0, 0, 0);
					if (p.first == "HumanoidRootPart")
						entity.root_position = pos;
				}
				else
				{
					entity.parts.erase(p.first);
				}
			}

			if (entity.parts.empty() || entity.primitive_cache.empty())
				entity.model_instance_addr = 0;
		}
		catch (...) {
			entity.model_instance_addr = 0;
			entity.parts.clear();
			entity.primitive_cache.clear();
		}
	}

	std::vector<cache::entity_t> alive_only;
	alive_only.reserve(entities_scratch.size());
	for (auto& entity : entities_scratch)
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
		if (entity.humanoid_state == HUMANOID_STATE_DEAD)
			continue;
		if (entity.parts.empty() || entity.primitive_cache.empty())
			continue;
		alive_only.push_back(entity);
	}
	{
		std::lock_guard<std::mutex> lock(cache::frame_data.mtx);
		cache::frame_data.entities = std::move(alive_only);
	}
}
