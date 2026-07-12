#include "StartMenu.h"
#include "SettingsMenu.h"
#include "HelpMenu.h"
#include "Menu.h"
#include "UIHelpers.h"
#include "../VersionChecker.h"
#include "../PicaJoystick.h"
#include "Texture.h"
#include "Platform.h"
#include "../../Platform/S3ECompat.h"
#include "../../Platform/Input.h"

#include "PicaStyle.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <memory>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cctype>
#include <cstring>

// Forward declarations from Graphics.cpp
void IwGxClear();
void IwGxSwapBuffers();
void PrepareForIwGx(bool fullscreen);
void RecoverFromIwGx(bool clear);

//======================================================================================================================
class StartMenu
{
public:
    StartMenu(GameSettings& gameSettings);
    ~StartMenu();

    // Render one frame, returns the result (STARTMENU_MAX if no action yet)
    StartMenuResult Update();

private:
    void LoadTextures();
    void Render();

    GameSettings& mGameSettings;
    StartMenuResult mResult;

    // Textures
    std::unique_ptr<Texture> mBackgroundTexture;
    std::unique_ptr<Texture> mFacebookTexture;
    std::unique_ptr<Texture> mHelpTexture;
    std::unique_ptr<Texture> mSettingsTexture;
    std::unique_ptr<Texture> mExitTexture;
    std::unique_ptr<Texture> mNewVersionTexture;
};

//======================================================================================================================
StartMenu::StartMenu(GameSettings& gameSettings)
    : mGameSettings(gameSettings)
    , mResult(STARTMENU_MAX)
{
    PrepareForIwGx(false);
    LoadTextures();
}

//======================================================================================================================
StartMenu::~StartMenu()
{
    // unique_ptr handles texture cleanup
    RecoverFromIwGx(false);
}

//======================================================================================================================
void StartMenu::LoadTextures()
{
    // Background
    mBackgroundTexture = std::make_unique<Texture>();
    mBackgroundTexture->LoadFromFile("Menus/StartBackground.jpg");
    mBackgroundTexture->SetMipMapping(false);
    mBackgroundTexture->Upload();

    // Icon buttons
    mFacebookTexture = std::make_unique<Texture>();
    mFacebookTexture->LoadFromFile("Menus/Facebook.png");
    mFacebookTexture->SetMipMapping(false);
    mFacebookTexture->Upload();

    mHelpTexture = std::make_unique<Texture>();
    mHelpTexture->LoadFromFile("Menus/Help.png");
    mHelpTexture->SetMipMapping(false);
    mHelpTexture->Upload();

    mSettingsTexture = std::make_unique<Texture>();
    mSettingsTexture->LoadFromFile("Menus/Utilities.png");
    mSettingsTexture->SetMipMapping(false);
    mSettingsTexture->Upload();

    mExitTexture = std::make_unique<Texture>();
    mExitTexture->LoadFromFile("Menus/Stop.png");
    mExitTexture->SetMipMapping(false);
    mExitTexture->Upload();

#if defined(PICASIM_WINDOWS)
    // NewVersion texture (Windows only)
    mNewVersionTexture = std::make_unique<Texture>();
    mNewVersionTexture->LoadFromFile("Menus/NewVersion.png");
    mNewVersionTexture->SetMipMapping(false);
    mNewVersionTexture->Upload();
#endif
}

//======================================================================================================================
StartMenuResult StartMenu::Update()
{
    mResult = STARTMENU_MAX;

    // Clear and render
    IwGxClear();
    Render();
    IwGxSwapBuffers();

    PollEvents();

    // Check for quit request
    if (CheckForQuitRequest())
    {
        mResult = STARTMENU_QUIT;
    }

    return mResult;
}

