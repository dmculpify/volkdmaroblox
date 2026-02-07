#include <cheats/esp/esp.h>
#include <globals.h>
#include <settings/settings.h>
#include <settings/config.h>
#include <cache/cache.h>
#include <game/game.h>
#include <cheats/esp/helpers/visualize.h>
#include <clipper2/clipper.h>
#include <clipper2/clipper.core.h>
#include <sdk/offsets/offsets.h>
#include <unordered_map>
#include <algorithm>
void esp::run()
{
    if (!settings::visuals::master)
    {
        return;
    }
    if (settings::performance::black_background)
    {
        ImDrawList* bg_draw = ImGui::GetBackgroundDrawList();
        bg_draw->AddRectFilled(
            ImVec2(0, 0),
            ImGui::GetIO().DisplaySize,
            IM_COL32(0, 0, 0, 255)
        );
    }
    static math::vector3 local_corners[8] =
    {
        { -1, -1, -1 }, { 1, -1, -1 }, { -1, 1, -1 }, { 1, 1,-1 }, { -1, -1, 1 }, { 1, -1, 1 }, { -1, 1, 1 }, { 1, 1, 1 }
    };
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
    {
        return;
    }
    bool has_local_position = std::isfinite(local_position.x);
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    math::vector2 game_dims = game::visualengine.get_dimensions();
    if (game_dims.x > 0 && game_dims.y > 0)
    {
        ImVec2 overlay_size = ImGui::GetIO().DisplaySize;
        scale_x = overlay_size.x / game_dims.x;
        scale_y = overlay_size.y / game_dims.y;
    }
    const bool need_boxes = settings::visuals::box || settings::visuals::filled_box;
    const bool need_chams = settings::visuals::chams;
    const bool need_skeleton = settings::visuals::skeleton;
    const bool need_any_visual = need_boxes || need_chams || settings::visuals::china_hat;
    const size_t player_count = snapshot.size();
    const bool low_player_count = player_count <= 10;
    const bool medium_player_count = player_count > 10 && player_count <= 25;
    const bool high_player_count = player_count > 25 && player_count <= 50;
    const bool very_high_player_count = player_count > 50;
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    if (settings::performance::low_end_mode || very_high_player_count)
    {
        draw->Flags &= ~ImDrawListFlags_AntiAliasedLines;
        draw->Flags &= ~ImDrawListFlags_AntiAliasedFill;
    }
    else
    {
        draw->Flags |= ImDrawListFlags_AntiAliasedLines;
    }
    static size_t frame_counter = 0;
    frame_counter++;
    const bool even_frame = (frame_counter % 2) == 0;
    float lod_distance_near = settings::performance::lod_distance_near;
    float lod_distance_mid = settings::performance::lod_distance_mid;
    float lod_distance_far = settings::performance::lod_distance_far;
    float lod_distance_cull = settings::performance::lod_distance_cull;
    if (very_high_player_count)
    {
        lod_distance_near *= 0.5f;
        lod_distance_mid *= 0.6f;
        lod_distance_far *= 0.6f;
        lod_distance_cull *= 0.75f;
    }
    else if (high_player_count)
    {
        lod_distance_near *= 0.75f;
        lod_distance_mid *= 0.8f;
        lod_distance_far *= 0.8f;
        lod_distance_cull *= 0.9f;
    }
    for (size_t entity_idx = 0; entity_idx < snapshot.size(); entity_idx++)
    {
        cache::entity_t& entity = snapshot[entity_idx];
        if (entity.instance.address == local_player_addr && local_player_addr != 0)
        {
            continue;
        }
        if (config::is_whitelisted(entity.userid))
        {
            continue;
        }
        if (entity.model_instance_addr == 0 ||
            entity.model_instance_addr <= 0x10000 ||
            entity.model_instance_addr >= 0x7FFFFFFFFFFF ||
            entity.parts.empty() ||
            entity.primitive_cache.empty() ||
            !std::isfinite(entity.root_position.x) ||
            !std::isfinite(entity.root_position.y) ||
            !std::isfinite(entity.root_position.z))
        {
            continue;
        }
        float pos_magnitude = std::sqrt(
            entity.root_position.x * entity.root_position.x +
            entity.root_position.y * entity.root_position.y +
            entity.root_position.z * entity.root_position.z
        );
        if (pos_magnitude > 100000.0f)
        {
            continue;
        }
        if (settings::visuals::team_check && local_team != 0 && entity.team != 0)
        {
            if (entity.team == local_team)
            {
                continue;
            }
        }
        float distance = 0.0f;
        int lod_level = 0;
        if (has_local_position)
        {
            float dx = entity.root_position.x - local_position.x;
            float dy = entity.root_position.y - local_position.y;
            float dz = entity.root_position.z - local_position.z;
            distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (settings::performance::distance_culling && distance > lod_distance_cull)
            {
                continue;
            }
            if (settings::visuals::max_render_distance > 0.0f && distance > settings::visuals::max_render_distance)
            {
                continue;
            }
            if (distance > lod_distance_far) lod_level = 3;
            else if (distance > lod_distance_mid) lod_level = 2;
            else if (distance > lod_distance_near) lod_level = 1;
            else lod_level = 0;
            if (high_player_count) lod_level = (std::min)(3, lod_level + 1);
            if (very_high_player_count) lod_level = 3;
        }
        if (!need_any_visual && !settings::visuals::name && !settings::visuals::healthbar &&
            !settings::visuals::distance && !settings::visuals::status && !settings::visuals::tool)
        {
            continue;
        }
        bool valid = false;
        float left = FLT_MAX, top = FLT_MAX;
        float right = -FLT_MAX, bottom = -FLT_MAX;
        std::vector<std::pair<std::string, std::vector<ImVec2>>> part_hulls;
        if (settings::visuals::chams && !entity.primitive_cache.empty() && lod_level < 2 && !very_high_player_count)
        {
            for (const auto& prim_pair : entity.primitive_cache)
            {
                const std::string& part_name = prim_pair.first;
                const auto& prim_data = prim_pair.second;
                if (part_name == "HumanoidRootPart")
                    continue;
                math::vector3 size = prim_data.size;
                math::vector3 pos = prim_data.position;
                std::vector<ImVec2> projected;
                projected.reserve(8);
                for (int i = 0; i < 8; ++i)
                {
                    math::vector3 corner = pos + math::vector3(
                        local_corners[i].x * size.x * 0.5f,
                        local_corners[i].y * size.y * 0.5f,
                        local_corners[i].z * size.z * 0.5f
                    );
                    math::vector2 screen;
                    if (game::visualengine.world_to_screen(corner, screen))
                    {
                        projected.emplace_back(screen.x * scale_x, screen.y * scale_y);
                    }
                }
                if (projected.size() >= 3)
                {
                    part_hulls.emplace_back(part_name, std::move(projected));
                }
            }
        }
        if (need_any_visual)
        {
            bool has_head = entity.primitive_cache.count("Head") > 0;
            bool has_root = entity.primitive_cache.count("HumanoidRootPart") > 0;
            bool has_feet = false;
            math::vector3 feet_pos;
            if (entity.primitive_cache.count("LeftFoot") > 0 && entity.primitive_cache.count("RightFoot") > 0)
            {
                math::vector3 left_foot = entity.primitive_cache.at("LeftFoot").position;
                math::vector3 right_foot = entity.primitive_cache.at("RightFoot").position;
                feet_pos = math::vector3(
                    (left_foot.x + right_foot.x) * 0.5f,
                    (std::min)(left_foot.y, right_foot.y) - 0.5f,
                    (left_foot.z + right_foot.z) * 0.5f
                );
                has_feet = true;
            }
            else if (entity.primitive_cache.count("Left Leg") > 0 && entity.primitive_cache.count("Right Leg") > 0)
            {
                math::vector3 left_leg = entity.primitive_cache.at("Left Leg").position;
                math::vector3 right_leg = entity.primitive_cache.at("Right Leg").position;
                feet_pos = math::vector3(
                    (left_leg.x + right_leg.x) * 0.5f,
                    (std::min)(left_leg.y, right_leg.y) - 1.5f,
                    (left_leg.z + right_leg.z) * 0.5f
                );
                has_feet = true;
            }
            if (has_head && has_root)
            {
                math::vector3 head_pos = entity.primitive_cache.at("Head").position;
                math::vector3 root_pos = entity.primitive_cache.at("HumanoidRootPart").position;
                math::vector3 bottom_pos = has_feet ? feet_pos : (root_pos - math::vector3(0, 2.5f, 0));
                float head_height = entity.primitive_cache.at("Head").size.y * 0.5f;
                math::vector3 top_pos = head_pos + math::vector3(0, head_height + 0.2f, 0);
                math::vector2 top_screen, bottom_screen;
                if (game::visualengine.world_to_screen(top_pos, top_screen) &&
                    game::visualengine.world_to_screen(bottom_pos, bottom_screen))
                {
                    float height = bottom_screen.y - top_screen.y;
                    if (height < 10.0f || height > 1000.0f)
                    {
                        continue;
                    }
                    math::vector3 left_shoulder = head_pos + math::vector3(-1.25f, -0.5f, 0);
                    math::vector3 right_shoulder = head_pos + math::vector3(1.25f, -0.5f, 0);
                    math::vector2 left_screen, right_screen;
                    if (game::visualengine.world_to_screen(left_shoulder, left_screen) &&
                        game::visualengine.world_to_screen(right_shoulder, right_screen))
                    {
                        float width = std::abs(right_screen.x - left_screen.x);
                        float center_x = top_screen.x;
                        left = (center_x - width * 0.5f) * scale_x;
                        right = (center_x + width * 0.5f) * scale_x;
                        top = top_screen.y * scale_y;
                        bottom = bottom_screen.y * scale_y;
                        valid = true;
                    }
                    else
                    {
                        float width = height * 0.45f;
                        float center_x = top_screen.x;
                        left = (center_x - width * 0.5f) * scale_x;
                        right = (center_x + width * 0.5f) * scale_x;
                        top = top_screen.y * scale_y;
                        bottom = bottom_screen.y * scale_y;
                        valid = true;
                    }
                }
            }
            else if (has_root)
            {
                math::vector3 root_pos = entity.primitive_cache.at("HumanoidRootPart").position;
                float char_height = 5.5f;
                float char_half_height = char_height * 0.5f;
                math::vector2 top_screen, bottom_screen;
                math::vector3 top_pos = root_pos + math::vector3(0, char_half_height, 0);
                math::vector3 bottom_pos = root_pos - math::vector3(0, char_half_height, 0);
                if (game::visualengine.world_to_screen(top_pos, top_screen) &&
                    game::visualengine.world_to_screen(bottom_pos, bottom_screen))
                {
                    float height = bottom_screen.y - top_screen.y;
                    if (height < 10.0f || height > 1000.0f)
                    {
                        continue;
                    }
                    float width = height * 0.45f;
                    float center_x = top_screen.x;
                    left = (center_x - width * 0.5f) * scale_x;
                    right = (center_x + width * 0.5f) * scale_x;
                    top = top_screen.y * scale_y;
                    bottom = bottom_screen.y * scale_y;
                    valid = true;
                }
            }
            else if (std::isfinite(entity.root_position.x))
            {
                math::vector3 root_pos = entity.root_position;
                float char_height = 5.5f;
                float char_half_height = char_height * 0.5f;
                math::vector2 top_screen, bottom_screen;
                math::vector3 top_pos = root_pos + math::vector3(0, char_half_height, 0);
                math::vector3 bottom_pos = root_pos - math::vector3(0, char_half_height, 0);
                if (game::visualengine.world_to_screen(top_pos, top_screen) &&
                    game::visualengine.world_to_screen(bottom_pos, bottom_screen))
                {
                    float height = bottom_screen.y - top_screen.y;
                    if (height >= 10.0f && height <= 1000.0f)
                    {
                        float width = height * 0.45f;
                        float center_x = top_screen.x;
                        left = (center_x - width * 0.5f) * scale_x;
                        right = (center_x + width * 0.5f) * scale_x;
                        top = top_screen.y * scale_y;
                        bottom = bottom_screen.y * scale_y;
                        valid = true;
                    }
                }
            }
        }
        if (!valid || left >= right || top >= bottom)
        {
            continue;
        }
        float box_width = right - left;
        float box_height = bottom - top;
        if (box_width < 5.0f || box_width > 500.0f ||
            box_height < 10.0f || box_height > 1000.0f)
        {
            continue;
        }
        ImVec2 screen_size = ImGui::GetIO().DisplaySize;
        if (right < 0 || left > screen_size.x || bottom < 0 || top > screen_size.y)
        {
            continue;
        }
        ImVec2 c1(left, top);
        ImVec2 c2(right - left, bottom - top);
        if (settings::visuals::filled_box)
        {
            ImVec4 fill_color = settings::visuals::colors::box_fill_color;
            ImU32 fill_col32 = IM_COL32(
                (int)(fill_color.x * 255),
                (int)(fill_color.y * 255),
                (int)(fill_color.z * 255),
                (int)(fill_color.w * 255)
            );
            visualize::filled(c1, c2, fill_col32);
        }
        if (settings::visuals::box)
        {
            ImVec4 box_color = settings::visuals::colors::box_color;
            ImU32 box_col32 = IM_COL32(
                (int)(box_color.x * 255),
                (int)(box_color.y * 255),
                (int)(box_color.z * 255),
                (int)(box_color.w * 255)
            );
            visualize::outline(c1, c2, box_col32);
        }
        if (need_skeleton && !entity.primitive_cache.empty() && lod_level < 3)
        {
            ImVec4 color = settings::visuals::colors::skeleton_color;
            ImU32 col32 = IM_COL32(
                (int)(color.x * 255),
                (int)(color.y * 255),
                (int)(color.z * 255),
                (int)(color.w * 255)
            );
            float line_thickness = (lod_level == 0) ? 2.0f : 1.5f;
            auto draw_bone = [&](const std::string& part1, const std::string& part2) -> bool {
                auto it1 = entity.primitive_cache.find(part1);
                auto it2 = entity.primitive_cache.find(part2);
                if (it1 == entity.primitive_cache.end() || it2 == entity.primitive_cache.end())
                    return false;
                const auto& pos1 = it1->second.position;
                const auto& pos2 = it2->second.position;
                if (!std::isfinite(pos1.x) || !std::isfinite(pos1.y) || !std::isfinite(pos1.z) ||
                    !std::isfinite(pos2.x) || !std::isfinite(pos2.y) || !std::isfinite(pos2.z))
                    return false;
                float dx = pos2.x - pos1.x;
                float dy = pos2.y - pos1.y;
                float dz = pos2.z - pos1.z;
                float bone_distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (bone_distance > 15.0f || bone_distance < 0.001f)
                    return false;
                math::vector2 screen1, screen2;
                if (!game::visualengine.world_to_screen(pos1, screen1) ||
                    !game::visualengine.world_to_screen(pos2, screen2))
                    return false;
                if (!std::isfinite(screen1.x) || !std::isfinite(screen1.y) ||
                    !std::isfinite(screen2.x) || !std::isfinite(screen2.y))
                    return false;
                float sx1 = screen1.x * scale_x;
                float sy1 = screen1.y * scale_y;
                float sx2 = screen2.x * scale_x;
                float sy2 = screen2.y * scale_y;
                float screen_dx = sx2 - sx1;
                float screen_dy = sy2 - sy1;
                float screen_dist = std::sqrt(screen_dx * screen_dx + screen_dy * screen_dy);
                if (screen_dist > 500.0f)
                    return false;
                draw->AddLine(
                    ImVec2(sx1, sy1),
                    ImVec2(sx2, sy2),
                    col32,
                    line_thickness
                );
                return true;
            };
            bool has_r15_parts = entity.primitive_cache.count("UpperTorso") > 0 ||
                                 entity.primitive_cache.count("LowerTorso") > 0;
            bool has_r6_parts = entity.primitive_cache.count("Torso") > 0;
            if (has_r15_parts)
            {
                draw_bone("LowerTorso", "UpperTorso");
                draw_bone("UpperTorso", "Head");
                if (lod_level == 0 && !very_high_player_count)
                {
                    draw_bone("UpperTorso", "LeftUpperArm");
                    draw_bone("LeftUpperArm", "LeftLowerArm");
                    draw_bone("LeftLowerArm", "LeftHand");
                    draw_bone("UpperTorso", "RightUpperArm");
                    draw_bone("RightUpperArm", "RightLowerArm");
                    draw_bone("RightLowerArm", "RightHand");
                }
                else if (lod_level <= 1 && !very_high_player_count)
                {
                    if (draw_bone("UpperTorso", "LeftUpperArm"))
                        draw_bone("LeftUpperArm", "LeftHand");
                    if (draw_bone("UpperTorso", "RightUpperArm"))
                        draw_bone("RightUpperArm", "RightHand");
                }
                if (lod_level == 0 && !very_high_player_count)
                {
                    draw_bone("LowerTorso", "LeftUpperLeg");
                    draw_bone("LeftUpperLeg", "LeftLowerLeg");
                    draw_bone("LeftLowerLeg", "LeftFoot");
                    draw_bone("LowerTorso", "RightUpperLeg");
                    draw_bone("RightUpperLeg", "RightLowerLeg");
                    draw_bone("RightLowerLeg", "RightFoot");
                }
                else if (lod_level <= 1 && !very_high_player_count)
                {
                    if (draw_bone("LowerTorso", "LeftUpperLeg"))
                        draw_bone("LeftUpperLeg", "LeftFoot");
                    if (draw_bone("LowerTorso", "RightUpperLeg"))
                        draw_bone("RightUpperLeg", "RightFoot");
                }
            }
            else if (has_r6_parts)
            {
                draw_bone("Torso", "Head");
                if (!very_high_player_count)
                {
                    draw_bone("Torso", "Left Arm");
                    draw_bone("Torso", "Right Arm");
                    draw_bone("Torso", "Left Leg");
                    draw_bone("Torso", "Right Leg");
                }
            }
        }
        if (settings::visuals::chams && !part_hulls.empty() && lod_level < 2 && !very_high_player_count)
        {
            draw->Flags &= ImDrawListFlags_AntiAliasedLines;
            Clipper2Lib::Paths64 all_parts;
            all_parts.reserve(part_hulls.size());
            for (std::pair<std::string, std::vector<ImVec2>>& hull_pair : part_hulls)
            {
                if (hull_pair.first == "HumanoidRootPart")
                {
                    continue;
                }
                std::vector<ImVec2>& projected = hull_pair.second;
                std::sort(projected.begin(), projected.end(), [](const ImVec2& a, const ImVec2& b) {
                    return a.x < b.x || (a.x == b.x && a.y < b.y);
                    });
                std::vector<ImVec2> hull;
                hull.reserve(projected.size());
                for (ImVec2& p : projected)
                {
                    while (hull.size() >= 2)
                    {
                        ImVec2& O = hull[hull.size() - 2];
                        ImVec2& A = hull[hull.size() - 1];
                        float cross_val = (A.x - O.x) * (p.y - O.y) - (A.y - O.y) * (p.x - O.x);
                        if (cross_val > 0)
                        {
                            break;
                        }
                        hull.pop_back();
                    }
                    hull.push_back(p);
                }
                size_t t = hull.size() + 1;
                for (int i = (int)projected.size() - 1; i >= 0; --i)
                {
                    ImVec2& p = projected[i];
                    while (hull.size() >= t)
                    {
                        ImVec2& O = hull[hull.size() - 2];
                        ImVec2& A = hull[hull.size() - 1];
                        float cross_val = (A.x - O.x) * (p.y - O.y) - (A.y - O.y) * (p.x - O.x);
                        if (cross_val > 0)
                        {
                            break;
                        }
                        hull.pop_back();
                    }
                    hull.push_back(p);
                }
                hull.pop_back();
                if (hull.size() >= 3)
                {
                    Clipper2Lib::Path64 path;
                    path.reserve(hull.size());
                    for (ImVec2& pt : hull)
                    {
                        path.push_back({ static_cast<int64_t>(pt.x * 1000.0), static_cast<int64_t>(pt.y * 1000.0) });
                    }
                    all_parts.push_back(path);
                }
            }
            if (!all_parts.empty())
            {
                ImVec4 chams_color = settings::visuals::colors::chams_color;
                ImU32 chams_col32 = IM_COL32(
                    (int)(chams_color.x * 255),
                    (int)(chams_color.y * 255),
                    (int)(chams_color.z * 255),
                    (int)(chams_color.w * 255)
                );
                ImVec4 outline_color = settings::visuals::colors::chams_outline_color;
                ImU32 outline_col32 = IM_COL32(
                    (int)(outline_color.x * 255),
                    (int)(outline_color.y * 255),
                    (int)(outline_color.z * 255),
                    (int)(outline_color.w * 255)
                );
                Clipper2Lib::Paths64 unified_solution = Clipper2Lib::Union(all_parts, Clipper2Lib::FillRule::NonZero);
                for (Clipper2Lib::Path64& sp : unified_solution)
                {
                    std::vector<ImVec2> poly;
                    poly.reserve(sp.size());
                    for (Clipper2Lib::Point64& pt : sp)
                    {
                        poly.push_back(ImVec2(pt.x / 1000.0f, pt.y / 1000.0f));
                    }
                    if (poly.size() >= 3)
                    {
                        draw->AddConcavePolyFilled(poly.data(), poly.size(), chams_col32);
                        if (settings::visuals::chams_outline)
                        {
                            draw->AddPolyline(poly.data(), poly.size(), outline_col32, true, 2.f);
                        }
                    }
                }
            }
        }
        if (settings::visuals::china_hat)
        {
            if (entity.instance.address == cache::local_entity.instance.address)
            {
                rbx::part_t head_part = entity.parts["Head"];
                if (head_part.address)
                {
                    auto it_head = entity.primitive_cache.find("Head");
                    if (it_head == entity.primitive_cache.end())
                        continue;
                    math::vector3 head_pos = it_head->second.position;
                    float head_height = it_head->second.size.y * 0.5f;
                    math::vector3 top = head_pos + math::vector3(0, head_height * 2.f, 0);
                    float radius = head_height * 3.f;
                    int segments = settings::performance::low_end_mode ? 12 : 24;
                    std::vector<ImVec2> projected;
                    projected.reserve(segments);
                    math::vector2 top_screen;
                    if (game::visualengine.world_to_screen(top, top_screen))
                    {
                        top_screen.x *= scale_x;
                        top_screen.y *= scale_y;
                        for (int i = 0; i < segments; i++)
                        {
                            float angle = (float)i / segments * 2.f * IM_PI;
                            math::vector3 rim_offset = {
                                cosf(angle) * radius,
                                -head_height * 1.5f,
                                sinf(angle) * radius
                            };
                            math::vector3 rim_point = top + rim_offset;
                            math::vector2 rim_screen;
                            if (game::visualengine.world_to_screen(rim_point, rim_screen))
                            {
                                projected.emplace_back(rim_screen.x * scale_x, rim_screen.y * scale_y);
                            }
                        }
                        if (projected.size() >= 3)
                        {
                            draw->Flags &= ~ImDrawListFlags_AntiAliasedLines;
                            for (std::int32_t i = 0; i < projected.size(); i++)
                            {
                                ImVec2& p1 = projected[i];
                                ImVec2& p2 = projected[(i + 1) % projected.size()];
                                draw->AddTriangleFilled(
                                    ImVec2(top_screen.x, top_screen.y),
                                    p1, p2,
                                    IM_COL32(255, 255, 255, 100)
                                );
                                draw->AddTriangle(
                                    ImVec2(top_screen.x, top_screen.y),
                                    p1, p2,
                                    IM_COL32(0, 0, 0, 200),
                                    1.0f
                                );
                            }
                        }
                    }
                }
            }
        }
        bool render_text = lod_level < 3 && (!very_high_player_count || even_frame);
        if (render_text)
        {
            if (settings::visuals::tool && lod_level < 2)
            {
                ImVec2 tool_pos(right + 5.f, top + 3.f);
                ImVec2 zero(0, 0);
                visualize::text(tool_pos, zero, entity.tool.name.c_str(), IM_COL32(255, 255, 255, 255), 1, false);
            }
            if (settings::visuals::name && lod_level < 2)
            {
                visualize::text(c1, c2, entity.name.c_str(), IM_COL32(255, 255, 255, 255), 1, true);
            }
            if (settings::visuals::healthbar)
            {
                visualize::healthbar(c1, c2, entity.health, entity.max_health, IM_COL32(0, 255, 0, 255));
            }
            if (settings::visuals::armor && lod_level < 2)
            {
                visualize::armorbar(c1, c2, entity.health, entity.max_health, IM_COL32(0, 0, 255, 255), settings::visuals::healthbar);
            }
        }
        if (settings::visuals::distance && has_local_position && lod_level < 2 && render_text)
        {
            rbx::part_t other_part = entity.parts["HumanoidRootPart"];
            if (other_part.address)
            {
                auto it_other = entity.primitive_cache.find("HumanoidRootPart");
                if (it_other != entity.primitive_cache.end())
                {
                    math::vector3 other = it_other->second.position;
                    if (std::isfinite(other.x))
                    {
                        float dist = (other - local_position).length();
                        if (std::isfinite(dist))
                        {
                            char dist_buffer[16];
                            sprintf_s(dist_buffer, sizeof dist_buffer, "[%.1fm]", dist);
                            visualize::text(c1, c2, dist_buffer, IM_COL32(255, 255, 255, 255), 1, false);
                        }
                    }
                }
            }
        }
        if (settings::visuals::status && lod_level < 2 && render_text)
        {
            const char* sz_state = "Unknown";
            auto it_head = entity.primitive_cache.find("Head");
            if (entity.humanoid_state == 8 && it_head != entity.primitive_cache.end() && it_head->second.velocity.length() < 0.1f)
            {
                sz_state = "Idle";
            }
            else if (entity.humanoid_state < 19)
            {
                static const char* states[] =
                {
                    "FallingDown",
                    "Ragdoll",
                    "GettingUp",
                    "Jumping",
                    "Swimming",
                    "Freefall",
                    "Flying",
                    "Landed",
                    "Running",
                    "Unknown",
                    "RunningNoPhysics",
                    "StrafingNoPhysics",
                    "Climbing",
                    "Seated",
                    "PlatformStanding",
                    "Dead",
                    "Physics",
                    "Unknown",
                    "None"
                };
                sz_state = states[entity.humanoid_state];
            }
            ImVec2 flags_pos(right + 5.f, top + 9.f);
            ImVec2 zero(0, 0);
            visualize::text(flags_pos, zero, sz_state, IM_COL32(255, 255, 255, 255), 0.f, left);
        }
        if (settings::visuals::tracers && lod_level < 3)
        {
            ImVec2 screen_bottom((int)ImGui::GetIO().DisplaySize.x * 0.5f, (int)ImGui::GetIO().DisplaySize.y);
            ImVec2 box_bottom((int)(left + right) * 0.5f, (int)bottom);
            visualize::line(screen_bottom, box_bottom, IM_COL32(255, 255, 255, 255));
        }
    }
}