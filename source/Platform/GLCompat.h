#pragma once

// AI generated fix
// this header is an AI generated fix for macos build

#ifdef __APPLE__
  // OpenGL core profile on macOS
  #include <OpenGL/gl3.h>

  // --- OpenGL ES compat (OES -> desktop) ---
  #ifndef GL_FRAMEBUFFER_OES
    #define GL_FRAMEBUFFER_OES GL_FRAMEBUFFER
  #endif
  #ifndef GL_RENDERBUFFER_OES
    #define GL_RENDERBUFFER_OES GL_RENDERBUFFER
  #endif
  #ifndef GL_DEPTH_ATTACHMENT_OES
    #define GL_DEPTH_ATTACHMENT_OES GL_DEPTH_ATTACHMENT
  #endif

  // Funzioni OES -> core
  #ifndef glFramebufferRenderbufferOES
    #define glFramebufferRenderbufferOES glFramebufferRenderbuffer
  #endif

  // Orthof (float) -> Ortho (double)
  #ifndef glOrthof
    #define glOrthof(l,r,b,t,n,f) glOrtho((GLdouble)(l),(GLdouble)(r),(GLdouble)(b),(GLdouble)(t),(GLdouble)(n),(GLdouble)(f))
  #endif

  // Core profile (gl3.h) removes these legacy enums. They are no longer used to
  // drive the fixed-function pipeline; the CPU-side matrix stack in Graphics.cpp
  // uses the matrix-mode/light enums purely as array selectors (e.g.
  // mode - GL_MODELVIEW), so we keep the canonical values here. The luminance
  // formats are still passed to glTexImage2D by Texture.cpp.
  #ifndef GL_MODELVIEW
    #define GL_MODELVIEW  0x1700
  #endif
  #ifndef GL_PROJECTION
    #define GL_PROJECTION 0x1701
  #endif
  #ifndef GL_TEXTURE
    #define GL_TEXTURE    0x1702
  #endif
  #ifndef GL_LIGHT0
    #define GL_LIGHT0 0x4000
    #define GL_LIGHT1 0x4001
    #define GL_LIGHT2 0x4002
    #define GL_LIGHT3 0x4003
    #define GL_LIGHT4 0x4004
    #define GL_LIGHT5 0x4005
    #define GL_LIGHT6 0x4006
    #define GL_LIGHT7 0x4007
  #endif
  #ifndef GL_LUMINANCE
    #define GL_LUMINANCE 0x1909
  #endif
  #ifndef GL_LUMINANCE_ALPHA
    #define GL_LUMINANCE_ALPHA 0x190A
  #endif
#endif