//======================================================================================================================
// Draws a small line-icon (paper plane / flag) as a closed polyline inside a rect.
static void DrawPlaneIcon(ImDrawList* dl, ImVec2 c, float r, ImU32 col, float th)
{
    // paper-plane silhouette, points normalised in a ~[-1,1] box
    const float px[7] = {-1.00f, 1.00f, 0.30f, 0.86f,-0.20f,-0.36f,-0.52f};
    const float py[7] = { 0.32f,-0.65f,-0.10f, 0.20f, 0.40f, 0.95f, 0.16f};
    ImVec2 p[7];
    for (int i = 0; i < 7; ++i) p[i] = ImVec2(c.x + px[i]*r, c.y + py[i]*r);
    dl->AddPolyline(p, 7, col, ImDrawFlags_Closed, th);
}
static void DrawFlagIcon(ImDrawList* dl, ImVec2 c, float r, ImU32 col, float th)
{
    ImVec2 top(c.x - r*0.55f, c.y - r);
    ImVec2 bot(c.x - r*0.55f, c.y + r);
    dl->AddLine(top, bot, col, th);
    ImVec2 f[3] = { ImVec2(top.x, top.y), ImVec2(c.x + r*0.9f, c.y - r*0.55f), ImVec2(top.x, c.y) };
    dl->AddPolyline(f, 3, col, ImDrawFlags_Closed, th);
    dl->AddConvexPolyFilled(f, 3, IM_COL32(239,75,34,60));
}

