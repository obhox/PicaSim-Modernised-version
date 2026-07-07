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
    SHADER_SKY_HOSEK,           // procedural dynamic sky
    SHADER_OVERLAY,
    SHADER_MODEL,
    SHADER_TEXTUREDMODEL,
    SHADER_TEXTUREDMODELSEPARATESPECULAR,
    SHADER_PLAIN,
    SHADER_TERRAIN,
    SHADER_TERRAIN_PANORAMA,
    SHADER_TERRAIN_SPLAT,       // opt-in height/slope splatting (<TerrainLayers>)
    SHADER_PLAIN_WATER,         // opt-in enhanced water (mEnhancedWater)
    SHADER_SHADOW,
    SHADER_SMOKE,
    SHADER_SHADOWCAST,          // depth-only CSM caster
    SHADER_TERRAIN_SHADOW_CSM,  // CSM terrain shadow-receiver decal
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
