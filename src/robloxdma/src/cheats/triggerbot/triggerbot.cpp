#include <cheats/triggerbot/triggerbot.h>
#include <cache/cache.h>
#include <game/game.h>
#include <settings/settings.h>
#include <settings/config.h>
#include <dma_helper.h>
#include <globals.h>
#include <makcu/makcu.h>
#include "VolkDMA/inputstate.hh"
#include <imgui/imgui.h>
#include <cmath>
#include <chrono>
#ifdef first
#undef first
#endif
#ifdef dx
#undef dx
#undef dy
#endif

namespace {

    constexpr float TRIGGER_RADIUS_PX = 25.0f;
    constexpr int MIN_TRIGGER_COOLDOWN_MS = 200;

    bool is_crosshair_on_entity(const cache::entity_t& entity, float center_x, float center_y,
        float screen_w, float screen_h, float scale_x, float scale_y, bool head_only) {

        auto check_part = [&](const char* part_name) -> bool {
            auto it = entity.primitive_cache.find(part_name);
            if (it == entity.primitive_cache.end())
                return false;
            math::vector2 screen_pos;
            if (!game::visualengine.world_to_screen(it->second.position, screen_pos))
                return false;
            float sx = screen_pos.x * scale_x;
            float sy = screen_pos.y * scale_y;
            float dx = sx - center_x;
            float dy = sy - center_y;
            return (dx * dx + dy * dy) <= (TRIGGER_RADIUS_PX * TRIGGER_RADIUS_PX);
        };

        if (head_only) {
            return check_part("Head") || check_part("head");
        }
        return check_part("Head") || check_part("head") ||
               check_part("UpperTorso") || check_part("LowerTorso") || check_part("Torso") ||
               check_part("HumanoidRootPart");
    }

} // namespace

void triggerbot::run() {
    static auto last_trigger_time = std::chrono::steady_clock::now();
    static auto pending_trigger_time = std::chrono::steady_clock::time_point{};
    static bool pending_trigger = false;
    if (!settings::triggerbot::enabled)
        return;
    InputState* input = get_input_state();
    if (!input || !input->read_bitmap())
        return;
    if (!input->is_key_down(static_cast<uint8_t>(settings::triggerbot::trigger_key & 0xFF)))
        return;

    std::vector<cache::entity_t> snapshot;
    std::uint64_t local_player_addr = 0;
    std::uint64_t local_team = 0;
    {
        std::lock_guard<std::mutex> lock(cache::frame_data.mtx);
        snapshot = cache::frame_data.entities;
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

    for (const auto& entity : snapshot) {
        if (entity.instance.address == local_player_addr && local_player_addr != 0)
            continue;
        if (config::is_whitelisted(entity.userid))
            continue;
        if (settings::triggerbot::team_check && local_team != 0 && entity.team != 0 && entity.team == local_team)
            continue;
        if (entity.model_instance_addr == 0 || entity.model_instance_addr <= 0x10000)
            continue;
        if (entity.parts.empty() || entity.primitive_cache.empty())
            continue;
        if (!std::isfinite(entity.root_position.x))
            continue;

        if (!is_crosshair_on_entity(entity, center_x, center_y, screen_w, screen_h, scale_x, scale_y, settings::triggerbot::head_only))
            continue;

        auto now = std::chrono::steady_clock::now();
        int cooldown_elapsed = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - last_trigger_time).count();
        if (cooldown_elapsed < MIN_TRIGGER_COOLDOWN_MS)
            return;

        if (settings::triggerbot::delay_ms > 0) {
            if (!pending_trigger) {
                pending_trigger = true;
                pending_trigger_time = now + std::chrono::milliseconds((int)settings::triggerbot::delay_ms);
                return;
            }
            if (now < pending_trigger_time)
                return;
        }
        pending_trigger = false;

        makcu::click(0);
        last_trigger_time = now;
        return;
    }
    pending_trigger = false;
}
