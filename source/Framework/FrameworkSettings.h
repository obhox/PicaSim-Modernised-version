#ifndef FRAMEWORKSETTINGS_H
#define FRAMEWORKSETTINGS_H

#include "../Platform/Platform.h"

struct FrameworkSettings
{
    FrameworkSettings();

    // Obviously not really const, but sometimes this just needs to be updated before things can work with it
    void UpdateScreenDimensions() const;

    bool isWindows() const { return mPlatform == Platform::PlatformID::Windows; }
    bool isAndroid() const { return mPlatform == Platform::PlatformID::Android; }
    bool isIOS() const { return mPlatform == Platform::PlatformID::iOS; }
    bool isMacOS() const { return mPlatform == Platform::PlatformID::macOS; }
    bool isLinux() const { return mPlatform == Platform::PlatformID::Linux; }
    bool isDesktop() const { return Platform::IsDesktop(); }
    bool isMobile() const { return Platform::IsMobile(); }

    int mPhysicsSubsteps;
    float mNearClipPlaneDistance;
    float mFarClipPlaneDistance;
    bool  mUseMultiLights;

    // HDR / post-processing pipeline (Phase 1c). When mClassicRendering is true
    // the 3D scene is drawn straight to the default framebuffer as it always was
    // (the safety escape hatch / low-end tier). Otherwise the scene is rendered
    // to a floating-point HDR target and resolved through PostProcess (tonemap,
    // optional bloom / FXAA). Bloom is OFF by default so the output stays
    // near-identical to the classic path.
    bool  mClassicRendering;
    bool  mBloomEnabled;
    float mBloomIntensity;
    float mExposure;
    bool  mFXAAEnabled;
    bool  mPBRTonemap;   // false => identity clamp (preserve LDR look); true => PBR Neutral tonemap

    // PBR-lite aircraft-model shading (Cook-Torrance GGX + SH ambient). When
    // false the models fall back to the legacy Phong path. Forced off when
    // mClassicRendering is set. mSHAmbientScale tunes the environment-ambient
    // brightness (1.0 ~= the old flat fill).
    bool  mUsePBR;
    float mSHAmbientScale;

    Platform::PlatformID mPlatform;
    mutable int mScreenWidth, mScreenHeight;
};

#endif
