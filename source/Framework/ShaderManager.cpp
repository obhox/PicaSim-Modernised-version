#include "Shaders.h"
#include "ShaderManager.h"
#include "LoadingScreenHelper.h"
#include "Trace.h"
#include "../Platform/Platform.h"

//======================================================================================================================
ShaderManager* ShaderManager::mInstance = 0;

//======================================================================================================================
ShaderManager& ShaderManager::GetInstance()
{
    IwAssert(ROWLHOUSE, mInstance != 0);
    return *mInstance;
}
//======================================================================================================================
ShaderManager::ShaderManager(LoadingScreenHelper* loadingScreen)
{
    // (Previously queried GL_MAX_*_VECTORS limits here for diagnostics; those
    // enums are ES-only and are GL_INVALID_ENUM on a desktop core profile, and
    // the values were unused - removed.)
    mShaders[SHADER_SIMPLE]                        = new SimpleShader();
    mShaders[SHADER_CONTROLLER]                    = new ControllerShader();
    mShaders[SHADER_SKYBOX]                        = new SkyboxShader();
    mShaders[SHADER_SKY_HOSEK]                     = new ProceduralSkyShader();
    mShaders[SHADER_OVERLAY]                       = new OverlayShader();
    mShaders[SHADER_MODEL]                         = new ModelShader();
    mShaders[SHADER_TEXTUREDMODEL]                 = new TexturedModelShader();
    mShaders[SHADER_TEXTUREDMODELSEPARATESPECULAR] = new TexturedModelSeparateSpecularShader();
    mShaders[SHADER_PLAIN]                         = new PlainShader();
    mShaders[SHADER_TERRAIN]                       = new TerrainShader();
    mShaders[SHADER_TERRAIN_PANORAMA]              = new TerrainPanoramaShader();
    mShaders[SHADER_TERRAIN_SPLAT]                 = new TerrainSplatShader();
    mShaders[SHADER_PLAIN_WATER]                   = new PlainWaterShader();
    mShaders[SHADER_SHADOW]                        = new ShadowShader();
    mShaders[SHADER_SMOKE]                         = new SmokeShader();
    mShaders[SHADER_SHADOWCAST]                    = new ShadowCastShader();
    mShaders[SHADER_TERRAIN_SHADOW_CSM]            = new TerrainShadowCsmShader();

    if (loadingScreen)
        loadingScreen->Update("Compiling shaders");
    for (int i = 0 ; i != NUM_SHADERS ; ++i)
    {
        mShaders[i]->Init();
        IwAssert(ROWLHOUSE, mShaders[i]->mShaderProgram > 0);
    }
}

//======================================================================================================================
void ShaderManager::Init(LoadingScreenHelper* loadingScreen)
{
    TRACE_FUNCTION_ONLY(1);
    IwAssert(ROWLHOUSE, !mInstance);
    mInstance = new ShaderManager(loadingScreen);
}

//======================================================================================================================
void ShaderManager::Terminate()
{
    TRACE_FUNCTION_ONLY(1);
    IwAssert(ROWLHOUSE, mInstance);

    for (int i = 0 ; i != NUM_SHADERS ; ++i)
    {
        mInstance->mShaders[i]->Terminate();
        delete mInstance->mShaders[i];
    }

    delete mInstance;
    mInstance = 0;
}

//======================================================================================================================
const Shader* ShaderManager::GetShader(ShaderType type)
{
    return mShaders[type];
}

//======================================================================================================================
void ShaderManager::PollHotReload()
{
#ifndef NDEBUG
    if (!mInstance)
        return;

    // Throttle the file-system stat() sweep to a few times per second - it runs
    // every frame otherwise.
    static uint64_t sLastCheckMs = 0;
    uint64_t nowMs = Timer::GetMilliseconds();
    if (nowMs - sLastCheckMs < 300)
        return;
    sLastCheckMs = nowMs;

    for (int i = 0; i < NUM_SHADERS; ++i)
    {
        if (mInstance->mShaders[i]->NeedsReload())
            mInstance->mShaders[i]->Reload();
    }
#endif
}

