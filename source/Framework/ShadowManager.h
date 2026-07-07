#ifndef SHADOWMANAGER_H
#define SHADOWMANAGER_H

#include "Graphics.h"
#include "Helpers.h"

#include <glm/glm.hpp>

// ShadowManager owns the Cascaded Shadow Map (CSM) resources and drives the
// depth-only caster pass. It is only used when FrameworkSettings::mShadowMode == 2
// (opt-in). See ShadowManager.cpp and data/SystemData/Shaders/csm helpers for the
// full pipeline description.
//
// Resources: one GL_DEPTH_COMPONENT24 GL_TEXTURE_2D_ARRAY with NUM_CASCADES
// layers, SHADOW_SIZE x SHADOW_SIZE each, configured for hardware PCF
// (GL_COMPARE_REF_TO_TEXTURE), plus one FBO whose depth attachment is switched
// between array layers with glFramebufferTextureLayer.
class ShadowManager
{
public:
    static const int NUM_CASCADES = 3;
    static const int SHADOW_SIZE  = 2048;

    ShadowManager();
    ~ShadowManager();

    // Lazily creates the depth array + FBO. Returns false if unavailable.
    bool EnsureResources();

    // Splits the camera frustum into NUM_CASCADES and fits a tight, texel-snapped
    // ortho light matrix to each. view/proj are STANDARD (glm) camera matrices;
    // sunDir is the world-space direction the sunlight TRAVELS in (== RenderManager
    // mLightingDirection). camNear/shadowMaxDist bound the shadowed depth range.
    void ComputeCascades(const glm::mat4& view, const glm::mat4& proj,
                         const Vector3& sunDir, float camNear, float shadowMaxDist);

    // Renders each cascade: binds the FBO/layer, clears depth, applies depth bias
    // and invokes the caster callback (which should draw ONLY dynamic model
    // casters via the shadowcast program). Uses the engine matrix stack so casters
    // render exactly as normal. Restores the previous FBO + viewport afterwards.
    typedef void (*CasterCallback)(void* user);
    void RenderCascades(CasterCallback callback, void* user);

    GLuint           GetTextureArray() const   { return mDepthArray; }
    const glm::mat4& GetCascadeViewProj(int i) const { return mCascadeVP[i]; }
    float            GetSplitDistance(int i) const   { return mSplits[i]; }

    // Tuning knobs (see ShadowManager.cpp for defaults / meaning).
    float mCascadeLambda;      // practical-split blend (0 = uniform, 1 = log)
    float mPolygonOffsetFactor;
    float mPolygonOffsetUnits;
    bool  mCullFrontFaces;     // front-face culling in the caster pass

private:
    void Destroy();

    bool      mInitialised;
    GLuint    mFBO;
    GLuint    mDepthArray;
    glm::mat4 mCascadeVP[NUM_CASCADES];
    float     mSplits[NUM_CASCADES];
};

#endif // SHADOWMANAGER_H
