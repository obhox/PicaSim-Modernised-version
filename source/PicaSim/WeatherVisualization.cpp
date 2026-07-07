#include "WeatherVisualization.h"

#include "PicaSim.h"
#include "GameSettings.h"
#include "Environment.h"
#include "ThermalManager.h"

#include "RenderManager.h"
#include "RenderObject.h"
#include "Viewport.h"
#include "Camera.h"
#include "Trace.h"
#include "Graphics.h"
#include "ShaderManager.h"
#include "Shaders.h"
#include "../Platform/S3ECompat.h"

#include <vector>
#include <cmath>

//======================================================================================================================
// Maps a wind speed to a blue(slow) -> green -> red(fast) colour with the given alpha.
static Vector4 SpeedColour(float speed, float refSpeed, float alpha)
{
    float t = ClampToRange(speed / refSpeed, 0.0f, 1.0f);
    float r, g, b;
    if (t < 0.5f)
    {
        float u = t * 2.0f;         // 0..1
        r = 0.0f; g = u; b = 1.0f - u;
    }
    else
    {
        float u = (t - 0.5f) * 2.0f; // 0..1
        r = u; g = 1.0f - u; b = 0.0f;
    }
    return Vector4(r, g, b, alpha);
}

//======================================================================================================================
void WeatherVisualization::Init()
{
    TRACE_FUNCTION_ONLY(1);
    // RENDER_LEVEL_OBJECTS is after the terrain (RENDER_LEVEL_TERRAIN) so the
    // overlay is correctly depth-occluded by the ground, and before the debug
    // level. Matches SkyGrid.
    RenderManager::GetInstance().RegisterRenderObject(this, RENDER_LEVEL_OBJECTS);
}

//======================================================================================================================
void WeatherVisualization::Terminate()
{
    TRACE_FUNCTION_ONLY(1);
    RenderManager::GetInstance().UnregisterRenderObject(this, RENDER_LEVEL_OBJECTS);
}

//======================================================================================================================
void WeatherVisualization::AddSegment(const Vector3& a, const Vector3& b, const Vector4& colA, const Vector4& colB)
{
    mPoints.push_back(a);
    mPoints.push_back(b);
    mColours.push_back(colA);
    mColours.push_back(colB);
}

//======================================================================================================================
void WeatherVisualization::AddRing(const Vector3& centre, float radius, const Vector4& col, int divisions)
{
    Vector3 prev(centre.x + radius, centre.y, centre.z);
    for (int i = 1; i <= divisions; ++i)
    {
        float ang = TWO_PI * float(i) / divisions;
        Vector3 pos(centre.x + radius * FastCos(ang), centre.y + radius * FastSin(ang), centre.z);
        AddSegment(prev, pos, col, col);
        prev = pos;
    }
}

//======================================================================================================================
void WeatherVisualization::Flush(bool additive)
{
    const size_t n = mPoints.size();
    if (n == 0)
        return;

    EnableBlend enableBlend;
    if (additive)
        glBlendFunc(GL_ONE, GL_ONE);
    else
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const SimpleShader* shader = (SimpleShader*) ShaderManager::GetInstance().GetShader(SHADER_SIMPLE);
    shader->Use();

    // Positions are already in world space; the current modelview is the pure
    // camera view, so MVP = proj * view transforms world -> clip directly.
    esSetModelViewProjectionMatrix(shader->u_mvpMatrix);

    gStreamVBO.Bind();
    gStreamVBO.Reserve(n * sizeof(Vector3) + n * sizeof(Vector4));
    size_t posOffset    = gStreamVBO.Upload(&mPoints[0].x,  n * sizeof(Vector3));
    size_t colourOffset = gStreamVBO.Upload(&mColours[0].x, n * sizeof(Vector4));
    glEnableVertexAttribArray(shader->a_position);
    glEnableVertexAttribArray(shader->a_colour);
    glVertexAttribPointer(shader->a_position, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)posOffset);
    glVertexAttribPointer(shader->a_colour,   4, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)colourOffset);

    glDrawArrays(GL_LINES, 0, (GLsizei)n);

    glDisableVertexAttribArray(shader->a_position);
    glDisableVertexAttribArray(shader->a_colour);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Restore the engine's default blend mode.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mPoints.clear();
    mColours.clear();
}

