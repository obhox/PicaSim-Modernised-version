#include "Window.h"
#include "Platform.h"
#include <stdio.h>
#include <time.h>

#include <stb_image_write.h>

// AI generated fix for macos build
#if defined(_WIN32)
    #include <glad/glad.h>
#elif defined(PICASIM_MACOS) || defined(__APPLE__)
    #include <OpenGL/gl.h>
#else
    #include "GLCompat.h"
#endif

// Global window instance
Window* gWindow = nullptr;

int Window::sAutoScreenshotFrame = 0;
std::string Window::sAutoScreenshotPath;

#if defined(_WIN32) && !defined(NDEBUG) && defined(GL_DEBUG_OUTPUT)
static void APIENTRY GLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                     GLsizei length, const GLchar* message, const void* userParam)
{
    (void)source; (void)id; (void)length; (void)userParam;
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
        return;
    fprintf(stderr, "GL debug (type 0x%04x, severity 0x%04x): %s\n", type, severity, message);
}
#endif

Window::Window()
    : mWindow(nullptr)
    , mContext(nullptr)
    , mWidth(0)
    , mHeight(0)
    , mFullscreen(false)
    , mGlMajorVersion(0)
    , mGlMinorVersion(0)
{
}

Window::~Window()
{
    Shutdown();
}

bool Window::Init(int width, int height, const char* title, bool fullscreen, int msaaSamples)
{
    // Initialize SDL video subsystem if not already done
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
    {
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
        {
            fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
            return false;
        }
    }

    // Get display mode for default resolution
    SDL_DisplayMode displayMode;
    if (SDL_GetDesktopDisplayMode(0, &displayMode) != 0)
    {
        fprintf(stderr, "SDL_GetDesktopDisplayMode failed: %s\n", SDL_GetError());
        return false;
    }

    // Use desktop resolution if width/height are 0
    if (width <= 0) width = displayMode.w;
    if (height <= 0) height = displayMode.h;

    // Set OpenGL attributes before creating window
    // Request OpenGL 3.3 Core Profile for desktop, but fall back to 2.1 if needed
#if defined(PS_PLATFORM_ANDROID) || defined(PS_PLATFORM_IOS)
    // Mobile platforms use OpenGL ES 2.0
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(PS_PLATFORM_MACOS)
    // macOS only exposes legacy fixed-function via 2.1; avoid core profile which drops our GLSL 120 shaders
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#else
    // Other desktop platforms - OpenGL 3.3 Compatibility Profile (supports legacy + modern GL)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif

#if !defined(NDEBUG) && !defined(PS_PLATFORM_MACOS) && !defined(PS_PLATFORM_ANDROID) && !defined(PS_PLATFORM_IOS)
    // Debug context so the driver reports errors via GL_KHR_debug (installed below)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    // Set MSAA attributes if requested
    if (msaaSamples > 0)
    {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, msaaSamples);
    }

    // Create window
    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    if (fullscreen)
    {
        windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    mWindow = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        windowFlags
    );

    if (!mWindow)
    {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    // Create OpenGL context
    mContext = SDL_GL_CreateContext(mWindow);
    if (!mContext)
    {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        
        // Try fallback: disable MSAA if it was requested
        if (msaaSamples > 0)
        {
            fprintf(stderr, "Retrying without MSAA...\n");
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
            mContext = SDL_GL_CreateContext(mWindow);
        }
        
        // If still failing, try OpenGL 2.1 compatibility mode
        if (!mContext)
        {
            fprintf(stderr, "Retrying with OpenGL 2.1...\n");
#if defined(PS_PLATFORM_MACOS)
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
#else
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
#endif
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
            mContext = SDL_GL_CreateContext(mWindow);
        }
        
        if (!mContext)
        {
            fprintf(stderr, "Failed to create any OpenGL context\n");
            SDL_DestroyWindow(mWindow);
            mWindow = nullptr;
            return false;
        }
    }

    // Make context current
    if (SDL_GL_MakeCurrent(mWindow, mContext) != 0)
    {
        fprintf(stderr, "SDL_GL_MakeCurrent failed: %s\n", SDL_GetError());
        SDL_GL_DeleteContext(mContext);
        SDL_DestroyWindow(mWindow);
        mContext = nullptr;
        mWindow = nullptr;
        return false;
    }

#ifdef _WIN32
    // Initialize GLAD for OpenGL function loading on Windows
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        fprintf(stderr, "Failed to initialize GLAD\n");
        SDL_GL_DeleteContext(mContext);
        SDL_DestroyWindow(mWindow);
        mContext = nullptr;
        mWindow = nullptr;
        return false;
    }

#if !defined(NDEBUG) && defined(GL_DEBUG_OUTPUT)
    // Driver-side error reporting; only present if we got a context that exposes it
    if (glDebugMessageCallback)
    {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(GLDebugCallback, nullptr);
    }
