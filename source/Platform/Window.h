#ifndef WINDOW_H
#define WINDOW_H

/**
  * Window - SDL2-based window and OpenGL context management
  *
  * This class replaces the Marmalade EGL initialization (eglInit/eglTerm)
  * with SDL2 window and OpenGL context creation.
  */

#include <SDL.h>
#include <string>

class Window
{
public:
        Window();
        ~Window();

        /**
          * Initialize the window and OpenGL context.
          * @param width Window width (0 for desktop resolution)
          * @param height Window height (0 for desktop resolution)
          * @param title Window title
          * @param fullscreen Start in fullscreen mode
          * @param msaaSamples MSAA sample count (0=off, 2, 4, 8)
          * @return true on success
          */
        bool Init(int width = 0, int height = 0, const char* title = "PicaSim", bool fullscreen = false, int msaaSamples = 0);

        /**
          * Shutdown the window and release resources.
          */
        void Shutdown();

        /**
          * Swap the front and back buffers.
          */
        void SwapBuffers();

        /**
          * Process window events.
          * @return false if quit was requested
          */
        bool ProcessEvents();

        // Accessors
        int GetWidth() const { return mWidth; }
        int GetHeight() const { return mHeight; }
        float GetAspectRatio() const { return mWidth > 0 ? (float)mWidth / mHeight : 1.0f; }
        bool IsFullscreen() const { return mFullscreen; }
        bool IsInitialized() const { return mWindow != nullptr; }

        // Update internal size (called on resize events)
        void SetSize(int width, int height) { mWidth = width; mHeight = height; }

        // Window manipulation
        void SetFullscreen(bool fullscreen);
        void SetTitle(const char* title);
        void Resize(int width, int height);

        // Get the SDL window handle (for advanced usage)
        SDL_Window* GetSDLWindow() const { return mWindow; }
        SDL_GLContext GetGLContext() const { return mContext; }

        // Get OpenGL version
        int GetGLMajorVersion() const { return mGlMajorVersion; }
        int GetGLMinorVersion() const { return mGlMinorVersion; }

        /**
          * Request a screenshot of the next completed frame (captured from the
          * back buffer just before the buffer swap, so it works from menus as
          * well as in-game). Empty path = auto-named PNG in the user data
          * Screenshots directory.
          */
        void RequestScreenshot(const std::string& path = std::string());

        /**
          * Arrange for a screenshot to be captured automatically when the given
          * frame number is reached (1-based, counted at buffer swaps). Used by
          * the --screenshot-after command line flag; may be called before the
          * window exists. frameNumber <= 0 disables.
          */
        static void SetAutoScreenshot(int frameNumber, const std::string& path);

        // Singleton access (optional)
        static Window& GetInstance();

private:
        void CaptureScreenshot(const std::string& path);

        SDL_Window* mWindow;
        SDL_GLContext mContext;
        int mWidth;
        int mHeight;
        bool mFullscreen;
        int mGlMajorVersion;
        int mGlMinorVersion;
        unsigned int mGlobalVAO = 0;
        long mFrameCounter = 0;
        bool mScreenshotPending = false;
        std::string mScreenshotPath;

        static int sAutoScreenshotFrame;
        static std::string sAutoScreenshotPath;

        // Prevent copying
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
};

// Global window instance for convenience (replaces eglInit/eglTerm globals)
extern Window* gWindow;

#endif // WINDOW_H