//======================================================================================================================
void WeatherVisualization::BuildWindStreamlines(const Vector3& centre)
{
    Environment& env = Environment::GetInstance();

    const int   N        = 6;      // seeds per axis (N^3 seeds total, bounded)
    const float spacing  = 7.0f;   // metres between seeds
    const float half     = 0.5f * (N - 1) * spacing;
    const int   numSteps = 12;     // integration steps per streamline
    const float dt       = 0.4f;   // integration time step (s)
    const float refSpeed = 12.0f;  // wind speed mapped to "fast" (red)
    const float maxAlpha = 0.55f;

    for (int ix = 0; ix < N; ++ix)
    for (int iy = 0; iy < N; ++iy)
    for (int iz = 0; iz < N; ++iz)
    {
        Vector3 seed(centre.x - half + ix * spacing,
                     centre.y - half + iy * spacing,
                     centre.z - half + iz * spacing);

        float terrainH = Environment::GetTerrain().GetTerrainHeightFast(seed.x, seed.y, true);
        if (seed.z < terrainH + 0.5f)
            continue;

        // Prepare turbulence data once per streamline (as the physics does per
        // plane per frame) so the query exactly matches the real air velocity.
        TurbulenceData turb = env.PrepareTurbulenceData(seed);

        Vector3 pos = seed;
        for (int s = 0; s < numSteps; ++s)
        {
            // RK2 (midpoint) integration through the real wind field.
            Vector3 w1  = env.GetWindAtPosition(pos, Environment::WIND_TYPE_ALL, &turb);
            Vector3 mid = pos + w1 * (0.5f * dt);
            Vector3 w2  = env.GetWindAtPosition(mid, Environment::WIND_TYPE_ALL, &turb);
            Vector3 next = pos + w2 * dt;

            float speed = w2.GetLength();
            float aStart = maxAlpha * (1.0f - float(s)     / numSteps);
            float aEnd   = maxAlpha * (1.0f - float(s + 1) / numSteps);
            AddSegment(pos, next,
                       SpeedColour(speed, refSpeed, aStart),
                       SpeedColour(speed, refSpeed, aEnd));
            pos = next;
        }
    }

    Flush(false);
}

//======================================================================================================================
void WeatherVisualization::BuildThermals(Viewport* /*viewport*/)
{
    Environment& env = Environment::GetInstance();

    std::vector<ThermalManager::ThermalVisInfo> thermals;
    env.GetThermalManager().GetThermalVisInfo(thermals);
    if (thermals.empty())
        return;

    const float time     = env.GetTime();
    const int   ringDivs = 24;
    const int   numRings = 8;

    for (size_t i = 0; i < thermals.size(); ++i)
    {
        const ThermalManager::ThermalVisInfo& t = thermals[i];

        float terrainH = Environment::GetTerrain().GetTerrainHeightFast(t.mPos.x, t.mPos.y, true);
        float baseZ = terrainH;
        float topZ  = Maximum(t.mPos.z + t.mDepth, terrainH + t.mDepth);
        float span  = topZ - baseZ;
        if (span < 1.0f)
            continue;

        // Warm/orange tint, more opaque for more-active thermals.
        float a = 0.12f + 0.18f * ClampToRange(t.mActivity, 0.0f, 1.0f);
        Vector4 coreCol (1.0f, 0.55f, 0.12f, a);
        Vector4 outerCol(1.0f, 0.35f, 0.08f, a * 0.5f);

        // Stacked rings on the core and outer radii give a cylinder impression.
        for (int r = 0; r <= numRings; ++r)
        {
            float z = baseZ + span * (float(r) / numRings);
            AddRing(Vector3(t.mPos.x, t.mPos.y, z), t.mCoreRadius,  coreCol,  ringDivs);
            AddRing(Vector3(t.mPos.x, t.mPos.y, z), t.mOuterRadius, outerCol, ringDivs);
        }

        // Vertical struts on the core radius.
        const int struts = 8;
        for (int s = 0; s < struts; ++s)
        {
            float ang = TWO_PI * s / struts;
            float c = FastCos(ang), sn = FastSin(ang);
            Vector3 b (t.mPos.x + c * t.mCoreRadius, t.mPos.y + sn * t.mCoreRadius, baseZ);
            Vector3 tp(t.mPos.x + c * t.mCoreRadius, t.mPos.y + sn * t.mCoreRadius, topZ);
            AddSegment(b, tp, coreCol, coreCol);
        }

        // A few chevron markers slowly rising inside the core to show the updraft.
        const int numMarkers = 6;
        float rise = Maximum(t.mUpdraftSpeed, 1.0f);
        Vector4 mc(1.0f, 0.8f, 0.3f, 0.6f);
        for (int m = 0; m < numMarkers; ++m)
        {
            float phase = float(m) / numMarkers;
            float h = fmodf(time * rise * 0.15f + phase * span, span);
            float z = baseZ + h;
            float mr  = t.mCoreRadius * 0.4f;
            float ang = phase * TWO_PI + time * 0.5f;
            Vector3 p(t.mPos.x + FastCos(ang) * mr, t.mPos.y + FastSin(ang) * mr, z);
            AddSegment(p + Vector3(-0.6f, 0.0f, -0.6f), p, mc, mc);
            AddSegment(p + Vector3( 0.6f, 0.0f, -0.6f), p, mc, mc);
        }
    }

    Flush(false);
}

