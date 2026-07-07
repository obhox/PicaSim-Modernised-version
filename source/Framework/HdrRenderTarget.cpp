#include "HdrRenderTarget.h"
#include "Trace.h"

// Some of these tokens are not present in the GLES2 headers used on the (not yet
// built) mobile targets; provide fallbacks so the file always compiles. On the
// desktop core profiles (macOS gl3, Windows/Linux 3.3) they come from the GL
// headers already.
#ifndef GL_RGBA16F
#define GL_RGBA16F 0x881A
#endif
#ifndef GL_R11F_G11F_B10F
#define GL_R11F_G11F_B10F 0x8C3A
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif

//======================================================================================================================
HdrRenderTarget::HdrRenderTarget(int width, int height)
    : mWidth(width)
    , mHeight(height)
    , mFBO(0)
    , mColorTex(0)
    , mDepthTex(0)
    , mDepthRB(0)
    , mPrevFBO(0)
{
    mPrevViewport[0] = mPrevViewport[1] = mPrevViewport[2] = mPrevViewport[3] = 0;
    Create();
}

//======================================================================================================================
HdrRenderTarget::~HdrRenderTarget()
{
    Destroy();
}

//======================================================================================================================
void HdrRenderTarget::Destroy()
{
    if (mColorTex) { glDeleteTextures(1, &mColorTex); mColorTex = 0; }
    if (mDepthTex) { glDeleteTextures(1, &mDepthTex); mDepthTex = 0; }
    if (mDepthRB)  { glDeleteRenderbuffers(1, &mDepthRB); mDepthRB = 0; }
    if (mFBO)      { glDeleteFramebuffers(1, &mFBO); mFBO = 0; }
}

//======================================================================================================================
void HdrRenderTarget::Create()
{
    if (mWidth < 1)  mWidth = 1;
    if (mHeight < 1) mHeight = 1;

    // Preserve whatever FBO is currently bound; we bind our own while building.
    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

    glGenFramebuffers(1, &mFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);

    // Depth attachment: prefer a DEPTH_COMPONENT24 texture (samplable by later
    // phases). If that turns out to be incomplete we fall back to a renderbuffer.
    glGenTextures(1, &mDepthTex);
    glBindTexture(GL_TEXTURE_2D, mDepthTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, mWidth, mHeight, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mDepthTex, 0);

    // Colour attachment: try progressively more conservative float formats, then
    // finally plain RGBA8, keeping whichever first yields a complete FBO.
    struct FormatChoice { GLint internalFormat; GLenum format; GLenum type; const char* name; };
    const FormatChoice choices[] = {
        { GL_RGBA16F,        GL_RGBA, GL_FLOAT,         "RGBA16F"        },
        { GL_R11F_G11F_B10F, GL_RGB,  GL_FLOAT,         "R11F_G11F_B10F" },
        { GL_RGBA8,          GL_RGBA, GL_UNSIGNED_BYTE, "RGBA8"          },
    };

    bool complete = false;
    for (int i = 0; i < 3 && !complete; ++i)
    {
        if (mColorTex == 0)
            glGenTextures(1, &mColorTex);
        glBindTexture(GL_TEXTURE_2D, mColorTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, choices[i].internalFormat, mWidth, mHeight, 0,
                     choices[i].format, choices[i].type, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColorTex, 0);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status == GL_FRAMEBUFFER_COMPLETE)
        {
            TRACE_FILE_IF(1) TRACE("HdrRenderTarget: using colour format %s (%dx%d)", choices[i].name, mWidth, mHeight);
            complete = true;
        }
        else
        {
            TRACE_FILE_IF(1) TRACE("HdrRenderTarget: colour format %s incomplete (status 0x%x), trying next", choices[i].name, status);
        }
    }

    // If the FBO is still incomplete, the depth texture attachment may be the
    // culprit - retry with a depth renderbuffer instead.
    if (!complete)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
        glDeleteTextures(1, &mDepthTex);
        mDepthTex = 0;

        glGenRenderbuffers(1, &mDepthRB);
        glBindRenderbuffer(GL_RENDERBUFFER, mDepthRB);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mWidth, mHeight);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepthRB);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        complete = (status == GL_FRAMEBUFFER_COMPLETE);
        TRACE_FILE_IF(1) TRACE("HdrRenderTarget: depth-renderbuffer fallback %s (status 0x%x)",
                               complete ? "succeeded" : "FAILED", status);
    }

    IwAssert(ROWLHOUSE, complete);

    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
}

//======================================================================================================================
void HdrRenderTarget::Bind()
{
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &mPrevFBO);
    glGetIntegerv(GL_VIEWPORT, &mPrevViewport[0]);

    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    glViewport(0, 0, mWidth, mHeight);
}

//======================================================================================================================
void HdrRenderTarget::Release()
{
    glBindFramebuffer(GL_FRAMEBUFFER, mPrevFBO);
    glViewport(mPrevViewport[0], mPrevViewport[1], mPrevViewport[2], mPrevViewport[3]);
}

//======================================================================================================================
void HdrRenderTarget::Resize(int width, int height)
{
    if (width == mWidth && height == mHeight)
        return;
    Destroy();
    mWidth = width;
    mHeight = height;
    Create();
}
