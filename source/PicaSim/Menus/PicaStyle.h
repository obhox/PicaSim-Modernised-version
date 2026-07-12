#ifndef PICASTYLE_H
#define PICASTYLE_H

#include "imgui.h"

//=================================================================================================
// PicaStyle - Centralized UI color and style definitions
//
// "Field index" design language: a flat periwinkle-sky ground, deep-navy ink,
// warm paper cards with a hairline navy border + hard offset shadow, and a single
// warm-orange flare accent used sparingly. All-monospace typography (see
// UIHelpers font loading). Everything below derives from the Palette namespace.
//=================================================================================================
namespace PicaStyle {

    // === PALETTE (single source of truth) ===
    namespace Palette {
        // sky #A9C4E8 | ink #15233F | paper #F6F2E9 | flare #EF4B22
        const ImVec4 Sky        = ImVec4(0.663f, 0.769f, 0.910f, 1.0f);
        const ImVec4 SkyDeep    = ImVec4(0.608f, 0.722f, 0.878f, 1.0f);
        const ImVec4 Ink        = ImVec4(0.082f, 0.137f, 0.247f, 1.0f);
        const ImVec4 Ink2       = ImVec4(0.329f, 0.392f, 0.541f, 1.0f);
        const ImVec4 Ink3       = ImVec4(0.514f, 0.565f, 0.682f, 1.0f);
        const ImVec4 Paper      = ImVec4(0.965f, 0.949f, 0.914f, 1.0f);
        const ImVec4 Paper2     = ImVec4(0.984f, 0.976f, 0.953f, 1.0f);
        const ImVec4 Flare      = ImVec4(0.937f, 0.294f, 0.133f, 1.0f);
        const ImVec4 FlareDeep  = ImVec4(0.835f, 0.243f, 0.094f, 1.0f);
        const ImVec4 Sun        = ImVec4(0.984f, 0.761f, 0.231f, 1.0f);
        const ImVec4 Leaf       = ImVec4(0.435f, 0.639f, 0.420f, 1.0f);
        const ImVec4 White      = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        // ImU32 forms for custom ImDrawList drawing
        const ImU32 SkyU32       = IM_COL32(169, 196, 232, 255);
        const ImU32 SkyDeepU32   = IM_COL32(155, 184, 224, 255);
        const ImU32 InkU32       = IM_COL32( 21,  35,  63, 255);
        const ImU32 Ink2U32      = IM_COL32( 84, 100, 138, 255);
        const ImU32 Ink3U32      = IM_COL32(131, 144, 174, 255);
        const ImU32 PaperU32     = IM_COL32(246, 242, 233, 255);
        const ImU32 Paper2U32    = IM_COL32(251, 249, 243, 255);
        const ImU32 FlareU32     = IM_COL32(239,  75,  34, 255);
        const ImU32 FlareDeepU32 = IM_COL32(213,  62,  24, 255);
        const ImU32 SunU32       = IM_COL32(251, 194,  59, 255);
        const ImU32 LeafU32      = IM_COL32(111, 163, 107, 255);
        const ImU32 WhiteU32     = IM_COL32(255, 255, 255, 255);
        const ImU32 LineU32      = IM_COL32( 21,  35,  63,  51);  // ink @ .20 hairline
        const ImU32 LineStrU32   = IM_COL32( 21,  35,  63, 107);  // ink @ .42
        const ImU32 ShadowU32    = IM_COL32( 21,  35,  63,  46);  // hard offset shadow
    }

    // === COMMON COLORS (mapped onto the palette; names kept for back-compat) ===
    namespace Common {
        // "Accent" is now the warm flare
        const ImVec4 Accent = Palette::Flare;
        const ImVec4 AccentHovered = Palette::FlareDeep;
        const ImVec4 AccentActive = Palette::FlareDeep;

        const ImU32 AccentU32 = Palette::FlareU32;
        const ImU32 AccentHoveredU32 = Palette::FlareDeepU32;
        const ImU32 AccentActiveU32 = Palette::FlareDeepU32;

        // Text colors
        const ImVec4 TextBlack = Palette::Ink;        // primary text is deep navy ink
        const ImVec4 TextWhite = Palette::White;
        const ImVec4 TextDimmed = Palette::Ink3;
        const ImVec4 TextSecondary = Palette::Ink2;
        const ImU32 TextPrimaryU32 = Palette::InkU32;
        const ImU32 TextSecondaryU32 = Palette::Ink2U32;

        // Input fields
        const ImVec4 FrameBgWhite = Palette::Paper2;