//======================================================================================================================
void WeatherVisualization::BuildTurbulence(const Vector3& centre)
{
    Environment& env = Environment::GetInstance();

    const int   N         = 6;
    const float spacing   = 8.0f;
    const float half      = 0.5f * (N - 1) * spacing;
    const float time      = env.GetTime();
    const float threshold = 0.6f; // m/s of turbulent delta before we show anything

    for (int ix = 0; ix < N; ++ix)
    for (int iy = 0; iy < N; ++iy)
    for (int iz = 0; iz < N; ++iz)
    {
        Vector3 p(centre.x - half + ix * spacing,
                  centre.y - half + iy * spacing,
                  centre.z - half + iz * spacing);

        float terrainH = Environment::GetTerrain().GetTerrainHeightFast(p.x, p.y, true);
        if (p.z < terrainH + 0.5f)
            continue;

        // Turbulent/gusty component = full field minus the smooth (mean) field.
        TurbulenceData turb = env.PrepareTurbulenceData(p);
        Vector3 smooth = env.GetWindAtPosition(p, Environment::WIND_TYPE_SMOOTH | Environment::WIND_TYPE_THERMALS);
        Vector3 full   = env.GetWindAtPosition(p, Environment::WIND_TYPE_ALL, &turb);
        float mag = (full - smooth).GetLength();
        if (mag < threshold)
            continue;

        float intensity = ClampToRange((mag - threshold) / 3.0f, 0.0f, 1.0f);
        float sz = 0.4f + 1.6f * intensity;

        // Deterministic pseudo-random jitter from position + time.
        float j = FastSin(p.x * 12.9898f + p.y * 78.233f + p.z * 37.719f + time * 3.0f);
        float k = FastCos(p.x * 39.346f  + p.y * 11.135f + p.z * 83.155f + time * 2.3f);
        Vector3 c = p + Vector3(j * sz * 0.5f, k * sz * 0.5f, (j * k) * sz * 0.5f);

        Vector4 col(1.0f, 0.9f, 0.2f, 0.25f + 0.4f * intensity);
        AddSegment(c - Vector3(sz, 0, 0), c + Vector3(sz, 0, 0), col, col);
        AddSegment(c - Vector3(0, sz, 0), c + Vector3(0, sz, 0), col, col);
        AddSegment(c - Vector3(0, 0, sz), c + Vector3(0, 0, sz), col, col);
    }

    Flush(false);
}

//======================================================================================================================
void WeatherVisualization::RenderUpdate(Viewport* viewport, int /*renderLevel*/)
{
    TRACE_METHOD_ONLY(2);

    const Options& options = PicaSim::GetInstance().GetSettings().mOptions;

    // Default OFF: with all aids disabled nothing is built or drawn, so there is
    // zero behavioural/visual change and zero cost.
    if (!options.mShowWindStreamlines && !options.mShowThermals && !options.mShowTurbulence)
        return;

    const Camera& camera = *viewport->GetCamera();

    // Centre the sampling volume on what the camera is looking at (the plane in
    // most modes), falling back to the camera position.
    Vector3 centre = camera.GetTargetPosition();
    if (!(centre == centre)) // NaN guard
        centre = camera.GetPosition();

    // Overlay: keep depth test on (so terrain occludes) but do not write depth.
    DisableDepthMask disableDepthMask;

    if (options.mShowThermals)
        BuildThermals(viewport);

    if (options.mShowWindStreamlines)
        BuildWindStreamlines(centre);

    if (options.mShowTurbulence)
        BuildTurbulence(centre);
}