#endif
#endif

    // Enable vsync (1 = enable, 0 = disable, -1 = adaptive)
    SDL_GL_SetSwapInterval(1);

    // Get actual window size (may differ from requested due to DPI scaling)
    SDL_GetWindowSize(mWindow, &mWidth, &mHeight);
    mFullscreen = fullscreen;

    // Get OpenGL version
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &mGlMajorVersion);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &mGlMinorVersion);

    // Enable multisampling if requested
    if (msaaSamples > 0)
    {
        glEnable(GL_MULTISAMPLE);
    }

    // Get actual MSAA samples (may be different from requested if not supported)
    int actualMsaaSamples = 0;
    SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &actualMsaaSamples);

    const GLubyte* vendor = glGetString(GL_VENDOR);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version = glGetString(GL_VERSION);

    if (!vendor || !renderer || !version)
    {
        fprintf(stderr, "OpenGL context is missing required strings (vendor=%p renderer=%p version=%p)\n", vendor, renderer, version);
        SDL_GL_DeleteContext(mContext);
        SDL_DestroyWindow(mWindow);
        mContext = nullptr;
        mWindow = nullptr;
        return false;
    }

    GLint depthBits = 0;
    glGetIntegerv(GL_DEPTH_BITS, &depthBits);

    printf("Window created: %dx%d, OpenGL %d.%d\n", mWidth, mHeight, mGlMajorVersion, mGlMinorVersion);
    printf("OpenGL Vendor: %s\n", vendor);
    printf("OpenGL Renderer: %s\n", renderer);
    printf("OpenGL Version: %s\n", version);
    printf("Depth buffer bits: %d\n", depthBits);
    printf("MSAA: requested=%d, actual=%d\n", msaaSamples, actualMsaaSamples);

    return true;
}

void Window::Shutdown()
{
    if (mContext)
    {
        SDL_GL_DeleteContext(mContext);
        mContext = nullptr;
    }

    if (mWindow)
    {
        SDL_DestroyWindow(mWindow);
        mWindow = nullptr;
    }

    mWidth = 0;
    mHeight = 0;
    mFullscreen = false;
}

void Window::SwapBuffers()
{
    if (mWindow)
    {
        ++mFrameCounter;

        if (mScreenshotPending)
        {
            CaptureScreenshot(mScreenshotPath);
            mScreenshotPending = false;
            mScreenshotPath.clear();
        }
        else if (sAutoScreenshotFrame > 0 && mFrameCounter == sAutoScreenshotFrame)
        {
            CaptureScreenshot(sAutoScreenshotPath);
            // Automated capture done - request quit so scripts get a clean exit
            SDL_Event quitEvent;
            quitEvent.type = SDL_QUIT;
            SDL_PushEvent(&quitEvent);
        }

#ifndef NDEBUG
        // Any error still queued here escaped the per-call checks this frame.
        // The same error typically repeats every frame, so only report a given
        // code once per run to avoid drowning out new signal.
        static unsigned int reported = 0;
        for (GLenum err; (err = glGetError()) != GL_NO_ERROR; )
        {
            unsigned int bit = 1u << (err & 0x1f);
            if (!(reported & bit))
            {
                reported |= bit;
                fprintf(stderr, "GL error 0x%04x reached end of frame %ld "
                                "(further occurrences of this code suppressed)\n", err, mFrameCounter);
            }
        }
#endif

        SDL_GL_SwapWindow(mWindow);
    }
}

void Window::RequestScreenshot(const std::string& path)
{
    mScreenshotPending = true;
    mScreenshotPath = path;
}

void Window::SetAutoScreenshot(int frameNumber, const std::string& path)
{
    sAutoScreenshotFrame = frameNumber;
    sAutoScreenshotPath = path;
}

void Window::CaptureScreenshot(const std::string& path)
{
    std::string file = path;
    if (file.empty())
    {
        std::string dir = Platform::GetUserDataPath() + "Screenshots";
        FileSystem::MakeDirectory(dir);

        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        char name[64];
        snprintf(name, sizeof(name), "/PicaSim-%04d%02d%02d-%02d%02d%02d-f%ld.png",
                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                 t->tm_hour, t->tm_min, t->tm_sec, mFrameCounter);
        file = dir + name;
    }

    unsigned char* pixels = (unsigned char*)malloc((size_t)mWidth * mHeight * 3);
    if (!pixels)
        return;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, mWidth, mHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    stbi_flip_vertically_on_write(1);
    int ok = stbi_write_png(file.c_str(), mWidth, mHeight, 3, pixels, mWidth * 3);
    stbi_flip_vertically_on_write(0);
    free(pixels);

    if (ok)
        printf("Screenshot saved: %s\n", file.c_str());
    else
        fprintf(stderr, "Failed to save screenshot: %s\n", file.c_str());
}

bool Window::ProcessEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_QUIT:
            return false;

        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                mWidth = event.window.data1;
                mHeight = event.window.data2;
            }
            break;

        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE)
            {
                // ESC can be used to exit fullscreen
                if (mFullscreen)
                {
                    SetFullscreen(false);
                }
            }
            break;
        }

        // TODO: Forward events to input system
    }

    return true;
}

void Window::SetFullscreen(bool fullscreen)
{
    if (mWindow && mFullscreen != fullscreen)
    {
        Uint32 flags = fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
        if (SDL_SetWindowFullscreen(mWindow, flags) == 0)
        {
            mFullscreen = fullscreen;
            SDL_GetWindowSize(mWindow, &mWidth, &mHeight);
        }
    }
}

void Window::SetTitle(const char* title)
{
    if (mWindow)
    {
        SDL_SetWindowTitle(mWindow, title);
    }
}

void Window::Resize(int width, int height)
{
    if (mWindow && !mFullscreen)
    {
        SDL_SetWindowSize(mWindow, width, height);
        mWidth = width;
        mHeight = height;
    }
}

Window& Window::GetInstance()
{
    static Window instance;
    return instance;
}