//======================================================================================================================
void StartMenu::Render()
{
    using namespace PicaStyle;
    const float W = (float)Platform::GetDisplayWidth();
    const float H = (float)Platform::GetDisplayHeight();
    const float s = UIHelpers::GetFontScale();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    UIHelpers::ApplyFontScale();

    ImFont* reg  = UIHelpers::GetFont();
    ImFont* bold = UIHelpers::GetBoldFont();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(W, H));
    ImGui::Begin("StartMenu", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // flat periwinkle ground
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), Palette::SkyU32);

    // clickable text helper
    auto textBtn = [&](const char* id, ImVec2 pos, const char* txt, ImFont* f, float sz, ImU32 base) -> bool {
        ImVec2 ts = f->CalcTextSizeA(sz, FLT_MAX, 0.0f, txt);
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton(id, ImVec2(ts.x + 2, ts.y + 2));
        bool hov = ImGui::IsItemHovered();
        dl->AddText(f, sz, pos, hov ? Palette::FlareU32 : base, txt);
        return ImGui::IsItemClicked();
    };

    const float M = 46.0f * s;

    // ---------- top bar ----------
    float topY = 30.0f * s;
    // wordmark: "pica" ink + "sim" flare
    float wsz = 30.0f * s;
    ImVec2 wpos(M, topY);
    const char* w1 = "pica"; const char* w2 = "sim";
    dl->AddText(bold, wsz, wpos, Palette::InkU32, w1);
    float w1w = bold->CalcTextSizeA(wsz, FLT_MAX, 0, w1).x;
    dl->AddText(bold, wsz, ImVec2(wpos.x + w1w, wpos.y), Palette::FlareU32, w2);
    float w2w = bold->CalcTextSizeA(wsz, FLT_MAX, 0, w2).x;
    dl->AddText(reg, 12.0f*s, ImVec2(wpos.x + w1w + w2w + 14*s, wpos.y + wsz*0.42f),
                Palette::Ink2U32, "// grab a plane, go make some noise");

    // right mini-nav
    float navSz = 13.0f * s;
    float nx = W - M;
    const char* navHelp = "Help"; const char* navCtl = "Controller"; const char* navSet = "Settings";
    nx -= reg->CalcTextSizeA(navSz, FLT_MAX, 0, navHelp).x;
    if (textBtn("nHelp", ImVec2(nx, topY + wsz*0.28f), navHelp, reg, navSz, Palette::Ink2U32)) mResult = STARTMENU_HELP;
    nx -= 20*s + reg->CalcTextSizeA(navSz, FLT_MAX, 0, navCtl).x;
    if (textBtn("nCtl", ImVec2(nx, topY + wsz*0.28f), navCtl, reg, navSz, Palette::Ink2U32)) mResult = STARTMENU_SETTINGS;
    nx -= 20*s + reg->CalcTextSizeA(navSz, FLT_MAX, 0, navSet).x;
    if (textBtn("nSet", ImVec2(nx, topY + wsz*0.28f), navSet, reg, navSz, Palette::Ink2U32)) mResult = STARTMENU_SETTINGS;

    // ---------- layout regions ----------
    float mainTop = topY + wsz + 26.0f * s;
    float footH   = 26.0f * s;
    float mainBot = H - footH - 20.0f * s;
    float colGap  = 22.0f * s;
    float leftW   = 320.0f * s;
    float leftX   = M;
    float rightX  = M + leftW + colGap;
    float rightR  = W - M;
    float cardGap = 20.0f * s;
    float modeH   = ((mainBot - mainTop) - cardGap) * 0.5f;

    // ---------- mode card helper ----------
    auto modeCard = [&](const char* id, ImVec2 a, ImVec2 b, const char* title, const char* sub,
                        const char* cta, bool flareIcon, bool plane) -> bool {
        float pad = 22.0f * s;
        ImGui::SetCursorScreenPos(a);
        ImGui::InvisibleButton(id, ImVec2(b.x - a.x, b.y - a.y));
        bool hov = ImGui::IsItemHovered();
        bool clk = ImGui::IsItemClicked();
        DrawCard(dl, a, b, hov);
        // icon tile
        float tile = 44.0f * s;
        ImVec2 t0(a.x + pad, a.y + pad), t1(t0.x + tile, t0.y + tile);
        dl->AddRectFilled(t0, t1, flareIcon ? Palette::FlareU32 : Palette::Paper2U32, 10.0f*s);
        dl->AddRect(t0, t1, Palette::InkU32, 10.0f*s, 0, 1.6f*s);
        ImVec2 ic((t0.x+t1.x)*0.5f, (t0.y+t1.y)*0.5f);
        if (plane) DrawPlaneIcon(dl, ic, tile*0.32f, flareIcon ? Palette::WhiteU32 : Palette::InkU32, 1.8f*s);
        else       DrawFlagIcon (dl, ic, tile*0.30f, flareIcon ? Palette::WhiteU32 : Palette::InkU32, 1.8f*s);
        // title
        float ty = t1.y + 16.0f*s;
        dl->AddText(bold, 23.0f*s, ImVec2(a.x + pad, ty), Palette::InkU32, title);
        // subtitle (wrapped)
        dl->AddText(reg, 12.5f*s, ImVec2(a.x + pad, ty + 30.0f*s), Palette::Ink2U32,
                    sub, nullptr, (b.x - a.x) - pad*2.0f);
        // cta bottom-left + arrow bottom-right
        dl->AddText(reg, 11.0f*s, ImVec2(a.x + pad, b.y - pad - 12.0f*s),
                    hov ? Palette::FlareU32 : Palette::InkU32, cta);
        const char* arw = "\xe2\x86\x92"; // →
        float aw = reg->CalcTextSizeA(15.0f*s, FLT_MAX, 0, arw).x;
        dl->AddText(reg, 15.0f*s, ImVec2(b.x - pad - aw + (hov?4*s:0), b.y - pad - 14.0f*s),
                    Palette::FlareU32, arw);
        return clk;
    };

    if (modeCard("cFly", ImVec2(leftX, mainTop), ImVec2(leftX+leftW, mainTop+modeH),
                 "Free Flight", "Open skies. Any aircraft, any site - soar, glide, learn the sticks.",
                 "FLY", false, true))
        mResult = STARTMENU_FLY;

    if (modeCard("cChl", ImVec2(leftX, mainTop+modeH+cardGap), ImVec2(leftX+leftW, mainBot),
                 "Challenges", "Race the gates, thread the limbo, beat your best time.",
                 "COMPETE", true, false))
        mResult = STARTMENU_CHALLENGE;

    // ---------- real current-selection data for the hero ----------
    const std::string& acRaw   = mGameSettings.mAeroplaneSettings.mTitle;
    const std::string& siteRaw = mGameSettings.mEnvironmentSettings.mTitle;
    const char* dash = "\xe2\x80\x94"; // em dash fallback
    const char* acName   = acRaw.empty()   ? dash : acRaw.c_str();
    const char* siteName = siteRaw.empty() ? dash : siteRaw.c_str();
    char siteUpper[80];
    {
        size_t n = siteRaw.size(); if (n >= sizeof(siteUpper)) n = sizeof(siteUpper) - 1;
        for (size_t i = 0; i < n; ++i) siteUpper[i] = (char)toupper((unsigned char)siteRaw[i]);
        siteUpper[n] = 0;
        if (n == 0) { siteUpper[0] = '\xe2'; siteUpper[1] = '\x80'; siteUpper[2] = '\x94'; siteUpper[3] = 0; }
    }
    char windLine[112];
    {
        static const char* dirs[8] = { "N","NE","E","SE","S","SW","W","NW" };
        float ms = mGameSettings.mEnvironmentSettings.mWindSpeed;
        float bearing = mGameSettings.mEnvironmentSettings.mWindBearing;
        int kmh = (int)lroundf(ms * 3.6f);
        int di = (((int)lroundf(bearing / 45.0f)) % 8 + 8) % 8;
        bool thermals = mGameSettings.mEnvironmentSettings.mThermalDensity > 0.0f;
        snprintf(windLine, sizeof(windLine), "wind %s %d km/h%s",
                 dirs[di], kmh, thermals ? " \xc2\xb7 thermals" : "");
    }

    // ---------- hero card ----------
    {
        ImVec2 a(rightX, mainTop), b(rightR, mainBot);
        DrawCard(dl, a, b, false, Palette::Paper2U32);
        float border = 1.6f * s;
        // scene image (top ~62%)
        float imgH = (b.y - a.y) * 0.62f;
        ImVec2 i0(a.x + border, a.y + border), i1(b.x - border, a.y + imgH);
        if (mBackgroundTexture && mBackgroundTexture->GetTextureID())
        {
            ImTextureID tex = (ImTextureID)(intptr_t)mBackgroundTexture->GetTextureID();
            dl->AddImageRounded(tex, i0, i1, ImVec2(0.30f, 0.16f), ImVec2(0.74f, 0.62f),
                                IM_COL32(255,255,255,255), 8.0f*s, ImDrawFlags_RoundCornersTop);
        }
        else dl->AddRectFilled(i0, i1, Palette::SkyDeepU32, 8.0f*s, ImDrawFlags_RoundCornersTop);
        // separator under image
        dl->AddLine(ImVec2(a.x, i1.y), ImVec2(b.x, i1.y), Palette::InkU32, 1.6f*s);

        // "NOW FLYING" stamp
        const char* stamp = "NOW FLYING";
        float stSz = 10.0f*s;
        ImVec2 sts = reg->CalcTextSizeA(stSz, FLT_MAX, 0, stamp);
        ImVec2 s0(i0.x + 12*s, i0.y + 12*s), s1(s0.x + sts.x + 18*s, s0.y + sts.y + 10*s);
        dl->AddRectFilled(s0, s1, Palette::FlareU32, 4.0f*s);
        dl->AddText(reg, stSz, ImVec2(s0.x + 9*s, s0.y + 5*s), Palette::WhiteU32, stamp);

        // caption (site + wind) bottom of image
        dl->AddText(bold, 18.0f*s, ImVec2(i0.x + 14*s, i1.y - 34*s), Palette::WhiteU32, siteUpper);
        dl->AddText(reg, 11.5f*s, ImVec2(i0.x + 14*s, i1.y - 15*s), IM_COL32(240,244,250,235), windLine);

        // footer row: setup chips + take-off
        float fy = i1.y + 18*s;
        dl->AddText(reg, 9.5f*s, ImVec2(a.x + 20*s, fy), Palette::Ink3U32, "AIRCRAFT");
        dl->AddText(bold, 14.0f*s, ImVec2(a.x + 20*s, fy + 13*s), Palette::InkU32, acName);
        dl->AddText(reg, 9.5f*s, ImVec2(a.x + 170*s, fy), Palette::Ink3U32, "SITE");
        dl->AddText(bold, 14.0f*s, ImVec2(a.x + 170*s, fy + 13*s), Palette::InkU32, siteName);
        // invisible buttons over the chips -> settings
        ImGui::SetCursorScreenPos(ImVec2(a.x + 18*s, fy - 2*s)); ImGui::InvisibleButton("chAc", ImVec2(140*s, 34*s));
        if (ImGui::IsItemClicked()) mResult = STARTMENU_SETTINGS;
        ImGui::SetCursorScreenPos(ImVec2(a.x + 168*s, fy - 2*s)); ImGui::InvisibleButton("chSite", ImVec2(150*s, 34*s));
        if (ImGui::IsItemClicked()) mResult = STARTMENU_SETTINGS;

        // take-off button (right)
        const char* to = "Take off";
        float toSz = 16.0f*s;
        float tow = bold->CalcTextSizeA(toSz, FLT_MAX, 0, to).x;
        float btnW = tow + 34*s + 24*s, btnH = 40*s;
        ImVec2 t0(b.x - 20*s - btnW, i1.y + (b.y - i1.y - btnH)*0.5f), t1(b.x - 20*s, t0.y + btnH);
        ImGui::SetCursorScreenPos(t0); ImGui::InvisibleButton("takeoff", ImVec2(btnW, btnH));
        bool thov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) mResult = STARTMENU_FLY;
        dl->AddRectFilled(ImVec2(t0.x+3*s, t0.y+4*s), ImVec2(t1.x+3*s, t1.y+4*s), Palette::ShadowU32, 9.0f*s);
        dl->AddRectFilled(t0, t1, thov ? Palette::FlareDeepU32 : Palette::FlareU32, 9.0f*s);
        dl->AddRect(t0, t1, Palette::InkU32, 9.0f*s, 0, 1.6f*s);
        dl->AddText(bold, toSz, ImVec2(t0.x + 16*s, t0.y + (btnH-toSz)*0.5f), Palette::WhiteU32, to);
        DrawPlaneIcon(dl, ImVec2(t1.x - 20*s, (t0.y+t1.y)*0.5f), 9*s, Palette::WhiteU32, 1.8f*s);
    }

    // ---------- footer ----------
    float fY = H - footH - 6*s;
    if (textBtn("fWhat", ImVec2(M, fY), "What's new - v1.1", reg, 11.5f*s, Palette::Ink2U32))
        mResult = STARTMENU_HELP;
