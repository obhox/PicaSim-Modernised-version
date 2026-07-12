#include "PostProcess.h"
#include "ShaderSource.h"
#include "Trace.h"

#include <string>

#ifndef GL_RGBA16F
#define GL_RGBA16F 0x881A
#endif

//======================================================================================================================
GLuint PostProcess::CompileProgram(const char* vertFile, const char* fragFile)
{
    std::string vertSrc = ShaderSource::Build(vertFile, ShaderSource::STAGE_VERTEX);
    std::string fragSrc = ShaderSource::Build(fragFile, ShaderSource::STAGE_FRAGMENT);
    if (vertSrc.empty() || fragSrc.empty())
    {
        TRACE("PostProcess: failed to load %s / %s", vertFile, fragFile);
        return 0;
    }
    GLuint prog = esLoadProgram(vertSrc.c_str(), fragSrc.c_str());
    if (prog == 0)
        TRACE("PostProcess: failed to compile/link %s / %s", vertFile, fragFile);
    return prog;
}

//======================================================================================================================
void PostProcess::Init()
{
    if (mInitialised)
        return;

    mTonemapProgram = CompileProgram("fullscreen.vert", "tonemap.frag");
    mDownProgram    = CompileProgram("fullscreen.vert", "bloom_down.frag");
    mUpProgram      = CompileProgram("fullscreen.vert", "bloom_up.frag");
    mFxaaProgram    = CompileProgram("fullscreen.vert", "fxaa.frag");
    mSsaoProgram    = CompileProgram("fullscreen.vert", "ssao.frag");
    mAoBlurProgram  = CompileProgram("fullscreen.vert", "ssao_blur.frag");

    if (mTonemapProgram)
    {
        u_hdrTexture     = glGetUniformLocation(mTonemapProgram, "u_hdrTexture");
        u_bloomTexture   = glGetUniformLocation(mTonemapProgram, "u_bloomTexture");
        u_uvOffset       = glGetUniformLocation(mTonemapProgram, "u_uvOffset");
        u_uvScale        = glGetUniformLocation(mTonemapProgram, "u_uvScale");
        u_exposure       = glGetUniformLocation(mTonemapProgram, "u_exposure");
        u_bloomIntensity = glGetUniformLocation(mTonemapProgram, "u_bloomIntensity");
        u_tonemap        = glGetUniformLocation(mTonemapProgram, "u_tonemap");
        u_aoTexture      = glGetUniformLocation(mTonemapProgram, "u_aoTexture");
        u_useAO          = glGetUniformLocation(mTonemapProgram, "u_useAO");
    }
    if (mSsaoProgram)
    {
        s_depthTex    = glGetUniformLocation(mSsaoProgram, "u_depthTex");
        s_texelSize   = glGetUniformLocation(mSsaoProgram, "u_texelSize");
        s_near        = glGetUniformLocation(mSsaoProgram, "u_near");
        s_far         = glGetUniformLocation(mSsaoProgram, "u_far");
        s_tanHalfFovY = glGetUniformLocation(mSsaoProgram, "u_tanHalfFovY");
        s_aspect      = glGetUniformLocation(mSsaoProgram, "u_aspect");
        s_radius      = glGetUniformLocation(mSsaoProgram, "u_radius");
        s_intensity   = glGetUniformLocation(mSsaoProgram, "u_intensity");
    }
    if (mAoBlurProgram)
    {
        b_aoTex     = glGetUniformLocation(mAoBlurProgram, "u_aoTex");
        b_texelSize = glGetUniformLocation(mAoBlurProgram, "u_texelSize");
    }
    if (mDownProgram)
    {
        d_texture   = glGetUniformLocation(mDownProgram, "u_texture");
        d_texelSize = glGetUniformLocation(mDownProgram, "u_texelSize");
        d_firstMip  = glGetUniformLocation(mDownProgram, "u_firstMip");
    }
    if (mUpProgram)
    {
        up_texture   = glGetUniformLocation(mUpProgram, "u_texture");
        up_texelSize = glGetUniformLocation(mUpProgram, "u_texelSize");
        up_radius    = glGetUniformLocation(mUpProgram, "u_radius");
    }
    if (mFxaaProgram)
    {
        fx_texture   = glGetUniformLocation(mFxaaProgram, "u_texture");
        fx_texelSize = glGetUniformLocation(mFxaaProgram, "u_texelSize");
    }

    mInitialised = (mTonemapProgram != 0);
    if (!mInitialised)
        TRACE("PostProcess: tonemap program missing - HDR resolve disabled");
}

