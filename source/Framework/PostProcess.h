#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include "Graphics.h"
#include <vector>

// Tunables for the HDR resolve. Defaults are chosen so the resolve is as close
// to a pass-through as the tonemap allows: bloom OFF, exposure 1.0, no FXAA.
struct PostSettings
{
    bool  bloomEnabled   = false;
    float bloomIntensity = 0.0f;   // 0 => bloom skipped entirely
    float exposure       = 1.0f;
    bool  fxaaEnabled    = false;
    bool  pbrTonemap     = false;  // false => identity clamp (preserve LDR look); true => PBR Neutral
};

// PostProcess resolves the floating-point HDR scene texture to the LDR default
// framebuffer: optional bloom, then PBR Neutral tonemap, then optional FXAA.
// It owns its own GLSL programs (compiled directly from disk via ShaderSource,
// bypassing ShaderManager) and its own scratch framebuffers.
//
// The fullscreen pass is a single attribute-less triangle generated from
// gl_VertexID, so it relies only on the app-wide bound VAO.
class PostProcess
{
public:
    void Init();
    void Terminate();
    bool IsInitialised() const { return mInitialised; }

    // Resolve the sub-rectangle [rectX,rectY,rectW,rectH] of the HDR texture
    // (whose full size is hdrW x hdrH) to the identically-placed rectangle on the
    // currently bound draw framebuffer (expected to be the default framebuffer).
    // For a full-screen (mono) resolve the rect is the whole texture; for stereo
    // it is one eye's half.
    void Resolve(GLuint hdrColorTex, int hdrW, int hdrH,
                 int rectX, int rectY, int rectW, int rectH,
                 const PostSettings& settings);

private:
    static GLuint CompileProgram(const char* vertFile, const char* fragFile);
    static void DrawFullscreenTriangle();

    void EnsureBloomResources(int w, int h);
    void EnsureLdrResources(int w, int h);
    void DestroyBloomResources();
    void DestroyLdrResources();
    // Runs the down/up bloom chain over hdrTex; returns the bloom result texture.
    GLuint RunBloom(GLuint hdrTex, int hdrW, int hdrH, const PostSettings& settings);

    bool mInitialised = false;

    GLuint mTonemapProgram = 0;
    GLuint mDownProgram    = 0;
    GLuint mUpProgram      = 0;
    GLuint mFxaaProgram    = 0;

    // tonemap uniform locations
    GLint u_hdrTexture = -1, u_bloomTexture = -1;
    GLint u_uvOffset = -1, u_uvScale = -1;
    GLint u_exposure = -1, u_bloomIntensity = -1, u_tonemap = -1;

    // downsample / upsample / fxaa uniform locations
    GLint d_texture = -1, d_texelSize = -1, d_firstMip = -1;
    GLint up_texture = -1, up_texelSize = -1, up_radius = -1;
    GLint fx_texture = -1, fx_texelSize = -1;

    struct Mip { GLuint tex = 0; GLuint fbo = 0; int w = 0; int h = 0; };
    std::vector<Mip> mMips;
    int mBloomBaseW = 0, mBloomBaseH = 0;

    // LDR scratch target used only when FXAA is enabled.
    GLuint mLdrTex = 0, mLdrFBO = 0;
    int mLdrW = 0, mLdrH = 0;
};

#endif // POSTPROCESS_H
