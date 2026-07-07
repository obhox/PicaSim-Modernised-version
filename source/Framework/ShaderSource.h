#ifndef SHADERSOURCE_H
#define SHADERSOURCE_H

#include <string>
#include <vector>
#include <cstdint>

// Loads GLSL shader sources from disk (data/SystemData/Shaders/), resolves
// #include directives, and prepends a per-stage / per-platform prelude that
// defines the compatibility macros the on-disk shaders are written against
// (ATTRIBUTE, VARYING, FRAGCOLOR, TEXTURE2D, TEXTURE2D_BIAS, LOWP, MEDIUMP).
//
// The on-disk shaders are deliberately version-agnostic: the prelude is the one
// place that knows the target GLSL version, so raising the floor to a core
// profile (Phase 1c) is a change to BuildPrelude() alone, not to every shader.
namespace ShaderSource
{
    enum Stage { STAGE_VERTEX, STAGE_FRAGMENT };

    // Directory (relative to the data working directory) holding the shaders.
    const char* Dir();

    // Build the full, compile-ready source for the named shader file (e.g.
    // "model.vert"). Resolves #include "common/x.glsl" relative to Dir() and
    // prepends the prelude for the given stage. If outSourceFiles is non-null,
    // it is filled with every file path touched (top-level + includes) so the
    // caller can watch their modification times for hot-reload.
    // Returns an empty string if the top-level file cannot be read.
    std::string Build(const std::string& fileName, Stage stage,
                      std::vector<std::string>* outSourceFiles = nullptr);

    // Modification time of a file in seconds, or 0 if it does not exist.
    // Used by the debug hot-reload watcher.
    int64_t FileModTime(const std::string& path);
}

#endif // SHADERSOURCE_H