//======================================================================================================================
void PostProcess::Terminate()
{
    DestroyBloomResources();
    DestroyLdrResources();
    DestroyAoResources();
    if (mTonemapProgram) { glDeleteProgram(mTonemapProgram); mTonemapProgram = 0; }
    if (mDownProgram)    { glDeleteProgram(mDownProgram);    mDownProgram = 0; }
    if (mUpProgram)      { glDeleteProgram(mUpProgram);      mUpProgram = 0; }
    if (mFxaaProgram)    { glDeleteProgram(mFxaaProgram);    mFxaaProgram = 0; }
    if (mSsaoProgram)    { glDeleteProgram(mSsaoProgram);    mSsaoProgram = 0; }
    if (mAoBlurProgram)  { glDeleteProgram(mAoBlurProgram);  mAoBlurProgram = 0; }
    mInitialised = false;
}

//======================================================================================================================
void PostProcess::DestroyAoResources()
{
    if (mAoFBO)     { glDeleteFramebuffers(1, &mAoFBO);     mAoFBO = 0; }
    if (mAoTex)     { glDeleteTextures(1, &mAoTex);         mAoTex = 0; }
    if (mAoBlurFBO) { glDeleteFramebuffers(1, &mAoBlurFBO); mAoBlurFBO = 0; }
    if (mAoBlurTex) { glDeleteTextures(1, &mAoBlurTex);     mAoBlurTex = 0; }
    mAoW = mAoH = 0;
}

