#ifndef HDRRENDERTARGET_H
#define HDRRENDERTARGET_H

#include "Graphics.h"

// HdrRenderTarget is an offscreen framebuffer with a floating-point (RGBA16F)
// colour attachment plus a depth attachment. The 3D scene is rendered into it
// so that colours have headroom above 1.0 (for bloom / tonemapping) before
// being resolved to the LDR default framebuffer by PostProcess.
//
// The colour format degrades gracefully: RGBA16F -> R11F_G11F_B10F -> RGBA8,
// chosen at construction based on framebuffer completeness. The depth
// attachment is a DEPTH_COMPONENT24 texture (so later phases can sample depth),
// falling back to a renderbuffer if the texture attachment is unsupported.
class HdrRenderTarget
{
public:
    HdrRenderTarget(int width, int height);
    ~HdrRenderTarget();

    // Bind the FBO for rendering, saving the previously bound FBO + viewport so
    // Release() can restore them (mirrors FrameBufferObject's behaviour).
    void Bind();
    void Release();

    GLuint GetColorTexture() const { return mColorTex; }
    // Returns the depth texture handle, or 0 if depth is a renderbuffer.
    GLuint GetDepthTexture() const { return mDepthTex; }

    void Resize(int width, int height);

    int GetWidth() const { return mWidth; }
    int GetHeight() const { return mHeight; }

private:
    void Create();
    void Destroy();

    int    mWidth;
    int    mHeight;

    GLuint mFBO;
    GLuint mColorTex;
    GLuint mDepthTex;   // 0 if using a renderbuffer instead
    GLuint mDepthRB;    // 0 if using a texture instead

    GLint  mPrevFBO;
    GLint  mPrevViewport[4];
};

#endif // HDRRENDERTARGET_H
