#pragma once
#include <imgui/imgui.h>

namespace settings
{
	namespace visuals
	{
		inline bool master = true;
		inline bool box = true;
		inline bool filled_box = false;
		inline bool name = false;
		inline bool distance = false;
		inline bool status = false;
		inline bool healthbar = false;
		inline bool armor = false;
		inline bool tool = false;
		inline bool team_check = false;
		inline float max_render_distance = 0.0f;
		inline bool chams = false;
		inline bool chams_outline = false;
		inline bool skeleton = false;
		inline bool china_hat = false;
		inline bool tracers = false;

		namespace colors
		{
			inline ImVec4 box_color = ImVec4(1, 1, 1, 1);
			inline ImVec4 box_fill_color = ImVec4(1, 1, 1, 0.2);
			inline ImVec4 names_color = ImVec4(1, 1, 1, 1);
			inline ImVec4 distance_color = ImVec4(1, 1, 1, 1);
			inline ImVec4 status_color = ImVec4(1, 1, 1, 1);
			inline ImVec4 tool_color = ImVec4(1, 1, 1, 1);
			inline ImVec4 healthbar_color = ImVec4(0, 1, 0, 1);
			inline ImVec4 armourbar_color = ImVec4(0, 0, 1, 1);
			inline ImVec4 chams_color = ImVec4(1, 0, 0, 0.5f);
			inline ImVec4 chams_outline_color = ImVec4(1, 1, 1, 1);
			inline ImVec4 skeleton_color = ImVec4(1, 1, 1, 1);
		}
	}

	namespace performance
	{
		inline int cache_ms = 250;
		inline int position_update_ms = 6;
		inline bool low_end_mode = false;
		inline bool cache_primitives = true;
		inline bool black_background = true;
		inline int resolution_width = 2560;
		inline int resolution_height = 1440;
		inline bool resolution_changed = false;
		inline int fps_cap = 0;
		inline bool enable_fps_cap = false;
		inline bool force_refresh = false;
		inline int entity_batch_size = 32;
		inline float lod_distance_near = 100.0f;
		inline float lod_distance_mid = 250.0f;
		inline float lod_distance_far = 500.0f;
		inline float lod_distance_cull = 2000.0f;
		inline bool batch_rendering = true;
		inline bool distance_culling = false;
	}

	namespace aimbot
	{
		inline bool enabled = false;
		inline bool team_check = true;
		inline bool aim_at_head = true;
		inline bool draw_fov = true;
		inline int aim_key = 0x02; // Right mouse button
		inline int target_priority = 0; // 0 = closest to crosshair, 1 = closest distance
		inline float fov = 100.0f;
		inline float smoothness = 1.0f;
		inline bool prediction = false;
		inline float prediction_strength = 1.0f;
	}

	namespace triggerbot
	{
		inline bool enabled = false;
		inline bool team_check = true;
		inline bool head_only = false;
		inline int trigger_key = 0x01; // Left mouse button
		inline float delay_ms = 0.0f;
	}
}
