#include "ChallengeFreeFly.h"
#include "PicaSim.h"
#include "Aeroplane.h"
#include "AeroplanePhysics.h"
#include "AIControllerGlider.h"
#include "AIControllerPowered.h"
#include "Menus/Menu.h"

#include "../Platform/S3ECompat.h"
#include "../Platform/FontRenderer.h"
#include "ShaderManager.h"
#include "Shaders.h"
#include "Graphics.h"

#include <vector>

//======================================================================================================================
void ChallengeFreeFly::Relaunched()
{
    mMaxSpeed = 0.0f;
    mOnGroundTime = 0.0f;
    mSmoothedAscentRate = 0.0f;
}

//======================================================================================================================
void ChallengeFreeFly::Init(Aeroplane* aeroplane, LoadingScreenHelper* loadingScreen)
{
    TRACE_METHOD_ONLY(1);
    mAeroplane = aeroplane;
    RenderManager::GetInstance().RegisterRenderGxObject(this, 0);
    mNeedToCacheText = true;

    Relaunched();

    const GameSettings& gs = PicaSim::GetInstance().GetSettings();
    const AIControllersSettings& aics = gs.mAIControllersSettings;

    int numAIControllers = 0;

    while (numAIControllers < gs.mOptions.mFreeFlightMaxAI)
    {
        bool gotOne = false;
        for (size_t i = 0 ; i != aics.mAIControllers.size() && numAIControllers < gs.mOptions.mFreeFlightMaxAI; ++i)
        {
            const AIControllersSettings::AIControllerSetting& aic = aics.mAIControllers[i];

            bool available = true;
            AeroplaneSettings as;
            if (!as.LoadFromFile(aic.mAeroplaneFile.c_str()))
            {
                available = false;
            }

            AIController* controller = 0;
            if (available)
            {
                switch (as.mAIType)
                {
                case AeroplaneSettings::AITYPE_GLIDER:
                    controller = new AIControllerGlider(aic, numAIControllers);
                    break;
                case AeroplaneSettings::AITYPE_POWERED:
                    controller = new AIControllerPowered(aic, numAIControllers);
                    break;
                default:
                    break;
                }
            }
            if (controller)
            {
                if (controller->Init(loadingScreen))
                {
                    mAIControllers.push_back(controller);

                    ++numAIControllers;
                    gotOne = true;
                }
                else
                {
                    delete controller;
                }
            }
        }

        if (!gotOne)
            break;

        if (!aics.mCreateMaxNumControllers)
            break;
    }
}

//======================================================================================================================
void ChallengeFreeFly::Reset()
{
    for (size_t i = 0 ; i != mAIControllers.size() ; ++i)
    {
        AIController* aic = mAIControllers[i];
        aic->Reset();
    }
}

//======================================================================================================================
void ChallengeFreeFly::Terminate()
{
    TRACE_METHOD_ONLY(1);
    RenderManager::GetInstance().UnregisterRenderGxObject(this, 0);

    while (!mAIControllers.empty())
    {
        AIController* controller = mAIControllers.back();
        controller->Terminate();
        delete controller;
        mAIControllers.pop_back();
    }
}

//======================================================================================================================
Challenge::ChallengeResult ChallengeFreeFly::UpdateChallenge(float deltaTime)
{
    Challenge::UpdateChallenge(deltaTime);

    GameSettings& gs = PicaSim::GetInstance().GetSettings();

    // TODO better metric for this
    float speed = mAeroplane->GetVelocity().GetLength();
    bool touching = mAeroplane->GetPhysics()->GetContactTime() > 0.0f;

    if (mAeroplane->GetLaunchMode() == Aeroplane::LAUNCHMODE_AEROTOW)
        touching = false;

    if (speed > mMaxSpeed)
        mMaxSpeed = speed;

    if (speed < 1.0f && touching)
        mOnGroundTime += deltaTime;
    else
        mOnGroundTime = 0.0f;

    if (mOnGroundTime > gs.mAeroplaneSettings.mRelaunchTime && gs.mAeroplaneSettings.mRelaunchWhenStationary)
    {
        mOnGroundTime = 0.0f;
        return CHALLENGE_RELAUNCH;
    }

    if (mAeroplane->GetCrashed() && mOnGroundTime > gs.mAeroplaneSettings.mRelaunchTime)
    {
        mOnGroundTime = 0.0f;
        return CHALLENGE_RELAUNCH;
    }

    return CHALLENGE_CONTINUE;
}