#if !defined(PICASIM_IOS) && !defined(PICASIM_WIN10)
    const char* quit = "Quit";
    float qw = reg->CalcTextSizeA(11.5f*s, FLT_MAX, 0, quit).x;
    if (textBtn("fQuit", ImVec2(W - M - qw, fY), quit, reg, 11.5f*s, Palette::Ink2U32))
        mResult = STARTMENU_QUIT;
#endif

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

//======================================================================================================================
StartMenuResult DisplayStartMenu(GameSettings& gameSettings)
{
    StartMenuResult result = STARTMENU_MAX;

    while (result == STARTMENU_MAX)
    {
        StartMenu startMenu(gameSettings);

        // Menu loop
        while (true)
        {
            result = startMenu.Update();

            if (result != STARTMENU_MAX)
                break;

            // Check for back button / quit
            if (CheckForQuitRequest() ||
                    (Input::GetInstance().GetKeyState(SDLK_AC_BACK) & KEY_STATE_PRESSED))
            {
                result = STARTMENU_QUIT;
                break;
            }
        }

        // Handle sub-menus (return to start menu after)
        if (result == STARTMENU_SETTINGS)
        {
            SettingsChangeActions actions;
            DisplaySettingsMenu(gameSettings, actions);
            result = STARTMENU_MAX;  // Return to start menu
        }
        else if (result == STARTMENU_HELP)
        {
            DisplayHelpMenu(gameSettings, false);
            result = STARTMENU_MAX;  // Return to start menu
        }
        else if (result == STARTMENU_REFRESH)
        {
            result = STARTMENU_MAX;  // Just refresh
        }
    }

    return result;
}
