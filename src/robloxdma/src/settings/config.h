#pragma once
#include <unordered_set>
#include <fstream>
#include <string>
#include <algorithm>
#include <cstring>
#include "settings.h"

namespace config
{
	inline std::unordered_set<std::uint64_t> whitelisted_players;
	inline const char* config_file = "robloxdma_config.ini";

	inline void save()
	{
		std::ofstream file(config_file);
		if (!file.is_open()) return;

		file << "[Visuals]\n";
		file << "master=" << settings::visuals::master << "\n";
		file << "box=" << settings::visuals::box << "\n";
		file << "filled_box=" << settings::visuals::filled_box << "\n";
		file << "name=" << settings::visuals::name << "\n";
		file << "distance=" << settings::visuals::distance << "\n";
		file << "status=" << settings::visuals::status << "\n";
		file << "healthbar=" << settings::visuals::healthbar << "\n";
		file << "team_check=" << settings::visuals::team_check << "\n";
		file << "max_render_distance=" << settings::visuals::max_render_distance << "\n";

		file << "\n[Performance]\n";
		file << "cache_ms=" << settings::performance::cache_ms << "\n";
		file << "position_update_ms=" << settings::performance::position_update_ms << "\n";
		file << "recache_dead_check_seconds=" << settings::performance::recache_dead_check_seconds << "\n";
		file << "low_end_mode=" << settings::performance::low_end_mode << "\n";

		file << "\n[Aimbot]\n";
		file << "enabled=" << (settings::aimbot::enabled ? "1" : "0") << "\n";
		file << "team_check=" << (settings::aimbot::team_check ? "1" : "0") << "\n";
		file << "draw_fov=" << (settings::aimbot::draw_fov ? "1" : "0") << "\n";
		file << "aim_key=" << settings::aimbot::aim_key << "\n";
		file << "aim_body_part=" << settings::aimbot::aim_body_part << "\n";
		file << "target_priority=" << settings::aimbot::target_priority << "\n";
		file << "fov=" << settings::aimbot::fov << "\n";
		file << "smoothness=" << settings::aimbot::smoothness << "\n";
		file << "prediction=" << (settings::aimbot::prediction ? "1" : "0") << "\n";
		file << "prediction_strength=" << settings::aimbot::prediction_strength << "\n";
		file << "com_port=" << settings::aimbot::com_port << "\n";

		file << "\n[Triggerbot]\n";
		file << "enabled=" << (settings::triggerbot::enabled ? "1" : "0") << "\n";
		file << "trigger_key=" << settings::triggerbot::trigger_key << "\n";

		file << "\n[Game]\n";
		file << "force_rivals=" << (settings::game::force_rivals ? "1" : "0") << "\n";

		file << "\n[Whitelist]\n";
		for (std::uint64_t userid : whitelisted_players)
			file << "player=" << userid << "\n";
	}

	inline void load()
	{
		std::ifstream file(config_file);
		if (!file.is_open()) return;

		std::string line;
		std::string current_section;

		while (std::getline(file, line))
		{
			if (line.empty()) continue;
			if (line[0] == '[')
			{
				current_section = line;
				continue;
			}

			size_t eq_pos = line.find('=');
			if (eq_pos == std::string::npos) continue;

			std::string key = line.substr(0, eq_pos);
			std::string value = line.substr(eq_pos + 1);

			if (current_section == "[Visuals]")
			{
				if (key == "master") settings::visuals::master = (value == "1");
				else if (key == "box") settings::visuals::box = (value == "1");
				else if (key == "filled_box") settings::visuals::filled_box = (value == "1");
				else if (key == "name") settings::visuals::name = (value == "1");
				else if (key == "distance") settings::visuals::distance = (value == "1");
				else if (key == "status") settings::visuals::status = (value == "1");
				else if (key == "healthbar") settings::visuals::healthbar = (value == "1");
				else if (key == "team_check") settings::visuals::team_check = (value == "1");
				else if (key == "max_render_distance") settings::visuals::max_render_distance = std::stof(value);
			}
			else if (current_section == "[Performance]")
			{
				if (key == "cache_ms") settings::performance::cache_ms = std::stoi(value);
				else if (key == "position_update_ms") settings::performance::position_update_ms = std::stoi(value);
				else if (key == "recache_dead_check_seconds") {
					int v = std::stoi(value);
					settings::performance::recache_dead_check_seconds = (v < 1) ? 1 : ((v > 30) ? 30 : v);
				}
				else if (key == "low_end_mode") settings::performance::low_end_mode = (value == "1");
			}
			else if (current_section == "[Aimbot]")
			{
				if (key == "enabled") settings::aimbot::enabled = (value == "1");
				else if (key == "team_check") settings::aimbot::team_check = (value == "1");
				else if (key == "draw_fov") settings::aimbot::draw_fov = (value == "1");
				else if (key == "aim_key") settings::aimbot::aim_key = std::stoi(value);
				else if (key == "aim_body_part") settings::aimbot::aim_body_part = std::stoi(value);
				else if (key == "target_priority") settings::aimbot::target_priority = std::stoi(value);
				else if (key == "fov") settings::aimbot::fov = std::stof(value);
				else if (key == "smoothness") settings::aimbot::smoothness = std::stof(value);
				else if (key == "prediction") settings::aimbot::prediction = (value == "1");
				else if (key == "prediction_strength") settings::aimbot::prediction_strength = std::stof(value);
				else if (key == "com_port") {
					size_t copy_len = (std::min)(value.size(), sizeof(settings::aimbot::com_port) - 1);
					std::memcpy(settings::aimbot::com_port, value.c_str(), copy_len);
					settings::aimbot::com_port[copy_len] = '\0';
				}
			}
			else if (current_section == "[Triggerbot]")
			{
				if (key == "enabled") settings::triggerbot::enabled = (value == "1");
				else if (key == "trigger_key") settings::triggerbot::trigger_key = std::stoi(value);
			}
			else if (current_section == "[Game]")
			{
				if (key == "force_rivals") settings::game::force_rivals = (value == "1");
			}
			else if (current_section == "[Whitelist]")
			{
				if (key == "player") whitelisted_players.insert(std::stoull(value));
			}
		}
	}

	inline bool is_whitelisted(std::uint64_t userid) { return whitelisted_players.count(userid) > 0; }
	inline void add_to_whitelist(std::uint64_t userid) { whitelisted_players.insert(userid); save(); }
	inline void remove_from_whitelist(std::uint64_t userid) { whitelisted_players.erase(userid); save(); }
}
