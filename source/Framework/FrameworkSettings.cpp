#include "FrameworkSettings.h"
#include "Trace.h"
#include "Helpers.h"
#include "../Platform/Window.h"

#include <cstring>
#include <algorithm>

// External window reference
extern Window* gWindow;

//======================================================================================================================
FrameworkSettings::FrameworkSettings()
    :
    mPhysicsSubsteps(8),
    mCrashDamage(false),
    mNearClipPlaneDistance(0.5f),
    mFarClipPlaneDistance(50000.0f),
    mUseMultiLights(true),
    mClassicRendering(false),
    mBloomEnabled(false),
    mBloomIntensity(0.15f),
    mExposure(1.0f),
    mFXAAEnabled(false),
    mPBRTonemap(false),
    mSSAO(false),
    mUsePBR(true),
    mSHAmbientScale(1.0f),
    mShadowMode(1),        // 1 = Blob (legacy default). 2 = CSM (opt-in).
    mCsmBias(0.0015f),
    mAnisotropy(1),        // 1 = off; raised by the graphics-quality tier.
    mEnhancedWater(false)  // opt-in; default OFF keeps existing water unchanged.
{
    // Get platform type
    mPlatform = Platform::GetPlatformID();
    TRACE_FILE_IF(1) TRACE("Platform = %s", Platform::GetPlatformName());

    UpdateScreenDimensions();
}

//======================================================================================================================
void FrameworkSettings::UpdateScreenDimensions() const
{
    // Get screen dimensions from Window if available, otherwise from Platform
    if (gWindow && gWindow->IsInitialized())
    {
        mScreenWidth = gWindow->GetWidth();
        mScreenHeight = gWindow->GetHeight();
    }
    else
    {
        mScreenWidth = Platform::GetDisplayWidth();
        mScreenHeight = Platform::GetDisplayHeight();
    }

    // On iOS, ensure landscape orientation (width > height)
    if (mPlatform == Platform::PlatformID::iOS && mScreenHeight > mScreenWidth)
    {
        std::swap(mScreenWidth, mScreenHeight);
    }
}
