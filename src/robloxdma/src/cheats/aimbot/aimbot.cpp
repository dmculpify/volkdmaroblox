#include <cheats/aimbot/aimbot.h>
#include <cache/cache.h>
#include <game/game.h>
#include <settings/settings.h>
#include <settings/config.h>
#include <dma_helper.h>
#include <globals.h>
#include "VolkDMA/inputstate.hh"
#include <imgui/imgui.h>
#include <cmath>
#include <algorithm>
#include <string>
#ifdef first
#undef first
#endif
#ifdef dx
#undef dx
#undef dy
#endif

namespace {

    const char* get_aim_bone_name() {
        switch (settings::aimbot::aim_body_part) {
            case 0: return "Head";
            case 1: return "UpperTorso";
            case 2: return "HumanoidRootPart";
            default: return "Head";
        }
    }

    const char* get_aim_bone_fallback_r6() {
        switch (settings::aimbot::aim_body_part) {
            case 0: return "Head";
            case 1: return "Torso";
            case 2: return "HumanoidRootPart";
            default: return "Head";
        }
    }

    bool get_aim_position(const cache::entity_t& entity, math::vector3& out_pos, math::vector3* out_vel) {
        const char* bone = get_aim_bone_name();
        const char* fallback = get_aim_bone_fallback_r6();
        auto it = entity.primitive_cache.find(bone);
        if (it == entity.primitive_cache.end())
            it = entity.primitive_cache.find(fallback);
        if (it == entity.primitive_cache.end())
            it = entity.primitive_cache.find("HumanoidRootPart");
        if (it == entity.primitive_cache.end())
            return false;
        out_pos = it->second.position;
        const std::string& part_name = it->first;
        if (out_vel && (part_name == "HumanoidRootPart" || part_name == "Head"))
            *out_vel = it->second.velocity;
        else if (out_vel)
            *out_vel = math::vector3(0, 0, 0);
        return true;
    }

    float fov_radius_pixels(float fov_deg, float screen_w, float screen_h) {
        float min_dim = (std::min)(screen_w, screen_h);
        return (fov_deg / 180.f) * min_dim * 0.5f;
    }
}

void aimbot::run() {
    if (!settings::aimbot::enabled)
        return;
    InputState* input = get_input_state();
    if (!input || !input->read_bitmap())
        return;
    if (!input->is_key_down(static_cast<uint8_t>(settings::aimbot::aim_key & 0xFF)))
        return;

    std::vector<cache::entity_t> snapshot;
    math::vector3 local_position;
    std::uint64_t local_player_addr = 0;
    std::uint64_t local_team = 0;
    {
        std::lock_guard<std::mutex> lock(cache::frame_data.mtx);
        snapshot = cache::frame_data.entities;
        local_position = cache::frame_data.local_entity.root_position;
        local_player_addr = cache::frame_data.local_entity.instance.address;
        local_team = cache::frame_data.local_entity.team;
    }
    if (snapshot.empty())
        return;

    ImVec2 display = ImGui::GetIO().DisplaySize;
    float screen_w = display.x;
    float screen_h = display.y;
    float scale_x = 1.0f, scale_y = 1.0f;
    math::vector2 game_dims = game::visualengine.get_dimensions();
    if (game_dims.x > 0 && game_dims.y > 0) {
        scale_x = screen_w / game_dims.x;
        scale_y = screen_h / game_dims.y;
    }

    float center_x = screen_w * 0.5f;
    float center_y = screen_h * 0.5f;
    float fov_radius = fov_radius_pixels(settings::aimbot::fov, screen_w, screen_h);

    struct candidate_t {
        float screen_x, screen_y;
        float dist_to_crosshair;
        float dist_3d;
        size_t entity_idx;
    };
    std::vector<candidate_t> candidates;

    for (size_t i = 0; i < snapshot.size(); ++i) {
        const cache::entity_t& entity = snapshot[i];
        if (entity.instance.address == local_player_addr && local_player_addr != 0)
            continue;
        if (config::is_whitelisted(entity.userid))
            continue;
        if (settings::aimbot::team_check && local_team != 0 && entity.team != 0 && entity.team == local_team)
            continue;
        if (entity.model_instance_addr == 0 || entity.model_instance_addr <= 0x10000)
            continue;
        if (!std::isfinite(entity.root_position.x))
            continue;

        math::vector3 aim_pos;
        math::vector3 aim_vel;
        if (!get_aim_position(entity, aim_pos, &aim_vel))
            continue;

        if (settings::aimbot::prediction && std::isfinite(aim_vel.x)) {
            float pred = settings::aimbot::prediction_strength * 0.05f;
            aim_pos.x += aim_vel.x * pred;
            aim_pos.y += aim_vel.y * pred;
            aim_pos.z += aim_vel.z * pred;
        }

        math::vector2 screen_pos;
        if (!game::visualengine.world_to_screen(aim_pos, screen_pos))
            continue;
        float sx = screen_pos.x * scale_x;
        float sy = screen_pos.y * scale_y;
        if (sx < 0 || sx > screen_w || sy < 0 || sy > screen_h)
            continue;

        float dx = sx - center_x;
        float dy = sy - center_y;
        float dist_to_crosshair = std::sqrt(dx * dx + dy * dy);
        if (dist_to_crosshair > fov_radius)
            continue;

        float dist_3d = 0.f;
        if (std::isfinite(local_position.x))
            dist_3d = (entity.root_position - local_position).length();

        candidates.push_back({ sx, sy, dist_to_crosshair, dist_3d, i });
    }

    if (candidates.empty())
        return;

    if (settings::aimbot::target_priority == 0)
        std::sort(candidates.begin(), candidates.end(), [](const candidate_t& a, const candidate_t& b) { return a.dist_to_crosshair < b.dist_to_crosshair; });
    else
        std::sort(candidates.begin(), candidates.end(), [](const candidate_t& a, const candidate_t& b) { return a.dist_3d < b.dist_3d; });

    const candidate_t& best = candidates[0];
    float smooth = (std::max)(1.0f, settings::aimbot::smoothness);
    float aim_x = center_x + (best.screen_x - center_x) / smooth;
    float aim_y = center_y + (best.screen_y - center_y) / smooth;

    float dist = std::sqrt((aim_x - center_x) * (aim_x - center_x) + (aim_y - center_y) * (aim_y - center_y));
    if (dist < 0.5f)
        return;

    INPUT input_event = {};
    input_event.type = INPUT_MOUSE;
    input_event.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    input_event.mi.dx = (LONG)(aim_x * 65536.0f / screen_w);
    input_event.mi.dy = (LONG)(aim_y * 65536.0f / screen_h);
    SendInput(1, &input_event, sizeof(INPUT));
}

void aimbot::draw_fov_circle() {
    if (!settings::aimbot::draw_fov)
        return;
    ImVec2 display = ImGui::GetIO().DisplaySize;
    float cx = display.x * 0.5f;
    float cy = display.y * 0.5f;
    float radius = fov_radius_pixels(settings::aimbot::fov, display.x, display.y);
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    draw->AddCircle(ImVec2(cx, cy), radius, IM_COL32(255, 255, 255, 180), 64, 1.5f);
    draw->AddLine(ImVec2(cx - 8, cy), ImVec2(cx + 8, cy), IM_COL32(255, 255, 255, 200), 1.5f);
    draw->AddLine(ImVec2(cx, cy - 8), ImVec2(cx, cy + 8), IM_COL32(255, 255, 255, 200), 1.5f);
}
