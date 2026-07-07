#include "ShadowManager.h"
#include "Shaders.h"
#include "ShaderManager.h"
#include "Trace.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif
#ifndef GL_TEXTURE_2D_ARRAY
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#endif
#ifndef GL_TEXTURE_COMPARE_MODE
#define GL_TEXTURE_COMPARE_MODE 0x884C
#endif
#ifndef GL_TEXTURE_COMPARE_FUNC
#define GL_TEXTURE_COMPARE_FUNC 0x884D
#endif
#ifndef GL_COMPARE_REF_TO_TEXTURE
#define GL_COMPARE_REF_TO_TEXTURE 0x884E
#endif

//======================================================================================================================
ShadowManager::ShadowManager()
    : mCascadeLambda(0.7f)
    , mPolygonOffsetFactor(2.5f)
    , mPolygonOffsetUnits(4.0f)
    , mCullFrontFaces(false)   // open/thin geometry (wings) - default no cull + bias
    , mInitialised(false)
    , mFBO(0)
    , mDepthArray(0)
{
    for (int i = 0; i < NUM_CASCADES; ++i)
    {
        mCascadeVP[i] = glm::mat4(1.0f);
        mSplits[i] = 0.0f;
    }
}

//======================================================================================================================
ShadowManager::~ShadowManager()
{
    Destroy();
}

//======================================================================================================================
void ShadowManager::Destroy()
{
    if (mDepthArray) { glDeleteTextures(1, &mDepthArray); mDepthArray = 0; }
    if (mFBO)        { glDeleteFramebuffers(1, &mFBO); mFBO = 0; }
    mInitialised = false;
}

//======================================================================================================================
bool ShadowManager::EnsureResources()
{
    if (mInitialised)
        return true;

    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

    glGenTextures(1, &mDepthArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, mDepthArray);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24,
                 SHADOW_SIZE, SHADOW_SIZE, NUM_CASCADES, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Hardware PCF: sampler2DArrayShadow compares the ref depth against the texel.
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    glGenFramebuffers(1, &mFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    // Attach layer 0 initially just to validate completeness.
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, mDepthArray, 0, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        TRACE_FILE_IF(1) TRACE("ShadowManager: FBO incomplete (0x%x) - CSM unavailable", status);
        Destroy();
        return false;
    }

    TRACE_FILE_IF(1) TRACE("ShadowManager: %d x %dx%d depth-array cascades ready",
                           NUM_CASCADES, SHADOW_SIZE, SHADOW_SIZE);
    mInitialised = true;
    return true;
}

