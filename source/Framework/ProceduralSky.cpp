#include "ProceduralSky.h"

#include "RenderManager.h"
#include "ShaderManager.h"
#include "Shaders.h"
#include "Viewport.h"
#include "Camera.h"
#include "Graphics.h"
#include "Trace.h"

#include <glm/glm.hpp>
#include <cmath>
#include <cstring>

//======================================================================================================================
ProceduralSky::ProceduralSky()
    : mSunDirWorld(0, 0, 1)
    , mSunElevationDeg(45.0f)
    , mSunAzimuthDeg(180.0f)
    , mLastSunTheta(-1000.0f)
    , mSkyBrightness(1.0f)
    , mTime(0.0f)
    , mForceRecompute(true)
    , mInitialised(false)
{
    SkyModel_Init(mSkyState, mParams.mTurbidity, DegreesToRadians(45.0f));
}

//======================================================================================================================
ProceduralSky::~ProceduralSky()
{
}

//======================================================================================================================
bool ProceduralSky::Init(const Params& params)
{
    TRACE_METHOD_ONLY(1);
    if (mInitialised)
        Terminate();

    mParams = params;
    mForceRecompute = true;
    mTime = 0.0f;

    RecomputeSun();

    RenderManager::GetInstance().RegisterRenderObject(this, RENDER_LEVEL_SKYBOX);
    mInitialised = true;
    return true;
}

//======================================================================================================================
void ProceduralSky::Terminate()
{
    if (mInitialised)
        RenderManager::GetInstance().UnregisterRenderObject(this, RENDER_LEVEL_SKYBOX);
    mInitialised = false;
}

//======================================================================================================================
// Simple solar-position model. declination is left at the equinox value (0) -
// good enough for a time-of-day demo; latitude and the hour angle drive the arc.
static void ComputeSunAngles(float timeOfDay, float latDeg, float& elevationDeg, float& azimuthDeg)
{
    const float declDeg = 0.0f;
    const float H  = (timeOfDay - 12.0f) * 15.0f;          // hour angle, degrees
    const float lat  = DegreesToRadians(latDeg);
    const float decl = DegreesToRadians(declDeg);
    const float Hr = DegreesToRadians(H);

    float sinEl = sinf(lat) * sinf(decl) + cosf(lat) * cosf(decl) * cosf(Hr);
    sinEl = ClampToRange(sinEl, -1.0f, 1.0f);
    const float el = asinf(sinEl);

    float cosAz = (sinf(decl) - sinEl * sinf(lat)) / (cosf(el) * cosf(lat) + 1e-5f);
    cosAz = ClampToRange(cosAz, -1.0f, 1.0f);
    float az = acosf(cosAz);           // measured from due north (0), through east
    if (Hr > 0.0f)                     // afternoon -> swing to the west
        az = TWO_PI - az;

    elevationDeg = RadiansToDegrees(el);
    azimuthDeg   = RadiansToDegrees(az);
}