//======================================================================================================================
void PostProcess::EnsureAoResources(int w, int h)
{
    if (w == mAoW && h == mAoH && mAoFBO != 0)
        return;
    DestroyAoResources();
    mAoW = w; mAoH = h;

    GLuint* texs[2] = { &mAoTex, &mAoBlurTex };
    GLuint* fbos[2] = { &mAoFBO, &mAoBlurFBO };
    for (int i = 0; i < 2; ++i)
    {
        glGenTextures(1, texs[i]);
        glBindTexture(GL_TEXTURE_2D, *texs[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenFramebuffers(1, fbos[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, *fbos[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *texs[i], 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

//======================================================================================================================
GLuint PostProcess::RunSSAO(GLuint depthTex, int w, int h,
                            float camNear, float camFar, float camTanHalfFovY, float camAspect,
                            const PostSettings& settings)
{
    if (depthTex == 0 || mSsaoProgram == 0 || mAoBlurProgram == 0)
        return 0;
    EnsureAoResources(w, h);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glActiveTexture(GL_TEXTURE0);

    // --- SSAO pass: depth -> raw AO ---
    glBindFramebuffer(GL_FRAMEBUFFER, mAoFBO);
    glViewport(0, 0, w, h);
    glUseProgram(mSsaoProgram);
    glBindTexture(GL_TEXTURE_2D, depthTex);
    glUniform1i(s_depthTex, 0);
    glUniform2f(s_texelSize, 1.0f / (float)w, 1.0f / (float)h);
    glUniform1f(s_near, camNear);
    glUniform1f(s_far, camFar);
    glUniform1f(s_tanHalfFovY, camTanHalfFovY);
    glUniform1f(s_aspect, camAspect);
    glUniform1f(s_radius, settings.ssaoRadius);
    glUniform1f(s_intensity, settings.ssaoIntensity);
    DrawFullscreenTriangle();

    // --- Blur pass: raw AO -> blurred AO ---
    glBindFramebuffer(GL_FRAMEBUFFER, mAoBlurFBO);
    glViewport(0, 0, w, h);
    glUseProgram(mAoBlurProgram);
    glBindTexture(GL_TEXTURE_2D, mAoTex);
    glUniform1i(b_aoTex, 0);
    glUniform2f(b_texelSize, 1.0f / (float)w, 1.0f / (float)h);
    DrawFullscreenTriangle();

    return mAoBlurTex;
}

//======================================================================================================================
void PostProcess::DrawFullscreenTriangle()
{
    // No vertex attributes: the vertex shader manufactures the triangle from
    // gl_VertexID. Relies on the app-wide bound VAO.
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

//======================================================================================================================
void PostProcess::DestroyBloomResources()
{
    for (size_t i = 0; i < mMips.size(); ++i)
    {
        if (mMips[i].fbo) glDeleteFramebuffers(1, &mMips[i].fbo);
        if (mMips[i].tex) glDeleteTextures(1, &mMips[i].tex);
    }
    mMips.clear();
    mBloomBaseW = mBloomBaseH = 0;
}

//======================================================================================================================
void PostProcess::EnsureBloomResources(int w, int h)
{
    if (w == mBloomBaseW && h == mBloomBaseH && !mMips.empty())
        return;

    DestroyBloomResources();
    mBloomBaseW = w;
    mBloomBaseH = h;

    // Build a mip chain, each half the previous, starting at half the base
    // resolution, down to a small floor. Cap the level count.
    const int kMaxMips = 6;
    int mw = w / 2;
    int mh = h / 2;
    for (int i = 0; i < kMaxMips; ++i)
    {
        if (mw < 2 || mh < 2)
            break;

        Mip mip;
        mip.w = mw;
        mip.h = mh;
        glGenTextures(1, &mip.tex);
        glBindTexture(GL_TEXTURE_2D, mip.tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, mw, mh, 0, GL_RGBA, GL_FLOAT, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenFramebuffers(1, &mip.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, mip.fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mip.tex, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        mMips.push_back(mip);
        mw /= 2;
        mh /= 2;
    }
}

//======================================================================================================================
GLuint PostProcess::RunBloom(GLuint hdrTex, int hdrW, int hdrH, const PostSettings& /*settings*/)
{
    EnsureBloomResources(hdrW, hdrH);
    if (mMips.empty() || mDownProgram == 0 || mUpProgram == 0)
        return 0;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glActiveTexture(GL_TEXTURE0);

    // --- Downsample chain ---
    glUseProgram(mDownProgram);
    glUniform1i(d_texture, 0);

    GLuint srcTex = hdrTex;
    int srcW = hdrW, srcH = hdrH;
    for (size_t i = 0; i < mMips.size(); ++i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mMips[i].fbo);
        glViewport(0, 0, mMips[i].w, mMips[i].h);
        glUniform2f(d_texelSize, 1.0f / (float)srcW, 1.0f / (float)srcH);
        glUniform1f(d_firstMip, (i == 0) ? 1.0f : 0.0f);
        glBindTexture(GL_TEXTURE_2D, srcTex);
        DrawFullscreenTriangle();

        srcTex = mMips[i].tex;
        srcW = mMips[i].w;
        srcH = mMips[i].h;
    }

    // --- Upsample chain (additive) ---
    glUseProgram(mUpProgram);
    glUniform1i(up_texture, 0);
    glUniform1f(up_radius, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    for (int i = (int)mMips.size() - 2; i >= 0; --i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mMips[i].fbo);
        glViewport(0, 0, mMips[i].w, mMips[i].h);
        glUniform2f(up_texelSize, 1.0f / (float)mMips[i + 1].w, 1.0f / (float)mMips[i + 1].h);
        glBindTexture(GL_TEXTURE_2D, mMips[i + 1].tex);
        DrawFullscreenTriangle();
    }

    glDisable(GL_BLEND);
    return mMips[0].tex;
}

//======================================================================================================================
void PostProcess::DestroyLdrResources()
{
    if (mLdrFBO) { glDeleteFramebuffers(1, &mLdrFBO); mLdrFBO = 0; }
    if (mLdrTex) { glDeleteTextures(1, &mLdrTex); mLdrTex = 0; }
    mLdrW = mLdrH = 0;
}

//======================================================================================================================
void PostProcess::EnsureLdrResources(int w, int h)
{
    if (w == mLdrW && h == mLdrH && mLdrFBO != 0)
        return;
    DestroyLdrResources();
    mLdrW = w;
    mLdrH = h;

    glGenTextures(1, &mLdrTex);
    glBindTexture(GL_TEXTURE_2D, mLdrTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &mLdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, mLdrFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mLdrTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//======================================================================================================================
void PostProcess::Resolve(GLuint hdrColorTex, GLuint depthTex, int hdrW, int hdrH,
                          int rectX, int rectY, int rectW, int rectH,
                          float camNear, float camFar, float camTanHalfFovY, float camAspect,
                          const PostSettings& settings)
{
    if (!mInitialised)
        return;

    // Snapshot GL state we intend to change; restore at the end so the following
    // (LDR) overlay pass sees the state it expects.
    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

    const bool wantBloom = settings.bloomEnabled && settings.bloomIntensity > 0.0f;
    GLuint bloomTex = 0;
    if (wantBloom)
        bloomTex = RunBloom(hdrColorTex, hdrW, hdrH, settings);

    const bool wantAO = settings.ssaoEnabled && depthTex != 0;
    GLuint aoTex = 0;
    if (wantAO)
        aoTex = RunSSAO(depthTex, hdrW, hdrH, camNear, camFar, camTanHalfFovY, camAspect, settings);

    // Sub-rect of the HDR texture that maps to this viewport rect.
    float uvOffX = (float)rectX / (float)hdrW;
    float uvOffY = (float)rectY / (float)hdrH;
    float uvScaleX = (float)rectW / (float)hdrW;
    float uvScaleY = (float)rectH / (float)hdrH;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);

    const bool wantFxaa = settings.fxaaEnabled && mFxaaProgram != 0;

    // Tonemap pass. If FXAA is on, render into an LDR scratch target (matched to
    // the resolve rect) first; otherwise straight to the currently bound FBO.
    if (wantFxaa)
    {
        EnsureLdrResources(rectW, rectH);
        glBindFramebuffer(GL_FRAMEBUFFER, mLdrFBO);
        glViewport(0, 0, rectW, rectH);
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
        glViewport(rectX, rectY, rectW, rectH);
    }

    glUseProgram(mTonemapProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrColorTex);
    glUniform1i(u_hdrTexture, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, wantBloom ? bloomTex : hdrColorTex);
    glUniform1i(u_bloomTexture, 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, (wantAO && aoTex) ? aoTex : hdrColorTex);
    glUniform1i(u_aoTexture, 2);
    if (u_useAO >= 0) glUniform1f(u_useAO, (wantAO && aoTex) ? 1.0f : 0.0f);
    glActiveTexture(GL_TEXTURE0);

    if (wantFxaa)
    {
        // Rendering into a 0-based scratch target: the whole target is this rect.
        glUniform2f(u_uvOffset, uvOffX, uvOffY);
        glUniform2f(u_uvScale, uvScaleX, uvScaleY);
    }
    else
    {
        glUniform2f(u_uvOffset, uvOffX, uvOffY);
        glUniform2f(u_uvScale, uvScaleX, uvScaleY);
    }
    glUniform1f(u_exposure, settings.exposure);
    glUniform1f(u_bloomIntensity, wantBloom ? settings.bloomIntensity : 0.0f);
    glUniform1f(u_tonemap, settings.pbrTonemap ? 1.0f : 0.0f);
    DrawFullscreenTriangle();

    // FXAA pass: scratch LDR -> destination rect on the original FBO.
    if (wantFxaa)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
        glViewport(rectX, rectY, rectW, rectH);
        glUseProgram(mFxaaProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mLdrTex);
        glUniform1i(fx_texture, 0);
        glUniform2f(fx_texelSize, 1.0f / (float)rectW, 1.0f / (float)rectH);
        DrawFullscreenTriangle();
    }

    // Restore state expected by subsequent LDR rendering.
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glUseProgram(0);
    glEnable(GL_DEPTH_TEST);
    PICASIM_CHECK_GL_ERRORS();
}
