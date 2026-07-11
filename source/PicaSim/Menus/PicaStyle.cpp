#include "PicaStyle.h"

namespace PicaStyle {

// Navy hairline used for frame/button borders across the menus.
static const ImVec4 kBorder = ImVec4(0.082f, 0.137f, 0.247f, 0.55f);

//======================================================================================================================
void PushMenuStyle()
{
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Menu::WindowBg);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Menu::ChildBg);
    ImGui::PushStyleColor(ImGuiCol_Tab, Menu::Tab);
    ImGui::PushStyleColor(ImGuiCol_TabHovered, Menu::TabHovered);
    ImGui::PushStyleColor(ImGuiCol_TabActive, Menu::TabActive);
    ImGui::PushStyleColor(ImGuiCol_Text, Common::TextBlack);
    ImGui::PushStyleColor(ImGuiCol_Button, Menu::Button);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Menu::ButtonHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Menu::ButtonActive);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Common::FrameBgWhite);
    ImGui::PushStyleColor(ImGuiCol_Border, kBorder);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Common::CornerRadius);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Common::CornerRadius);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
}

//======================================================================================================================
void PopMenuStyle()
{
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(11);
}

//======================================================================================================================
void PushDialogStyle()
{
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Dialog::WindowBg);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Dialog::ChildBg);
    ImGui::PushStyleColor(ImGuiCol_Text, Common::TextBlack);
    ImGui::PushStyleColor(ImGuiCol_Button, Menu::Button);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Menu::ButtonHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Menu::ButtonActive);
    ImGui::PushStyleColor(ImGuiCol_Border, Dialog::Border);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Common::CornerRadius);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Common::CornerRadius);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
}

//======================================================================================================================
void PopDialogStyle()
{
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(7);
}

//======================================================================================================================
void PushSettingsStyle()
{
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Settings::WindowBg);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Settings::BlockBg);
    ImGui::PushStyleColor(ImGuiCol_Tab, Menu::Tab);
    ImGui::PushStyleColor(ImGuiCol_TabHovered, Menu::TabHovered);
    ImGui::PushStyleColor(ImGuiCol_TabActive, Menu::TabActive);
    ImGui::PushStyleColor(ImGuiCol_Text, Common::TextBlack);
    ImGui::PushStyleColor(ImGuiCol_Button, Menu::Button);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Menu::ButtonHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Menu::ButtonActive);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Common::FrameBgWhite);
    ImGui::PushStyleColor(ImGuiCol_Border, kBorder);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Common::CornerRadius);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Common::CornerRadius);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
}

//======================================================================================================================
void PopSettingsStyle()
{
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(11);
}

//======================================================================================================================
void PushStartMenuButtonStyle(float scale)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Common::CornerRadius * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Button, StartMenu::Button);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, StartMenu::ButtonHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, StartMenu::ButtonActive);
    ImGui::PushStyleColor(ImGuiCol_Text, Common::TextBlack);
}

//======================================================================================================================
void PopStartMenuButtonStyle()
{
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

//======================================================================================================================
void DrawCard(ImDrawList* dl, ImVec2 a, ImVec2 b, bool hovered, ImU32 fill, float rounding)
{
    const float s = ImGui::GetIO().FontGlobalScale;
    const float sx = (hovered ? 8.0f : 5.0f) * s;
    const float sy = (hovered ? 11.0f : 6.0f) * s;
    const float border = 1.6f * s;
    // hard offset shadow (behind), then paper fill, then hairline navy border
    dl->AddRectFilled(ImVec2(a.x + sx, a.y + sy), ImVec2(b.x + sx, b.y + sy),
                      Palette::ShadowU32, rounding);
    dl->AddRectFilled(a, b, fill, rounding);
    dl->AddRect(a, b, Palette::InkU32, rounding, 0, border);
}

} // namespace PicaStyle
