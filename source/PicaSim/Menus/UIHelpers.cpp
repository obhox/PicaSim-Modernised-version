#include "UIHelpers.h"
#include "Texture.h"
#include "Platform.h"

#include "imgui.h"

#include <cstdio>

namespace UIHelpers
{

// Static state
static ImFont* sFont = nullptr;      // regular UI font (monospace)
static ImFont* sFontBold = nullptr;  // bold face for headings / wordmark
static float sBaseFontSize = 17.0f;  // Base font size at 720p

//======================================================================================================================
void Init()
{
    // The UI uses JetBrains Mono (SIL OFL, bundled in Fonts/). The whole "field
    // index" look is built on a single monospace family - a bold face is loaded
    // alongside so headings/wordmarks read heavier without a second typeface.
    const char* fontPath     = "Fonts/JetBrainsMono-Regular.ttf";
    const char* fontBoldPath = "Fonts/JetBrainsMono-Bold.ttf";

    // Calculate initial font size based on current screen
    int height = Platform::GetDisplayHeight();
    float scale = height / 720.0f;
    if (scale < 1.0f) scale = 1.0f;
    float fontSize = sBaseFontSize * scale;

    ImGuiIO& io = ImGui::GetIO();

    // Custom glyph ranges: basic Latin + Latin Extended + General Punctuation (arrows, bullet •)
    static const ImWchar glyphRanges[] =
    {
        0x0020, 0x00FF,  // Basic Latin + Latin Supplement
        0x0100, 0x017F,  // Latin Extended-A
        0x2000, 0x206F,  // General Punctuation (bullet •, dashes, ↵)
        0x2100, 0x214F,  // Letterlike Symbols
        0x2190, 0x21FF,  // Arrows (→ ↑ ↓)
        0,
    };

    ImFontConfig fontConfig;
    fontConfig.OversampleH = 2;
    fontConfig.OversampleV = 2;
    sFont = io.Fonts->AddFontFromFileTTF(fontPath, fontSize, &fontConfig, glyphRanges);
    if (sFont)
        printf("UIHelpers: Loaded font from %s at size %.1f\n", fontPath, fontSize);
    else
    {
        // Fall back to the previous bundled sans if the mono font is missing.
        printf("UIHelpers: Failed to load %s, trying FontRegular.ttf\n", fontPath);
        sFont = io.Fonts->AddFontFromFileTTF("Fonts/FontRegular.ttf", fontSize, &fontConfig, glyphRanges);
        if (!sFont)
            sFont = io.Fonts->AddFontDefault();
    }

    // Bold face (slightly larger base so headings have presence). Optional.
    ImFontConfig boldConfig;
    boldConfig.OversampleH = 2;
    boldConfig.OversampleV = 2;
    sFontBold = io.Fonts->AddFontFromFileTTF(fontBoldPath, fontSize, &boldConfig, glyphRanges);
    if (!sFontBold)
        sFontBold = sFont;   // fall back to regular if bold is unavailable

    // Build the font atlas
    io.Fonts->Build();
}

//======================================================================================================================
void Shutdown()
{
    // Font is managed by ImGui, no need to manually delete
    sFont = nullptr;
}

//======================================================================================================================
float GetFontScale()
{
    int height = Platform::GetDisplayHeight();
    float scale = height / 720.0f;
    if (scale < 1.0f) scale = 1.0f;
    return scale;
}

//======================================================================================================================
void ApplyFontScale()
{
    ImGui::GetIO().FontGlobalScale = GetFontScale();
}

//======================================================================================================================
ImFont* GetFont()
{
    return sFont;
}

//======================================================================================================================
ImFont* GetBoldFont()
{
    return sFontBold ? sFontBold : sFont;
}

//======================================================================================================================
ImDrawList* DrawBackground(Texture* texture)
{
    if (!texture)
        return ImGui::GetBackgroundDrawList();

    int width = Platform::GetDisplayWidth();
    int height = Platform::GetDisplayHeight();

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    GLuint texID = texture->GetTextureID();
    int texW = texture->GetWidth();
    int texH = texture->GetHeight();

    // Calculate UV coordinates for centered cropping
    ImVec2 uv0(0.0f, 0.0f);
    ImVec2 uv1(1.0f, 1.0f);

    // Height if we stretched the texture to fit horizontally
    float h = (float(width) / texW) * texH;
    if (h < height)
    {
        // Image is wider than screen - crop sides
        float f = 0.5f * (1.0f - h / height);
        uv0.x = f;
        uv1.x = 1.0f - f;
    }
    else
    {
        // Image is taller than screen - crop top/bottom
        float f = 0.5f * (1.0f - height / h);
        uv0.y = f;
        uv1.y = 1.0f - f;
    }

    drawList->AddImage(
        (ImTextureID)(intptr_t)texID,
        ImVec2(0, 0),
        ImVec2((float)width, (float)height),
        uv0, uv1
    );

    return drawList;
}

//======================================================================================================================
void DrawCenteredText(const char* text, float verticalPosition, ImU32 color, float scale)
{
    if (!text || !*text)
        return;

    int width = Platform::GetDisplayWidth();
    int height = Platform::GetDisplayHeight();

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    // Use the loaded font if available
    ImFont* font = sFont ? sFont : ImGui::GetFont();

    // Calculate font size: base font size * screen scale * optional extra scale
    float screenScale = GetFontScale();
    float fontSize = font->FontSize * screenScale * scale;

    ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
    float textX = (width - textSize.x) * 0.5f;
    float textY = height * verticalPosition - textSize.y * 0.5f;

    drawList->AddText(font, fontSize, ImVec2(textX, textY), color, text);
}

//======================================================================================================================
bool DrawImageButton(const char* id, Texture* texture, float x, float y, float size)
{
    if (!texture)
        return false;

    // Position the cursor
    ImGui::SetCursorPos(ImVec2(x, y));

    // Get texture ID and calculate size maintaining aspect ratio
    GLuint texID = texture->GetTextureID();
    int texW = texture->GetWidth();
    int texH = texture->GetHeight();

    // Calculate button size maintaining aspect ratio
    float aspect = (texH > 0) ? (float)texW / (float)texH : 1.0f;
    ImVec2 buttonSize;
    if (aspect >= 1.0f)
    {
        buttonSize = ImVec2(size, size / aspect);
    }
    else
    {
        buttonSize = ImVec2(size * aspect, size);
    }

    // Draw the image button with transparent background (light hover for dark backgrounds)
    ImGui::PushStyleColor(ImGuiCol_Button, PicaStyle::ImageButton::Transparent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, PicaStyle::ImageButton::HoverLight);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, PicaStyle::ImageButton::ActiveLight);

    bool clicked = ImGui::ImageButton(id,
        (ImTextureID)(intptr_t)texID,
        buttonSize);

    ImGui::PopStyleColor(3);

    return clicked;
}

// NOTE: PushSettingsStyle, PopSettingsStyle, PushStartMenuButtonStyle, PopStartMenuButtonStyle
// have been moved to PicaStyle.cpp

} // namespace UIHelpers