//======================================================================================================================
const char* GetSpeedUnitText(const Options& options)
{
    switch (options.mFreeFlightUnits)
    {
    case Options::UNITS_MPS:
    default:
        return "m/s";
    case Options::UNITS_KPH:
        return "km/h";
    case Options::UNITS_MPH:
        return "mph";
    }
}

//======================================================================================================================
const float GetSpeed(const Options& options, float speed)
{
    switch (options.mFreeFlightUnits)
    {
    case Options::UNITS_MPS:
    default:
        return speed;
    case Options::UNITS_KPH:
        return speed * 3.6f;
    case Options::UNITS_MPH:
        return speed * 2.23693629f; // to mph
    }
}

//======================================================================================================================
const char* GetAscentRateUnitText(const Options& options)
{
    switch (options.mFreeFlightUnits)
    {
    case Options::UNITS_MPS:
    case Options::UNITS_KPH:
    default:
        return "m/s";
    case Options::UNITS_MPH:
        return "ft/s";
    }
}

//======================================================================================================================
const float GetAscentRate(const Options& options, float speed)
{
    switch (options.mFreeFlightUnits)
    {
    case Options::UNITS_MPS:
    case Options::UNITS_KPH:
    default:
        return speed;
    case Options::UNITS_MPH:
        return speed * 3.2808399f; // to ft/s
    }
}

//======================================================================================================================
const char* GetDistanceUnitText(const Options& options)
{
    switch (options.mFreeFlightUnits)
    {
    case Options::UNITS_MPS:
    case Options::UNITS_KPH:
    default:
        return "m";
    case Options::UNITS_MPH:
        return "ft";
    }
}

//======================================================================================================================
const float GetDistance(const Options& options, float distance)
{
    switch (options.mFreeFlightUnits)
    {
    case Options::UNITS_MPS:
    case Options::UNITS_KPH:
    default:
        return distance;
    case Options::UNITS_MPH:
        return distance * 3.2808399f; // to feet
    }
}