        // Standard styling
        const float CornerRadius = 8.0f;
    }

    // === MENU STYLES (FileMenu, HelpMenu, WhatsNewMenu) ===
    namespace Menu {
        const ImVec4 WindowBg = Palette::Sky;
        const ImVec4 ChildBg = Palette::Paper;
        const ImVec4 Button = Palette::Paper2;
        const ImVec4 ButtonHovered = ImVec4(0.937f, 0.906f, 0.851f, 1.0f); // warm paper hover
        const ImVec4 ButtonActive = ImVec4(0.898f, 0.859f, 0.792f, 1.0f);
        const ImVec4 Tab = Palette::SkyDeep;
        const ImVec4 TabHovered = Palette::Paper2;
        const ImVec4 TabActive = Palette::Paper;
    }

    // === SETTINGS MENU STYLES ===
    namespace Settings {
        const ImVec4 WindowBg = Palette::Sky;
        const ImVec4 BlockBg = Palette::Paper;
        // Button colors: use Menu::Button, Menu::ButtonHovered, Menu::ButtonActive

        // Section headers: navy bar (primary), orange bar (uber)
        const ImVec4 SectionHeaderBg = Palette::Ink;
        const ImVec4 UberSectionHeaderBg = Palette::Flare;
        const float SectionHeaderRounding = 6.0f;
        const float BlockRounding = 8.0f;
        const ImVec2 BlockPadding = ImVec2(12.0f, 10.0f);

        // Custom control colors
        const ImU32 SliderTrackBg = Palette::LineU32;
        const ImU32 SliderTrackFill = Palette::FlareU32;
        const ImU32 SliderGrab = Palette::FlareU32;
        const ImU32 SliderGrabHovered = Palette::FlareDeepU32;
        const ImU32 SliderGrabActive = Palette::FlareDeepU32;
        const float SliderTrackHeight = 6.0f;
        const float SliderGrabRadius = 8.0f;

        const ImU32 CheckboxBorder = Palette::InkU32;
        const ImU32 CheckboxFill = Palette::FlareU32;
        const ImU32 CheckboxCheck = Palette::WhiteU32;
        const float CheckboxSize = 18.0f;

        const ImVec4 ComboPopupBg = Palette::Paper2;
    }

    // === DIALOG STYLES (modal overlays) ===
    namespace Dialog {
        const ImVec4 WindowBg = ImVec4(0.965f, 0.949f, 0.914f, 0.98f); // paper, near-opaque
        const ImVec4 ChildBg = Palette::Paper2;
        const ImVec4 Border = Palette::Ink;
        // Uses Menu::Button colors for buttons
    }

    // === START MENU STYLES ===
    namespace StartMenu {
        const ImVec4 Button = Palette::Paper2;
        const ImVec4 ButtonHovered = Palette::Paper;
        const ImVec4 ButtonActive = ImVec4(0.898f, 0.859f, 0.792f, 1.0f);
    }

    // === IMAGE BUTTON STYLES (transparent buttons over images) ===
    namespace ImageButton {
        const ImVec4 Transparent = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        const ImVec4 HoverDark = ImVec4(0.082f, 0.137f, 0.247f, 0.14f);
        const ImVec4 ActiveDark = ImVec4(0.082f, 0.137f, 0.247f, 0.24f);
        const ImVec4 HoverLight = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
        const ImVec4 ActiveLight = ImVec4(1.0f, 1.0f, 1.0f, 0.4f);
        const ImVec4 Placeholder = Palette::SkyDeep;
    }

    // === LAYOUT CONSTANTS ===
    namespace Layout {
        const float ControlWidthFraction = 0.45f;
        const float LabelRightPadding = 16.0f;
        const float LabelValueButtonWidth = 60.0f;
        const float RowExtraSpacing = 3.0f;
    }

    // === HELPER FUNCTIONS ===
    void PushMenuStyle();
    void PopMenuStyle();
    void PushDialogStyle();
    void PopDialogStyle();
    void PushSettingsStyle();
    void PopSettingsStyle();
    void PushStartMenuButtonStyle(float scale);
    void PopStartMenuButtonStyle();

    // Draw an editorial "card": hard offset shadow + paper fill + hairline navy
    // border, into the given draw list. Pass hovered=true to lift/strengthen it.
    void DrawCard(ImDrawList* dl, ImVec2 pmin, ImVec2 pmax, bool hovered = false,
                  ImU32 fill = Palette::PaperU32, float rounding = 8.0f);

} // namespace PicaStyle

#endif // PICASTYLE_H