//======================================================================================================================
void ShadowManager::ComputeCascades(const glm::mat4& view, const glm::mat4& proj,
                                    const Vector3& sunDir, float camNear, float shadowMaxDist)
{
    // Sun travel direction in world space (light rays move along +sunDir).
    glm::vec3 L = glm::normalize(glm::vec3(sunDir.x, sunDir.y, sunDir.z));
    if (glm::length(L) < 1e-4f)
        L = glm::vec3(0.0f, 0.0f, -1.0f);

    // World is +Z up; pick an up vector not parallel to L.
    glm::vec3 up = (fabsf(L.z) > 0.95f) ? glm::vec3(0.0f, 1.0f, 0.0f)
                                        : glm::vec3(0.0f, 0.0f, 1.0f);

    // Half-tangents of the (symmetric) camera frustum, recovered from proj.
    float tanX = (proj[0][0] != 0.0f) ? 1.0f / proj[0][0] : 1.0f;
    float tanY = (proj[1][1] != 0.0f) ? 1.0f / proj[1][1] : 1.0f;

    glm::mat4 invView = glm::inverse(view);

    float nearD = Maximum(camNear, 0.1f);
    float farD  = Maximum(shadowMaxDist, nearD + 1.0f);

    // Practical (log/uniform blend) split distances.
    float splitD[NUM_CASCADES + 1];
    splitD[0] = nearD;
    for (int i = 1; i <= NUM_CASCADES; ++i)
    {
        float p   = (float)i / (float)NUM_CASCADES;
        float logD = nearD * powf(farD / nearD, p);
        float uniD = nearD + (farD - nearD) * p;
        splitD[i] = mCascadeLambda * logD + (1.0f - mCascadeLambda) * uniD;
    }

    for (int i = 0; i < NUM_CASCADES; ++i)
    {
        float dN = splitD[i];
        float dF = splitD[i + 1];
        mSplits[i] = dF;

        // 8 sub-frustum corners in VIEW space (camera looks down -Z).
        glm::vec3 centreW(0.0f);
        glm::vec3 cornersW[8];
        int c = 0;
        for (int fi = 0; fi < 2; ++fi)
        {
            float d = (fi == 0) ? dN : dF;
            float x = d * tanX;
            float y = d * tanY;
            glm::vec4 vs[4] = {
                glm::vec4(-x, -y, -d, 1.0f),
                glm::vec4( x, -y, -d, 1.0f),
                glm::vec4( x,  y, -d, 1.0f),
                glm::vec4(-x,  y, -d, 1.0f),
            };
            for (int k = 0; k < 4; ++k)
            {
                glm::vec3 w = glm::vec3(invView * vs[k]);
                cornersW[c++] = w;
                centreW += w;
            }
        }
        centreW *= (1.0f / 8.0f);

        // Bounding sphere radius of this cascade (keeps the ortho size stable as
        // the camera rotates, which reduces shimmer).
        float radius = 0.0f;
        for (int k = 0; k < 8; ++k)
            radius = Maximum(radius, glm::length(cornersW[k] - centreW));
        radius = ceilf(radius * 16.0f) / 16.0f;

        // Pull the light eye back well behind the cascade so tall casters above
        // the ground are still captured.
        float pullback = radius * 2.0f;
        glm::vec3 eye = centreW - L * pullback;
        glm::mat4 lightView = glm::lookAt(eye, centreW, up);

        float zNear = 0.0f;
        float zFar  = pullback + radius * 2.0f;
        glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius, zNear, zFar);

        glm::mat4 lightVP = lightProj * lightView;

        // Texel snapping: round the projected world origin to whole shadow-map
        // texels to keep the shadow edges stable as the camera moves.
        glm::vec4 origin = lightVP * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        origin *= (float)SHADOW_SIZE * 0.5f;
        glm::vec2 rounded(floorf(origin.x + 0.5f), floorf(origin.y + 0.5f));
        glm::vec2 offset = (rounded - glm::vec2(origin.x, origin.y)) * (2.0f / (float)SHADOW_SIZE);
        glm::mat4 snap(1.0f);
        snap[3][0] = offset.x;
        snap[3][1] = offset.y;
        lightVP = snap * lightVP;

        mCascadeVP[i] = lightVP;
    }
}

//======================================================================================================================
void ShadowManager::RenderCascades(CasterCallback callback, void* user)
{
    if (!mInitialised || !callback)
        return;

    GLint prevFBO = 0;
    GLint prevViewport[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    // Depth bias to fight shadow acne. glPolygonOffset + a shader-independent
    // normal offset is left to the receiver bias (u_csmBias); here we use polygon
    // offset on the caster side.
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(mPolygonOffsetFactor, mPolygonOffsetUnits);

    if (mCullFrontFaces)
    {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
    }
    else
    {
        glDisable(GL_CULL_FACE);
    }

    const ShadowCastShader* shader =
        (const ShadowCastShader*)ShaderManager::GetInstance().GetShader(SHADER_SHADOWCAST);
    shader->Use();

    for (int i = 0; i < NUM_CASCADES; ++i)
    {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, mDepthArray, 0, i);
        glClear(GL_DEPTH_BUFFER_BIT);

        // Load the cascade world->light-clip matrix into GL_MODELVIEW (with an
        // identity projection) so that a caster pushing its world transform and
        // calling esSetModelViewProjectionMatrix() gets the correct light-clip
        // position - reusing all the normal model-render matrix machinery. See the
        // memory-layout note in ShadowManager.h/Graphics: a glm mat4's raw floats
        // are bit-identical to the engine's GLMat44 for this purpose.
        esMatrixMode(GL_PROJECTION);
        esLoadIdentity();
        esMatrixMode(GL_MODELVIEW);
        esLoadIdentity();
        esMultMatrixf(glm::value_ptr(mCascadeVP[i]));

        callback(user);
    }

    // Restore state.
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_CULL_FACE);
    glUseProgram(0);

    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
}