//======================================================================================================================
//======================================================================================================================
// FPV "OSD": drawn when the onboard (aeroplane) camera is active, so drone /
// FPV flying gets a Betaflight-style overlay - a centre reticle, an artificial
// horizon that banks + pitches with the aircraft, and corner readouts. Uses the
// same FontRenderer + ControllerShader 2D-line path as the rest of the HUD.
static void DrawFpvOsd(DisplayConfig& dc, const Aeroplane* a, const Camera* cam)
{
    FontRenderer& font = FontRenderer::GetInstance();
    uint32 origColour = font.GetColourABGR();
    uint16 fh = font.GetFontHeight();
    const Options& options = PicaSim::GetInstance().GetSettings().mOptions;

    // --- centre reticle + artificial horizon (GL lines) ---
    {
        DisableDepthTest disableDepthTest;
        DisableDepthMask disableDepthMask;
        EnableBlend enableBlend;
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        GLint vp[4];
        glGetIntegerv(GL_VIEWPORT, vp);
        float cx = vp[0] + vp[2] * 0.5f;
        float cy = vp[1] + vp[3] * 0.5f;
        float H = (float)vp[3];

        // Attitude from the aircraft frame (X fwd, Y left, Z up).
        const Transform& tm = a->GetTransform();
        Vector3 fwd = tm.RowX(), up = tm.RowZ(), left = tm.RowY();
        float pitch = asinf(ClampToRange(fwd.z, -1.0f, 1.0f));
        float roll  = atan2f(-left.z, up.z);
        float vfov = cam->GetVerticalFOV();
        if (vfov < 0.1f) vfov = 1.0f;
        float horizonY = cy - (pitch / vfov) * H;   // nose-up pushes the horizon down
        float ca = cosf(roll), sa = sinf(roll);

        std::vector<GLfloat> pts;
        auto seg = [&](float x0, float y0, float x1, float y1) {
            pts.push_back(x0); pts.push_back(y0); pts.push_back(0);
            pts.push_back(x1); pts.push_back(y1); pts.push_back(0);
        };
        // horizon endpoints are rotated by roll about (cx, horizonY)
        auto rot = [&](float x, float y, float& ox, float& oy) {
            ox = cx + x * ca - y * sa; oy = horizonY + x * sa + y * ca;
        };

        float s = H / 720.0f;               // scale reticle with resolution
        float g = 7 * s, arm = 15 * s;
        seg(cx - g - arm, cy, cx - g, cy);  seg(cx + g, cy, cx + g + arm, cy);
        seg(cx, cy - g - arm, cx, cy - g);  seg(cx, cy + g, cx, cy + g + arm);

        float hw = vp[2] * 0.40f, hg = 45 * s, tick = 9 * s;
        float ax, ay, bx, by;
        rot(-hw, 0, ax, ay); rot(-hg, 0, bx, by); seg(ax, ay, bx, by);
        rot( hg, 0, ax, ay); rot( hw, 0, bx, by); seg(ax, ay, bx, by);
        rot(-hw, 0, ax, ay); rot(-hw, -tick, bx, by); seg(ax, ay, bx, by);
        rot( hw, 0, ax, ay); rot( hw, -tick, bx, by); seg(ax, ay, bx, by);

        const ControllerShader* shader = (ControllerShader*) ShaderManager::GetInstance().GetShader(SHADER_CONTROLLER);
        shader->Use();
        gStreamVBO.Bind();
        size_t off = gStreamVBO.Upload(pts.data(), pts.size() * sizeof(GLfloat));
        glVertexAttribPointer(shader->a_position, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)off);
        glEnableVertexAttribArray(shader->a_position);
        glUniform4f(shader->u_colour, 1.0f, 1.0f, 1.0f, 0.85f);
        esSetModelViewProjectionMatrix(shader->u_mvpMatrix);
        glDrawArrays(GL_LINES, 0, (GLsizei)(pts.size() / 3));
        glDisableVertexAttribArray(shader->a_position);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    // --- corner readouts ---
    int16 topY = (int16)(dc.mBottom + fh / 4);
    int16 botY = (int16)(dc.mBottom + dc.mHeight - fh * 5 / 4);
    int16 pad  = (int16)(fh / 2);
    char t[64];
    font.SetColourABGR(0xffffffff);

    font.SetRect((int16)(dc.mLeft + pad), topY, (int16)dc.mWidth, fh);
    font.SetAlignmentHor(FONT_ALIGN_LEFT);
    font.RenderText("FPV");

    float ft = a->GetFlightTime();
    int mn = (int)(ft / 60.0f); float scs = ft - mn * 60.0f;
    if (mn > 0) snprintf(t, sizeof(t), "%d:%04.1f", mn, scs); else snprintf(t, sizeof(t), "%4.1f", scs);
    font.SetRect(dc.mLeft, topY, (int16)(dc.mWidth - pad), fh);
    font.SetAlignmentHor(FONT_ALIGN_RIGHT);
    font.RenderText(t);

    float z = a->GetTransform().GetTrans().z;
    float launchZ = PicaSim::GetInstance().GetObserver().GetTransform().GetTrans().z;
    snprintf(t, sizeof(t), "ALT %4.0f%s", GetDistance(options, z - launchZ), GetDistanceUnitText(options));
    font.SetRect((int16)(dc.mLeft + pad), botY, (int16)dc.mWidth, fh);
    font.SetAlignmentHor(FONT_ALIGN_LEFT);
    font.RenderText(t);

    float spd = a->GetVelocity().GetLength();
    snprintf(t, sizeof(t), "%4.0f%s", GetSpeed(options, spd), GetSpeedUnitText(options));
    font.SetRect(dc.mLeft, botY, (int16)(dc.mWidth - pad), fh);
    font.SetAlignmentHor(FONT_ALIGN_RIGHT);
    font.RenderText(t);

    font.SetColourABGR(origColour);
}

//======================================================================================================================
// One metric in the flight-data card: a dim label above a bright value.
struct FlightCell
{
    const char* label;
    char        value[24];
    uint32      labelCol;
    uint32      valueCol;
    int         reserve;   // fixed value-field width in chars (0 = size to the value)
};

// Fill a screen-space rectangle (x,y from the top-left, y growing down) with a
// solid colour, using the same ControllerShader path as the rest of the HUD.
static void FillHudRect(float x, float y, float w, float h, float r, float g, float b, float a, int viewportHeight)
{
    float glY0 = viewportHeight - (y + h);
    float glY1 = viewportHeight - y;
    GLfloat pts[] = {
        x,     glY0, 0,
        x + w, glY0, 0,
        x + w, glY1, 0,
        x,     glY1, 0,
    };
    const ControllerShader* shader = (ControllerShader*) ShaderManager::GetInstance().GetShader(SHADER_CONTROLLER);
    shader->Use();
    gStreamVBO.Bind();
    size_t off = gStreamVBO.Upload(pts, sizeof(pts));
    glVertexAttribPointer(shader->a_position, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)off);
    glEnableVertexAttribArray(shader->a_position);
    glUniform4f(shader->u_colour, r, g, b, a);
    esSetModelViewProjectionMatrix(shader->u_mvpMatrix);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDisableVertexAttribArray(shader->a_position);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Draw the flight data as a horizontal card of labelled cells, centred at the top