//======================================================================================================================
void ProceduralSky::RecomputeSun()
{
    ComputeSunAngles(mParams.mTimeOfDay, mParams.mLatitude, mSunElevationDeg, mSunAzimuthDeg);

    RenderManager& rm = RenderManager::GetInstance();

    // Drive the SAME lighting-direction machinery the photo environments use, so
    // PBR + CSM follow the sun. mLightingDirection is the direction light travels;
    // the vector towards the sun is its negation.
    rm.SetLightingDirection(mSunAzimuthDeg, mSunElevationDeg);
    mSunDirWorld = -rm.GetLightingDirection();
    mSunDirWorld.Normalise();

    // Sun colour: dim & warm near / below the horizon, bright white when high.
    // t: 0 at horizon, 1 high in the sky.
    float elev01 = ClampToRange(mSunElevationDeg / 60.0f, 0.0f, 1.0f);
    Vector3 warm(1.0f, 0.45f, 0.20f);
    Vector3 white(1.0f, 0.98f, 0.95f);
    Vector3 sunColour = warm + (white - warm) * elev01;

    // Intensity fades out as the sun sets (and stays low just below the horizon
    // for a dusk feel).
    float sunIntensity = ClampToRange((mSunElevationDeg + 3.0f) / 12.0f, 0.0f, 1.0f);
    sunIntensity = sunIntensity * sunIntensity * (3.0f - 2.0f * sunIntensity); // smoothstep
    Vector3 diffuse = sunColour * (0.15f + 1.05f * sunIntensity);

    // Ambient (sky fill): bluish, brighter when the sun is up. Turbidity/albedo
    // lift the floor a little (hazier air / brighter ground scatters more fill).
    float ambientLevel = 0.12f + 0.35f * sunIntensity;
    Vector3 skyAmbient(0.55f, 0.70f, 1.0f);
    ambientLevel *= (1.0f + 0.15f * (mParams.mTurbidity - 3.0f) / 7.0f);
    ambientLevel += 0.10f * mParams.mGroundAlbedo;
    Vector3 ambient = skyAmbient * ambientLevel;

    rm.SetLightingDiffuseColour(diffuse);
    rm.SetLightingAmbientColour(ambient);

    // Overall sky brightness fades toward dusk so the atmosphere darkens with the
    // sun rather than staying fully lit.
    mSkyBrightness = 0.15f + 0.85f * sunIntensity;

    // Recompute the (relatively expensive) Perez coefficients only when the sun
    // has moved appreciably.
    float sunTheta = DegreesToRadians(90.0f - mSunElevationDeg);
    sunTheta = ClampToRange(sunTheta, 0.0f, DegreesToRadians(93.0f));
    if (mForceRecompute || fabsf(sunTheta - mLastSunTheta) > DegreesToRadians(0.5f))
    {
        SkyModel_Init(mSkyState, mParams.mTurbidity, sunTheta);
        mLastSunTheta = sunTheta;
        mForceRecompute = false;
    }
}

//======================================================================================================================
void ProceduralSky::Update(float deltaTime)
{
    mTime += deltaTime;
    if (mParams.mTimeScale != 0.0f)
    {
        mParams.mTimeOfDay += mParams.mTimeScale * deltaTime;
        while (mParams.mTimeOfDay >= 24.0f) mParams.mTimeOfDay -= 24.0f;
        while (mParams.mTimeOfDay < 0.0f)   mParams.mTimeOfDay += 24.0f;
    }
    RecomputeSun();
}

//======================================================================================================================
void ProceduralSky::RenderUpdate(class Viewport* viewport, int renderLevel)
{
    TRACE_METHOD_ONLY(2);

    const ProceduralSkyShader* shader =
        (const ProceduralSkyShader*) ShaderManager::GetInstance().GetShader(SHADER_SKY_HOSEK);
    shader->Use();

    // Reconstruct a world-space ray direction per fragment from the inverse of
    // the rotation-only view-projection (translation removed, so the result is a
    // pure direction independent of the camera position).
    GLMat44 viewM; esGetMatrix(viewM, GL_MODELVIEW);
    GLMat44 projM; esGetMatrix(projM, GL_PROJECTION);

    glm::mat4 view, proj;
    memcpy(&view[0][0], viewM, sizeof(GLMat44));
    memcpy(&proj[0][0], projM, sizeof(GLMat44));
    view[3][0] = view[3][1] = view[3][2] = 0.0f;   // strip translation
    glm::mat4 invVP = glm::inverse(proj * view);

    glUniformMatrix4fv(shader->u_invViewProjRot, 1, GL_FALSE, &invVP[0][0]);
    glUniform3f(shader->u_sunDir, mSunDirWorld.x, mSunDirWorld.y, mSunDirWorld.z);

    glUniform1fv(shader->u_perezY, 5, mSkyState.perez[0]);
    glUniform1fv(shader->u_perezx, 5, mSkyState.perez[1]);
    glUniform1fv(shader->u_perezy, 5, mSkyState.perez[2]);
    glUniform3f(shader->u_zenith, mSkyState.zenith[0], mSkyState.zenith[1], mSkyState.zenith[2]);
    glUniform1f(shader->u_sunTheta, mSkyState.sunTheta);
    glUniform1f(shader->u_skyBrightness, mSkyBrightness);
    glUniform1f(shader->u_cloudCover, mParams.mCloudCover);
    glUniform1f(shader->u_time, mTime);

    // Sky writes colour everywhere but no depth: terrain/objects drawn later
    // (with depth testing on) overwrite it, exactly like the photo Skybox.
    DisableDepthMask disableDepthMask;
    DisableDepthTest disableDepthTest;
    DisableFog       disableFog;

    // Fullscreen triangle from gl_VertexID (relies on the app-wide bound VAO,
    // same as the post-process passes).
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
