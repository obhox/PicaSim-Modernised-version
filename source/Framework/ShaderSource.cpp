#include "ShaderSource.h"
#include "Graphics.h"
#include "Trace.h"
#include "../Platform/Platform.h"

#include <sys/stat.h>

namespace ShaderSource
{

const char* Dir()
{
    return "SystemData/Shaders/";
}

//======================================================================================================================
int64_t FileModTime(const std::string& path)
{
#if defined(_WIN32)
    struct _stat st;
    if (_stat(path.c_str(), &st) == 0)
        return (int64_t)st.st_mtime;
#else
    struct stat st;
    if (stat(path.c_str(), &st) == 0)
        return (int64_t)st.st_mtime;
#endif
    return 0;
}

//======================================================================================================================
// The prelude picks the GLSL version and defines the compatibility macros the
// on-disk shaders are written against, so the shader files themselves are
// version-agnostic. The renderer targets a modern core profile everywhere:
// GLSL 330/410 core on desktop, GLSL 300 es on mobile - all of which use
// in/out qualifiers, an explicit fragment output and texture() sampling.
static std::string BuildPrelude(Stage stage)
{
    std::string p;
    bool es = false;

#if defined(PS_PLATFORM_ANDROID) || defined(PS_PLATFORM_IOS)
    // OpenGL ES 3.0
    p += "#version 300 es\n";
    es = true;
#elif defined(PICASIM_MACOS)
    // macOS OpenGL 4.1 Core
    p += "#version 410 core\n";
#else
    // Other desktop (Windows/Linux) - OpenGL 3.3 Core
    p += "#version 330 core\n";
#endif

    if (es)
    {
        // Precision qualifiers are meaningful (and required for some types) on ES.
        p += (stage == STAGE_VERTEX) ? "precision highp float;\n" : "precision mediump float;\n";
        p += "#define LOWP lowp\n";
        p += "#define MEDIUMP mediump\n";
    }
    else
    {
        // No-ops on desktop core.
        p += "#define LOWP\n";
        p += "#define MEDIUMP\n";
    }

    // Sampling: modern GLSL uses the overloaded texture() for both plain and
    // LOD-biased sampling.
    p += "#define TEXTURE2D texture\n";
    p += "#define TEXTURE2D_BIAS texture\n";

    if (stage == STAGE_VERTEX)
    {
        p += "#define ATTRIBUTE in\n";
        p += "#define VARYING out\n";
    }
    else
    {
        p += "#define VARYING in\n";
        // Explicit fragment output replaces the old gl_FragColor.
        p += "out vec4 ps_FragColour;\n";
        p += "#define FRAGCOLOR ps_FragColour\n";
    }

    return p;
}

//======================================================================================================================
static bool ReadShaderFile(const std::string& path, std::string& out)
{
    return FileSystem::ReadTextFile(path, out);
}

//======================================================================================================================
// Recursively inline #include "..." directives. included set guards against
// double-inclusion and include cycles. Records touched files in sourceFiles.
static bool ResolveIncludes(const std::string& fileName, std::string& out,
                            std::vector<std::string>& sourceFiles,
                            std::vector<std::string>& includedStack,
                            int depth)
{
    if (depth > 16)
    {
        TRACE("Shader include depth exceeded resolving %s", fileName.c_str());
        return false;
    }

    std::string path = std::string(Dir()) + fileName;
    sourceFiles.push_back(path);

    std::string src;
    if (!ReadShaderFile(path, src))
    {
        TRACE("Failed to read shader source %s", path.c_str());
        return false;
    }

    // Directory of the current file, so includes resolve relative to it.
    std::string baseDir;
    size_t slash = fileName.find_last_of('/');
    if (slash != std::string::npos)
        baseDir = fileName.substr(0, slash + 1);

    size_t pos = 0;
    while (pos < src.size())
    {
        size_t lineEnd = src.find('\n', pos);
        if (lineEnd == std::string::npos)
            lineEnd = src.size();
        std::string line = src.substr(pos, lineEnd - pos);

        // Detect a leading (whitespace-tolerant) #include "path"
        size_t firstNonWs = line.find_first_not_of(" \t");
        bool handled = false;
        if (firstNonWs != std::string::npos && line.compare(firstNonWs, 8, "#include") == 0)
        {
            size_t q1 = line.find('"', firstNonWs + 8);
            size_t q2 = (q1 == std::string::npos) ? std::string::npos : line.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos)
            {
                std::string incName = line.substr(q1 + 1, q2 - q1 - 1);
                // Resolve relative to including file's directory.
                std::string resolved = incName;
                if (!incName.empty() && incName[0] != '/' && !baseDir.empty() &&
                    incName.compare(0, baseDir.size(), baseDir) != 0)
                {
                    // Only prepend baseDir when not already an explicit path.
                    if (incName.find('/') == std::string::npos)
                        resolved = baseDir + incName;
                }

                bool already = false;
                for (const std::string& s : includedStack)
                    if (s == resolved) { already = true; break; }

                if (!already)
                {
                    includedStack.push_back(resolved);
                    if (!ResolveIncludes(resolved, out, sourceFiles, includedStack, depth + 1))
                        return false;
                }
                handled = true;
            }
        }

        if (!handled)
        {
            out += line;
            out += '\n';
        }

        pos = lineEnd + 1;
    }
    return true;
}

//======================================================================================================================
std::string Build(const std::string& fileName, Stage stage,
                  std::vector<std::string>* outSourceFiles)
{
    std::vector<std::string> sourceFiles;
    std::vector<std::string> includedStack;
    includedStack.push_back(fileName);

    std::string body;
    if (!ResolveIncludes(fileName, body, sourceFiles, includedStack, 0))
        return std::string();

    if (outSourceFiles)
        *outSourceFiles = sourceFiles;

    return BuildPrelude(stage) + body;
}

} // namespace ShaderSource