// (or bottom) of the display. A solid panel + amber top rule + thin cell dividers
// make it read clearly over any scenery.
static void DrawFlightDataCard(DisplayConfig& dc, const FlightCell* cells, int n, bool atTop)
{
    if (n <= 0)
        return;

    FontRenderer& font = FontRenderer::GetInstance();
    uint32 origColour = font.GetColourABGR();
    float fh = (float) font.GetFontHeight();
    const float charW   = fh * 0.5f;   // matches the HUD's own width estimate
    const float padX    = fh * 0.55f;  // panel inner horizontal padding
    const float padY    = fh * 0.5f;   // panel inner vertical padding
    const float cellGap = fh * 0.5f;   // space between cells
    const float cellPad = charW * 0.5f;// padding inside each cell
    const float rowGap  = fh * 0.15f;  // gap between label row and value row

    // Each cell reserves a fixed value-field width (cells[i].reserve) so its size -
    // and therefore the centred card's position - stays constant as the live values
    // change digit count. Without this the card resizes and slides every frame at
    // launch while the numbers ramp -> shake. reserve==0 sizes to the value (crash).
    float cellW[8];
    float contentW = 0.0f;
    for (int i = 0 ; i < n ; ++i)
    {
        int lc = (int) strlen(cells[i].label);
        int vc = cells[i].reserve > 0 ? cells[i].reserve : (int) strlen(cells[i].value);
        int mc = lc > vc ? lc : vc;
        cellW[i] = mc * charW + cellPad * 2.0f;
        contentW += cellW[i];
        if (i)
            contentW += cellGap;
    }

    float cardW = contentW + padX * 2.0f;
    float cardH = padY * 2.0f + fh * 2.0f + rowGap;
    float cardX = dc.mLeft + (dc.mWidth - cardW) * 0.5f;
    float cardY = atTop ? (dc.mBottom + fh * 0.5f)
                        : (dc.mBottom + dc.mHeight - cardH - fh * 0.6f);

    // Draw the panel and its text together with depth test off, so the 2D card
    // always overlays the scene and the glyphs sit on top of the panel.
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    int vh = vp[3];

    float labelY = cardY + padY;
    float valueY = labelY + fh + rowGap;

    {
        DisableDepthTest disableDepthTest;
        DisableDepthMask disableDepthMask;
        EnableBlend enableBlend;
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        float shadow = Maximum(3.0f, fh * 0.12f);
        FillHudRect(cardX + shadow, cardY + shadow, cardW, cardH, 0.0f, 0.0f, 0.0f, 0.35f, vh); // drop shadow
        FillHudRect(cardX, cardY, cardW, cardH, 0.07f, 0.08f, 0.10f, 0.82f, vh);                // panel
        FillHudRect(cardX, cardY, cardW, Maximum(2.0f, fh * 0.11f), 1.0f, 0.62f, 0.14f, 0.95f, vh); // amber top rule

        {
            float x = cardX + padX;
            for (int i = 0 ; i < n ; ++i)
            {
                if (i)
                    FillHudRect(x - cellGap * 0.5f, cardY + padY, 1.0f, fh * 2.0f + rowGap, 1.0f, 1.0f, 1.0f, 0.10f, vh);
                x += cellW[i] + cellGap;
            }
        }

        // --- text (still inside the depth-disabled scope) ---
        font.SetAlignmentHor(FONT_ALIGN_CENTRE);
        float x = cardX + padX;
        for (int i = 0 ; i < n ; ++i)
        {
            font.SetColourABGR(cells[i].labelCol);
            font.SetRect((int16)x, (int16)labelY, (int16)cellW[i], (int16)fh);
            font.RenderText(cells[i].label);

            font.SetColourABGR(cells[i].valueCol);
            font.SetRect((int16)x, (int16)valueY, (int16)cellW[i], (int16)fh);
            font.RenderText(cells[i].value);

            x += cellW[i] + cellGap;
        }
    }
    font.SetColourABGR(origColour);
}

