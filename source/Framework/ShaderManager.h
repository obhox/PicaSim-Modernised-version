#ifndef SHADERMANAGER_H
#define SHADERMANAGER_H

#include <vector>

class LoadingScreenHelper;
class Shader;

enum ShaderType
{
    SHADER_SIMPLE,
    SHADER_CONTROLLER,
    SHADER_SKYBOX,
    SHADER_OVERLAY,
    SHADER_MODEL,
    SHADER_TEXTUREDMODEL,
    SHADER_TEXTUREDMODELSEPARATESPECULAR,
    SHADER_PLAIN,
    SHADER_TERRAIN,
    SHADER_TERRAIN_PANORAMA,
    SHADER_SHADOW,
    SHADER_SMOKE,
    NUM_SHADERS
};

//======================================================================================================================
class ShaderManager
{
public:
    /// Gets the singleton
    static ShaderManager& GetInstance(); 

    static void Init(LoadingScreenHelper* loadingScreen);
    static void Terminate();

    const Shader* GetShader(ShaderType type);

    // Debug hot-reload: in debug builds, poll the on-disk shader sources and
    // recompile any that have changed. No-op in release builds. Safe to call
    // once per frame; it throttles its own file-system checks.
    static void PollHotReload();

private:
    ShaderManager(LoadingScreenHelper* loadingScreen);
    static ShaderManager* mInstance;
    Shader* mShaders[NUM_SHADERS];
};


#endif
