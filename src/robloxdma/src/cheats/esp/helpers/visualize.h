#pragma once
#include <cmath>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <algorithm>
#include <render/render.h>
namespace visualize
{
	__forceinline void outline(ImVec2& c1, ImVec2& c2, ImU32 col, float rounding = 0.f)
	{
		c1.x = std::round(c1.x); c1.y = std::round(c1.y);
		c2.x = std::round(c2.x); c2.y = std::round(c2.y);
		ImDrawList* draw = ImGui::GetBackgroundDrawList();
		ImRect rect_bb(c1.x, c1.y, c1.x + c2.x, c1.y + c2.y);
		ImVec2 shadow_offset = { cosf(0.f) * 2.f, sinf(0.f) * 2.f };
		draw->AddRect(rect_bb.Min, rect_bb.Max, IM_COL32(0, 0, 0, col >> 24), rounding);
		draw->AddRect({ rect_bb.Min.x - 2.f, rect_bb.Min.y - 2.f }, { rect_bb.Max.x + 2.f, rect_bb.Max.y + 2.f }, IM_COL32(0, 0, 0, col >> 24), rounding);
		draw->AddRect({ rect_bb.Min.x - 1.f, rect_bb.Min.y - 1.f }, { rect_bb.Max.x + 1.f, rect_bb.Max.y + 1.f }, col, rounding);
	}
	__forceinline void filled(ImVec2& c1, ImVec2& c2, ImU32 col, float rounding = 0.f)
	{
		c1.x = std::round(c1.x); c1.y = std::round(c1.y);
		c2.x = std::round(c2.x); c2.y = std::round(c2.y);
		ImDrawList* draw = ImGui::GetBackgroundDrawList();
		ImRect rect_bb(c1.x, c1.y, c1.x + c2.x, c1.y + c2.y);
		draw->AddRectFilled(rect_bb.Min, rect_bb.Max, col, rounding);
	}
	__forceinline void text(ImVec2& c1, ImVec2& c2, const char* text, ImU32 col, std::int32_t align = 1, bool top = true)
	{
		ImDrawList* draw = ImGui::GetBackgroundDrawList();
		ImGui::PushFont(esp_font);
		ImVec2 text_size = ImGui::CalcTextSize(text);
		float ref_width = c2.x > 0.f ? c2.x : text_size.x;
		float x = c1.x;
		switch (align) {
		case 0: x = c1.x; break;
		case 1: x = c1.x + (ref_width * 0.5f) - (text_size.x * 0.5f); break;
		case 2: x = c1.x + ref_width - text_size.x; break;
		}
		ImVec2 final_pos;
		if (top)
		{
			final_pos = { std::round(x), std::round(c1.y - 12) };
		}
		else
		{
			final_pos = { std::round(x), std::round(c1.y + c2.y + 3) };
		}
		constexpr ImVec2 offsets[8] =
		{
			ImVec2(-1, 0),
			ImVec2(1, 0),
			ImVec2(1, -1),
			ImVec2(-1, 1),
			ImVec2(0, -1),
			ImVec2(0, 1),
			ImVec2(-1, -1),
			ImVec2(1, 1)
		};
		for (const ImVec2& o : offsets)
		{
			draw->AddText(nullptr, ImGui::GetFontSize(), ImVec2(final_pos.x + o.x, final_pos.y + o.y), IM_COL32(0, 0, 0, 255), text);
		}
		draw->AddText(nullptr, ImGui::GetFontSize(), final_pos, col, text);
		ImGui::PopFont();
	}
	__forceinline void line(ImVec2& c1, ImVec2& c2, ImU32 color, float thickness = 1.0f, ImU32 outline_color = IM_COL32(0, 0, 0, 255), float outline_thickness = 2.0f)
	{
		ImDrawList* draw = ImGui::GetBackgroundDrawList();
		draw->Flags &= ~ImDrawListFlags_AntiAliasedLines;
		draw->AddLine(c1, c2, outline_color, thickness + outline_thickness);
		draw->AddLine(c1, c2, color, thickness);
	}
	__forceinline void healthbar(ImVec2& c1, ImVec2& c2, float& health, float& max_health, ImU32 col)
	{
		auto draw = ImGui::GetBackgroundDrawList();
		float ratio = (max_health > 0.f) ? health / max_health : 0.f;
		ratio = std::clamp(ratio, 0.f, 1.f);
		float box_top = std::round(c1.y);
		float box_bottom = std::round(c1.y + c2.y);
		float box_left = std::round(c1.x);
		float bar_x = (box_left - 2.f) - 3.f;
		ImVec2 outline_min(bar_x - 1.f, box_top - 2.f);
		ImVec2 outline_max(bar_x + 2.f, box_bottom + 2.f);
		draw->AddRectFilled(outline_min, outline_max, IM_COL32(0, 0, 0, 255));
		float bar_height = (box_bottom - box_top) * ratio;
		ImVec2 fill_min(bar_x, (box_bottom - bar_height) - 1.f);
		ImVec2 fill_max(bar_x + 1.f, box_bottom + 1.f);
		draw->AddRectFilled(fill_min, fill_max, col);
	}
	__forceinline void armorbar(ImVec2& c1, ImVec2& c2, float& armor, float& max_armor, ImU32 col, bool has_healthbar)
	{
		auto draw = ImGui::GetBackgroundDrawList();
		float ratio = (max_armor > 0.f) ? armor / max_armor : 0.f;
		ratio = std::clamp(ratio, 0.f, 1.f);
		float box_top = std::round(c1.y);
		float box_bottom = std::round(c1.y + c2.y);
		float box_left = std::round(c1.x);
		float base_offset = has_healthbar ? 7.f : 3.f;
		float bar_x = (box_left - 2.f) - base_offset;
		ImVec2 outline_min(bar_x - 1.f, box_top - 2.f);
		ImVec2 outline_max(bar_x + 2.f, box_bottom + 2.f);
		draw->AddRectFilled(outline_min, outline_max, IM_COL32(0, 0, 0, 255));
		float bar_height = (box_bottom - box_top) * ratio;
		ImVec2 fill_min(bar_x, (box_bottom - bar_height) - 1.f);
		ImVec2 fill_max(bar_x + 1.f, box_bottom + 1.f);
		draw->AddRectFilled(fill_min, fill_max, col);
	}
}