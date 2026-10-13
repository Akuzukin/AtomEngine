#pragma once
#include "imgui.h"
#include <cmath>

namespace AtomEngine::EditorTheme
{
// Palette values describe display sRGB. The UI renderer blends linear vertex colors.
inline ImVec4 Color(int r, int g, int b, float alpha = 1.f)
{
    auto linear = [](int byte) {
        float value = byte / 255.f;
        return value <= .04045f ? value / 12.92f : std::pow((value + .055f) / 1.055f, 2.4f);
    };
    return ImVec4(linear(r), linear(g), linear(b), alpha);
}
inline ImVec4 Gray(int value, float alpha = 1.f)
{
    return Color(value, value, value, alpha);
}
inline ImU32 U32(int r, int g, int b, float alpha = 1.f)
{
    return ImGui::GetColorU32(Color(r, g, b, alpha));
}
inline ImVec4 Accent()
{
    return Color(66, 145, 215);
}
inline void Tooltip(const char *text)
{
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", text);
}
inline bool ToggleButton(const char *label, bool active, ImVec2 size = ImVec2(0, 0), ImVec4 color = Color(47, 86, 119))
{
    if (active)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Accent());
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Color(39, 103, 155));
    }
    bool clicked = ImGui::Button(label, size);
    if (active)
        ImGui::PopStyleColor(3);
    return clicked;
}
enum class Icon
{
    Play,
    Stop,
    Pause,
    Step
};
inline bool IconButton(const char *id, Icon icon, bool active, ImVec4 background, ImVec4 foreground = Gray(214))
{
    const float height = ImGui::GetFrameHeight();
    bool clicked = ToggleButton(id, active, ImVec2(height * 1.25f, height), background);
    auto min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    const float x = (min.x + max.x) * .5f, y = (min.y + max.y) * .5f, r = height * .23f;
    auto draw = ImGui::GetWindowDrawList();
    auto color = ImGui::GetColorU32(foreground);
    switch (icon)
    {
    case Icon::Play:
        draw->AddTriangleFilled(ImVec2(x - r * .65f, y - r), ImVec2(x + r, y), ImVec2(x - r * .65f, y + r), color);
        break;
    case Icon::Stop:
        draw->AddRectFilled(ImVec2(x - r, y - r), ImVec2(x + r, y + r), color, 1);
        break;
    case Icon::Pause:
        draw->AddRectFilled(ImVec2(x - r, y - r), ImVec2(x - r * .35f, y + r), color, 1);
        draw->AddRectFilled(ImVec2(x + r * .35f, y - r), ImVec2(x + r, y + r), color, 1);
        break;
    case Icon::Step:
        draw->AddTriangleFilled(ImVec2(x - r, y - r), ImVec2(x + r * .45f, y), ImVec2(x - r, y + r), color);
        draw->AddRectFilled(ImVec2(x + r * .65f, y - r), ImVec2(x + r * 1.05f, y + r), color, 1);
        break;
    }
    return clicked;
}
inline void Apply()
{
    ImGui::StyleColorsDark();
    auto &style = ImGui::GetStyle();
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.CellPadding = ImVec2(6, 5);
    style.WindowRounding = 0;
    style.ChildRounding = 3;
    style.FrameRounding = 3;
    style.PopupRounding = 4;
    style.ScrollbarRounding = 4;
    style.GrabRounding = 2;
    style.TabRounding = 3;
    style.WindowBorderSize = 1;
    style.ChildBorderSize = 1;
    style.PopupBorderSize = 1;
    style.FrameBorderSize = 0;
    style.TabBarBorderSize = 1;
    style.TabBarOverlineSize = 2;
    style.ScrollbarSize = 12;
    style.IndentSpacing = 16;
    style.DockingSeparatorSize = 4;
    style.DisabledAlpha = .5f;
    style.TreeLinesFlags = ImGuiTreeNodeFlags_DrawLinesToNodes;
    auto &c = style.Colors;
    c[ImGuiCol_Text] = Gray(214);
    c[ImGuiCol_TextDisabled] = Gray(128);
    c[ImGuiCol_WindowBg] = Gray(36);
    c[ImGuiCol_ChildBg] = Gray(30);
    c[ImGuiCol_PopupBg] = Gray(42);
    c[ImGuiCol_Border] = Gray(17);
    c[ImGuiCol_BorderShadow] = Gray(0, 0);
    c[ImGuiCol_FrameBg] = Gray(24);
    c[ImGuiCol_FrameBgHovered] = Gray(52);
    c[ImGuiCol_FrameBgActive] = Gray(60);
    c[ImGuiCol_TitleBg] = Gray(23);
    c[ImGuiCol_TitleBgActive] = Gray(32);
    c[ImGuiCol_TitleBgCollapsed] = Gray(23);
    c[ImGuiCol_MenuBarBg] = Gray(26);
    c[ImGuiCol_ScrollbarBg] = Gray(27);
    c[ImGuiCol_ScrollbarGrab] = Gray(65);
    c[ImGuiCol_ScrollbarGrabHovered] = Gray(83);
    c[ImGuiCol_ScrollbarGrabActive] = Gray(101);
    c[ImGuiCol_CheckMark] = Accent();
    c[ImGuiCol_SliderGrab] = Gray(117);
    c[ImGuiCol_SliderGrabActive] = Accent();
    c[ImGuiCol_Button] = Gray(51);
    c[ImGuiCol_ButtonHovered] = Gray(66);
    c[ImGuiCol_ButtonActive] = Gray(78);
    c[ImGuiCol_Header] = Gray(48);
    c[ImGuiCol_HeaderHovered] = Color(49, 67, 82);
    c[ImGuiCol_HeaderActive] = Color(43, 82, 115);
    c[ImGuiCol_Separator] = Gray(19);
    c[ImGuiCol_SeparatorHovered] = Accent();
    c[ImGuiCol_SeparatorActive] = Accent();
    c[ImGuiCol_ResizeGrip] = Gray(100, .15f);
    c[ImGuiCol_ResizeGripHovered] = Accent();
    c[ImGuiCol_ResizeGripActive] = Accent();
    c[ImGuiCol_InputTextCursor] = Gray(230);
    c[ImGuiCol_Tab] = Gray(25);
    c[ImGuiCol_TabHovered] = Gray(58);
    c[ImGuiCol_TabSelected] = Gray(43);
    c[ImGuiCol_TabSelectedOverline] = Accent();
    c[ImGuiCol_TabDimmed] = Gray(25);
    c[ImGuiCol_TabDimmedSelected] = Gray(39);
    c[ImGuiCol_TabDimmedSelectedOverline] = Gray(85);
    c[ImGuiCol_DockingPreview] = Color(66, 145, 215, .3f);
    c[ImGuiCol_DockingEmptyBg] = Gray(20);
    c[ImGuiCol_PlotLines] = Gray(162);
    c[ImGuiCol_PlotLinesHovered] = Accent();
    c[ImGuiCol_PlotHistogram] = Color(202, 164, 85);
    c[ImGuiCol_PlotHistogramHovered] = Color(232, 188, 96);
    c[ImGuiCol_TableHeaderBg] = Gray(43);
    c[ImGuiCol_TableBorderStrong] = Gray(19);
    c[ImGuiCol_TableBorderLight] = Gray(43);
    c[ImGuiCol_TableRowBg] = Gray(0, 0);
    c[ImGuiCol_TableRowBgAlt] = Gray(255, .018f);
    c[ImGuiCol_TextLink] = Accent();
    c[ImGuiCol_TextSelectedBg] = Color(48, 101, 148, .65f);
    c[ImGuiCol_TreeLines] = Gray(73);
    c[ImGuiCol_DragDropTarget] = Accent();
    c[ImGuiCol_DragDropTargetBg] = Color(66, 145, 215, .12f);
    c[ImGuiCol_UnsavedMarker] = Color(217, 172, 87);
    c[ImGuiCol_NavCursor] = Accent();
    c[ImGuiCol_NavWindowingHighlight] = Gray(220, .7f);
    c[ImGuiCol_NavWindowingDimBg] = Gray(0, .45f);
    c[ImGuiCol_ModalWindowDimBg] = Gray(0, .65f);
}
} // namespace AtomEngine::EditorTheme
