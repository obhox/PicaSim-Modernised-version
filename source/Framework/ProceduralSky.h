#ifndef PROCEDURALSKY_H
#define PROCEDURALSKY_H

#include "Helpers.h"
#include "RenderObject.h"
#include "ArHosekSkyModel.h"

// Procedural "dynamic sky" for the new DynamicSky environment type. Renders an
// analytic Preetham/Hosek-Wilkie-style atmosphere as a fullscreen pass into the
// HDR scene target (drawn at RENDER_LEVEL_SKYBOX, in place of the photo Skybox),
// with a movable sun / time-of-day that also drives the scene lighting so PBR
// and CSM automatically track the sun.
//
// The 7 photo-panorama environments never construct this class; their skybox
// code path is untouched.
class ProceduralSky : public RenderObject
{
public:
    struct Params
    {
        float mTimeOfDay;    // hours, 0..24 (local solar time)
        float mTimeScale;    // hours advanced per real second (0 = frozen sun)
        float mLatitude;     // degrees
        float mTurbidity;    // 1 (clear) .. ~10 (hazy)
        float mCloudCover;   // 0 (clear) .. 1 (overcast cirrus)
        float mGroundAlbedo; // 0..1 (currently only a small horizon-fill hint)

        Params()
            : mTimeOfDay(14.0f), mTimeScale(0.0f), mLatitude(45.0f),
              mTurbidity(3.0f), mCloudCover(0.0f), mGroundAlbedo(0.1f) {}
    };

    ProceduralSky();
    ~ProceduralSky();

    // Registers this object at RENDER_LEVEL_SKYBOX.
    bool Init(const Params& params);
    void Terminate();

    // Advances the sun by mTimeScale*deltaTime and pushes the resulting sun
    // direction / diffuse (sun) colour / ambient (sky) colour into RenderManager
    // (i.e. into mLightingDirection / mLightingDiffuseColour /
    // mLightingAmbientColour) so the rest of the scene tracks the sun. Recomputes
    // the (cached) sky coefficients only when the sun has moved appreciably.
    void Update(float deltaTime);

    void RenderUpdate(class Viewport* viewport, int renderLevel) OVERRIDE;
    float GetRenderBoundingRadius() const OVERRIDE { return FLT_MAX; }

    // Runtime-adjustable time-of-day (for a future UI slider).
    void  SetTimeOfDay(float hours) { mParams.mTimeOfDay = hours; mForceRecompute = true; }
    float GetTimeOfDay() const { return mParams.mTimeOfDay; }

private:
    void RecomputeSun();

    Params mParams;
    SkyModelState mSkyState;

    Vector3 mSunDirWorld;    // unit vector pointing TOWARDS the sun (world space)
    float   mSunElevationDeg;
    float   mSunAzimuthDeg;
    float   mLastSunTheta;   // for the recompute cache
    float   mSkyBrightness;  // overall exposure-ish scale fed to the shader
    float   mTime;           // accumulated seconds (cirrus scroll)
    bool    mForceRecompute;
    bool    mInitialised;
};

#endif