void ChallengeFreeFly::GxRender(int renderLevel, DisplayConfig& displayConfig)
{
    TRACE_METHOD_ONLY(2);
    const GameSettings& gs = PicaSim::GetInstance().GetSettings();
    const Options& options = gs.mOptions;

    // FPV OSD when flying from the onboard (aeroplane) camera.
    if (PicaSim::GetInstance().GetShowUI())
    {
        const Camera* cam = PicaSim::GetInstance().GetMainViewport().GetCamera();
        if (cam && cam->GetUserData() == (void*) CAMERA_AEROPLANE)
        {
            const Aeroplane* a = dynamic_cast<const Aeroplane*>(cam->GetCameraTarget());
            if (!a) a = mAeroplane;
            if (a && !a->GetCrashed())
            {
                DrawFpvOsd(displayConfig, a, cam);
                return;
            }
        }
    }
    if (
        !options.mFreeFlightDisplayAltitude && 
        !options.mFreeFlightDisplayDistance && 
        !options.mFreeFlightDisplayAscentRate && 
        !options.mFreeFlightDisplaySpeed && 
        !options.mFreeFlightDisplayAirSpeed && 
        !options.mFreeFlightDisplayMaxSpeed && 
        !options.mFreeFlightDisplayTime &&
        !mAeroplane->GetCrashed())
    {
        return;
    }

    if (!PicaSim::GetInstance().GetShowUI())
        return;

    FontRenderer& font = FontRenderer::GetInstance();
    uint32 origColour = font.GetColourABGR();
    uint16 fontHeight = font.GetFontHeight();
    IwChar txt[64] = ""; // reused by the FPS readout below

    bool atTop = (PicaSim::GetInstance().GetStatus() == PicaSim::STATUS_FLYING && options.mFreeFlightTextAtTop);

    const CameraTarget* cameraTarget = PicaSim::GetInstance().GetMainViewport().GetCamera()->GetCameraTarget();
    const Aeroplane* aeroplane = dynamic_cast<const Aeroplane*>(cameraTarget);
    if (!aeroplane)
        aeroplane = dynamic_cast<const Aeroplane*>(PicaSim::GetInstance().GetMainViewport().GetCamera()->GetCameraTransform());
    if (!aeroplane)
        aeroplane = mAeroplane;

    // Build one cell per enabled metric, then draw them all as a single card.
    const uint32 kLabelCol = 0xffb9afaa; // dim warm grey (ABGR)
    const uint32 kValueCol = 0xffffffff; // white
    FlightCell cells[8];
    int numCells = 0;

    int crashFlags = aeroplane->GetCrashFlags();
    if (crashFlags)
    {
        Language language = options.mLanguage;
        FlightCell& c = cells[numCells++];
        c.label = TXT(PS_CRASHED);
        c.labelCol = 0xff3838ff; // red
        c.valueCol = 0xff5c5cff;
        c.reserve = 0; // size to the (static) crash text
        size_t vl = 0;
        c.value[0] = 0;
        if (crashFlags & Aeroplane::CRASHFLAG_AIRFRAME)
            vl += snprintf(c.value + vl, sizeof(c.value) - vl, "%s ", TXT(PS_AIRFRAME));
        if (vl < sizeof(c.value) && (crashFlags & Aeroplane::CRASHFLAG_PROPELLER))
            vl += snprintf(c.value + vl, sizeof(c.value) - vl, "%s ", TXT(PS_PROPELLER));
        if (vl < sizeof(c.value) && (crashFlags & Aeroplane::CRASHFLAG_UNDERCARRIAGE))
            vl += snprintf(c.value + vl, sizeof(c.value) - vl, "%s ", TXT(PS_UNDERCARRIAGE));
    }
    else
    {
        float ascentRate = aeroplane->GetVelocity().z;
        mSmoothedAscentRate += (ascentRate - mSmoothedAscentRate) * 0.01f;

        if (options.mFreeFlightDisplayTime)
        {
            float flightTime = aeroplane->GetFlightTime();
            int minutes = (int) (flightTime / 60.0f);
            float seconds = flightTime - minutes * 60.0f;
            FlightCell& c = cells[numCells++];
            c.label = "TIME"; c.labelCol = kLabelCol; c.valueCol = kValueCol; c.reserve = 6;
            if (minutes > 0)
                snprintf(c.value, sizeof(c.value), "%d:%04.1f", minutes, seconds);
            else
                snprintf(c.value, sizeof(c.value), "%.1fs", seconds);
        }
        if (numCells < 8 && options.mFreeFlightDisplaySpeed)
        {
            float speed = aeroplane->GetVelocity().GetLength();
            FlightCell& c = cells[numCells++];
            c.label = "GND"; c.labelCol = kLabelCol; c.valueCol = kValueCol; c.reserve = 9;
            snprintf(c.value, sizeof(c.value), "%.1f%s", GetSpeed(options, speed), GetSpeedUnitText(options));
        }
        if (numCells < 8 && options.mFreeFlightDisplayAirSpeed)
        {
            Vector3 windVel = Environment::GetInstance().GetWindAtPosition(aeroplane->GetTransform().GetTrans(), Environment::WIND_TYPE_SMOOTH | Environment::WIND_TYPE_GUSTY);
            float speed = (aeroplane->GetVelocity() - windVel).GetLength();
            FlightCell& c = cells[numCells++];
            c.label = "AIR"; c.labelCol = kLabelCol; c.valueCol = kValueCol; c.reserve = 9;
            snprintf(c.value, sizeof(c.value), "%.1f%s", GetSpeed(options, speed), GetSpeedUnitText(options));
        }
        if (numCells < 8 && options.mFreeFlightDisplayMaxSpeed)
        {
            FlightCell& c = cells[numCells++];
            c.label = "MAX"; c.labelCol = kLabelCol; c.valueCol = kValueCol; c.reserve = 9;
            snprintf(c.value, sizeof(c.value), "%.1f%s", GetSpeed(options, mMaxSpeed), GetSpeedUnitText(options));
        }
        if (numCells < 8 && options.mFreeFlightDisplayAscentRate)
        {
            FlightCell& c = cells[numCells++];
            c.label = "VARIO"; c.labelCol = kLabelCol; c.reserve = 8;
            snprintf(c.value, sizeof(c.value), "%.2f%s", GetAscentRate(options, ascentRate), GetAscentRateUnitText(options));
            if (options.mFreeFlightColourText)
            {
                // 1 is up, 0 is down -> red..green
                float aa = ClampToRange(0.5f + ascentRate / 12.0f, 0.0f, 1.0f);
                int r = ClampToRange((int) ((1.0f - aa) * 255.0f), 0, 255);
                int g = ClampToRange((int) (aa * 255.0f), 0, 255);
                c.valueCol = 0xff000000 | (g << 8) | r;
            }
            else
            {
                c.valueCol = kValueCol;
            }
        }
        if (numCells < 8 && options.mFreeFlightDisplayAltitude)
        {
            float z = aeroplane->GetTransform().GetTrans().z;
            float launchZ = PicaSim::GetInstance().GetObserver().GetTransform().GetTrans().z;
            FlightCell& c = cells[numCells++];
            c.label = "ALT"; c.labelCol = kLabelCol; c.valueCol = kValueCol; c.reserve = 7;
            snprintf(c.value, sizeof(c.value), "%.1f%s", GetDistance(options, z - launchZ), GetDistanceUnitText(options));
        }
        if (numCells < 8 && options.mFreeFlightDisplayDistance)
        {
            Vector3 delta = aeroplane->GetTransform().GetTrans() - PicaSim::GetInstance().GetObserver().GetTransform().GetTrans();
            FlightCell& c = cells[numCells++];
            c.label = "DIST"; c.labelCol = kLabelCol; c.valueCol = kValueCol; c.reserve = 8;
            snprintf(c.value, sizeof(c.value), "%.1f%s", GetDistance(options, delta.GetLength()), GetDistanceUnitText(options));
        }
    }

    DrawFlightDataCard(displayConfig, cells, numCells, atTop);

    if (gs.mOptions.mDisplayFPS)
    {
        font.SetRect(displayConfig.mLeft,(int16)(displayConfig.mBottom + displayConfig.mHeight - fontHeight*5/4),(int16)displayConfig.mWidth,fontHeight);
        font.SetAlignmentHor(FONT_ALIGN_RIGHT);
        sprintf(txt, "%d", (int) (gs.mStatistics.mSmoothedFPS + 0.5f));
        font.SetColourABGR(0xff00ffff);
        font.RenderText(txt);
    }

    if (mNeedToCacheText)
    {
        font.SetColourABGR(0x00ffffff);
        font.RenderText("0123456789.");
        mNeedToCacheText = false;
    }

    font.SetColourABGR(origColour);
}

