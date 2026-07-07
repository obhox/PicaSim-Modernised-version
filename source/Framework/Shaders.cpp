#include "Shaders.h"
#include "ShaderSource.h"
#include "Trace.h"

// GLSL sources live on disk under data/SystemData/Shaders/ and are loaded and
// preprocessed by ShaderSource. See that module for the per-platform prelude
// that defines ATTRIBUTE / VARYING / FRAGCOLOR / TEXTURE2D / LOWP etc.

//======================================================================================================================
void Shader::Init(const char* vertexShaderStr, const char* fragmentShaderStr)
{
    mVertexShaderStr = vertexShaderStr;
    mFragmentShaderStr = fragmentShaderStr;
    mShaderProgram = esLoadProgram(vertexShaderStr, fragmentShaderStr);
}

//======================================================================================================================
void Shader::InitFromFiles(const char* vertexFileName, const char* fragmentFileName)
{
    mVertexFileName = vertexFileName;
    mFragmentFileName = fragmentFileName;

    std::vector<std::string> vertFiles, fragFiles;
    std::string vertSrc = ShaderSource::Build(mVertexFileName, ShaderSource::STAGE_VERTEX, &vertFiles);
    std::string fragSrc = ShaderSource::Build(mFragmentFileName, ShaderSource::STAGE_FRAGMENT, &fragFiles);

    if (vertSrc.empty() || fragSrc.empty())
    {
        TRACE("Shader::InitFromFiles failed to load %s / %s", vertexFileName, fragmentFileName);
        mShaderProgram = 0;
        return;
    }

    mShaderProgram = esLoadProgram(vertSrc.c_str(), fragSrc.c_str());

    // Record modification times of every source file (vertex + fragment +
    // their includes) so the debug watcher can detect edits.
    mSourceFileTimes.clear();
    for (const std::string& f : vertFiles)
        mSourceFileTimes.push_back(std::make_pair(f, ShaderSource::FileModTime(f)));
    for (const std::string& f : fragFiles)
        mSourceFileTimes.push_back(std::make_pair(f, ShaderSource::FileModTime(f)));
}

//======================================================================================================================
bool Shader::NeedsReload() const
{
    for (size_t i = 0; i < mSourceFileTimes.size(); ++i)
    {
        if (ShaderSource::FileModTime(mSourceFileTimes[i].first) != mSourceFileTimes[i].second)
            return true;
    }
    return false;
}

//======================================================================================================================
void Shader::Reload()
{
    if (mVertexFileName.empty() || mFragmentFileName.empty())
        return;

    GLuint oldProgram = mShaderProgram;
    // Re-run the subclass Init(), which reloads the files and re-queries the
    // uniform/attribute locations into this object.
    Init();
    if (mShaderProgram != 0 && mShaderProgram != oldProgram)
    {
        if (oldProgram != 0)
            glDeleteProgram(oldProgram);
        TRACE("Reloaded shader %s / %s", mVertexFileName.c_str(), mFragmentFileName.c_str());
    }
    else
    {
        // Reload failed - keep the old, working program.
        mShaderProgram = oldProgram;
    }
}

//======================================================================================================================
void Shader::Terminate()
{
    if (mShaderProgram > 0)
        glDeleteProgram(mShaderProgram);
    mShaderProgram = 0;
}

//======================================================================================================================
void Shader::Use() const
{
    IwAssert(ROWLHOUSE, mShaderProgram);
    glUseProgram(mShaderProgram);
}

//======================================================================================================================
static int getUniformLocation(int shaderProgram, const char* str)
{
    int loc = glGetUniformLocation(shaderProgram, str);
    IwAssert(ROWLHOUSE, loc >= 0);
    return loc;
}

//======================================================================================================================
// Non-asserting variant for optional uniforms (e.g. PBR controls a driver may
// strip if a branch is provably unreachable). Returns -1 if not present.
static int getUniformLocationOpt(int shaderProgram, const char* str)
{
    return glGetUniformLocation(shaderProgram, str);
}

//======================================================================================================================
static int getAttribLocation(int shaderProgram, const char* str)
{
    int loc = glGetAttribLocation(shaderProgram, str);
    IwAssert(ROWLHOUSE, loc >= 0);
    return loc;
}

//======================================================================================================================
void SimpleShader::Init()
{
    Shader::InitFromFiles("simple.vert", "simple.frag");
    u_mvpMatrix = getUniformLocation(mShaderProgram, "u_mvpMatrix");
    a_position  = getAttribLocation(mShaderProgram, "a_position");
    a_colour    = getAttribLocation(mShaderProgram, "a_colour");
}

//======================================================================================================================
void ControllerShader::Init()
{
    Shader::InitFromFiles("controller.vert", "controller.frag");
    u_mvpMatrix = getUniformLocation(mShaderProgram, "u_mvpMatrix");
    a_position  = getAttribLocation(mShaderProgram, "a_position");
    u_colour  = getUniformLocation(mShaderProgram, "u_colour");
}

//======================================================================================================================
void SkyboxShader::Init()
{
    Shader::InitFromFiles("skybox.vert", "skybox.frag");
    u_mvpMatrix  = getUniformLocation(mShaderProgram, "u_mvpMatrix");
    a_position   = getAttribLocation(mShaderProgram, "a_position");
    a_texCoord   = getAttribLocation(mShaderProgram, "a_texCoord");
    u_texture    = getUniformLocation(mShaderProgram, "u_texture");
}

//======================================================================================================================
void OverlayShader::Init()
{
    Shader::InitFromFiles("overlay.vert", "overlay.frag");
    u_mvpMatrix = getUniformLocation(mShaderProgram, "u_mvpMatrix");
    a_position  = getAttribLocation(mShaderProgram, "a_position");
    a_texCoord  = getAttribLocation(mShaderProgram, "a_texCoord");
    u_texture   = getUniformLocation(mShaderProgram, "u_texture");
    u_colour    = getUniformLocation(mShaderProgram, "u_colour");
}

//======================================================================================================================
void ModelShader::Init()
{
    Shader::InitFromFiles("model.vert", "model.frag");
    SetupVars();
}

//======================================================================================================================
void ModelShader::SetupVars()
{
    u_mvpMatrix           = getUniformLocation(mShaderProgram, "u_mvpMatrix");
    u_normalMatrix        = getUniformLocation(mShaderProgram, "u_normalMatrix");
    lightShaderInfo[0].u_lightDir         = getUniformLocation(mShaderProgram, "u_lightDir[0]");
    lightShaderInfo[1].u_lightDir         = getUniformLocation(mShaderProgram, "u_lightDir[1]");
    lightShaderInfo[2].u_lightDir         = getUniformLocation(mShaderProgram, "u_lightDir[2]");
    lightShaderInfo[3].u_lightDir         = getUniformLocation(mShaderProgram, "u_lightDir[3]");
    lightShaderInfo[4].u_lightDir         = getUniformLocation(mShaderProgram, "u_lightDir[4]");
    lightShaderInfo[0].u_lightAmbientColour  = getUniformLocation(mShaderProgram, "u_lightAmbientColour[0]");
    lightShaderInfo[1].u_lightAmbientColour  = getUniformLocation(mShaderProgram, "u_lightAmbientColour[1]");
    lightShaderInfo[2].u_lightAmbientColour  = getUniformLocation(mShaderProgram, "u_lightAmbientColour[2]");
    lightShaderInfo[3].u_lightAmbientColour  = getUniformLocation(mShaderProgram, "u_lightAmbientColour[3]");
    lightShaderInfo[4].u_lightAmbientColour  = getUniformLocation(mShaderProgram, "u_lightAmbientColour[4]");
    lightShaderInfo[0].u_lightDiffuseColour  = getUniformLocation(mShaderProgram, "u_lightDiffuseColour[0]");
    lightShaderInfo[1].u_lightDiffuseColour  = getUniformLocation(mShaderProgram, "u_lightDiffuseColour[1]");
    lightShaderInfo[2].u_lightDiffuseColour  = getUniformLocation(mShaderProgram, "u_lightDiffuseColour[2]");
    lightShaderInfo[3].u_lightDiffuseColour  = getUniformLocation(mShaderProgram, "u_lightDiffuseColour[3]");
    lightShaderInfo[4].u_lightDiffuseColour  = getUniformLocation(mShaderProgram, "u_lightDiffuseColour[4]");
    lightShaderInfo[0].u_lightSpecularColour = getUniformLocation(mShaderProgram, "u_lightSpecularColour[0]");
    lightShaderInfo[1].u_lightSpecularColour = getUniformLocation(mShaderProgram, "u_lightSpecularColour[1]");
    lightShaderInfo[2].u_lightSpecularColour = getUniformLocation(mShaderProgram, "u_lightSpecularColour[2]");
    lightShaderInfo[3].u_lightSpecularColour = getUniformLocation(mShaderProgram, "u_lightSpecularColour[3]");
    lightShaderInfo[4].u_lightSpecularColour = getUniformLocation(mShaderProgram, "u_lightSpecularColour[4]");
    u_specularAmount      = getUniformLocation(mShaderProgram, "u_specularAmount");
    u_specularExponent    = getUniformLocation(mShaderProgram, "u_specularExponent");
    a_position            = getAttribLocation(mShaderProgram, "a_position");
    a_normal              = getAttribLocation(mShaderProgram, "a_normal");
    a_colour              = getAttribLocation(mShaderProgram, "a_colour");
    // PBR-lite (optional - both code paths are compiled so these should exist,
    // but query non-assertively in case a driver strips an unused uniform).
    u_usePBR              = getUniformLocationOpt(mShaderProgram, "u_usePBR");
    u_roughness           = getUniformLocationOpt(mShaderProgram, "u_roughness");
    u_metallic            = getUniformLocationOpt(mShaderProgram, "u_metallic");
    u_shCoeffs            = getUniformLocationOpt(mShaderProgram, "u_shCoeffs[0]");
    u_shAmbientScale      = getUniformLocationOpt(mShaderProgram, "u_shAmbientScale");
}


//======================================================================================================================
void TexturedModelShader::Init()
{
    Shader::InitFromFiles("texturedmodel.vert", "texturedmodel.frag");
    ModelShader::SetupVars();
    a_texCoord            = getAttribLocation(mShaderProgram, "a_texCoord");
    u_texture             = getUniformLocation(mShaderProgram, "u_texture");
    u_texBias             = getUniformLocation(mShaderProgram, "u_texBias");
}

//======================================================================================================================
void TexturedModelSeparateSpecularShader::Init()
{
    Shader::InitFromFiles("texturedmodel.vert", "texturedmodel_separatespecular.frag");
    ModelShader::SetupVars();
    a_texCoord            = getAttribLocation(mShaderProgram, "a_texCoord");
    u_texture             = getUniformLocation(mShaderProgram, "u_texture");
    u_texBias             = getUniformLocation(mShaderProgram, "u_texBias");
}

//======================================================================================================================
void PlainShader::Init()
{
    Shader::InitFromFiles("plain.vert", "plain.frag");
    u_mvpMatrix     = getUniformLocation(mShaderProgram, "u_mvpMatrix");
    u_textureMatrix = getUniformLocation(mShaderProgram, "u_textureMatrix");
    a_position      = getAttribLocation(mShaderProgram, "a_position");
    a_colour        = getAttribLocation(mShaderProgram, "a_colour");
    u_texture       = getUniformLocation(mShaderProgram, "u_texture");
}

//======================================================================================================================
void TerrainShader::Init()
{
    Shader::InitFromFiles("terrain.vert", "terrain.frag");
    u_mvpMatrix      = getUniformLocation(mShaderProgram, "u_mvpMatrix");
    u_textureMatrix0 = getUniformLocation(mShaderProgram, "u_textureMatrix0");
    u_textureMatrix1 = getUniformLocation(mShaderProgram, "u_textureMatrix1");
    a_position       = getAttribLocation(mShaderProgram, "a_position");
    u_texture0       = getUniformLocation(mShaderProgram, "u_texture0");
    u_texture1       = getUniformLocation(mShaderProgram, "u_texture1");
}

//======================================================================================================================
void TerrainPanoramaShader::Init()
{
    Shader::InitFromFiles("terrain_panorama.vert", "terrain_panorama.frag");
    u_mvpMatrix      = getUniformLocation(mShaderProgram, "u_mvpMatrix");
    a_position       = getAttribLocation(mShaderProgram, "a_position");
}

//======================================================================================================================
void ShadowShader::Init()
{
    Shader::InitFromFiles("shadow.vert", "shadow.frag");
    u_mvpMatrix      = getUniformLocation(mShaderProgram, "u_mvpMatrix");
    u_textureMatrix  = getUniformLocation(mShaderProgram, "u_textureMatrix");
    a_position       = getAttribLocation(mShaderProgram, "a_position");
    a_texCoord       = getAttribLocation(mShaderProgram, "a_texCoord");
    u_texture        = getUniformLocation(mShaderProgram, "u_texture");
    u_colour         = getUniformLocation(mShaderProgram, "u_colour");
}

//======================================================================================================================
void SmokeShader::Init()
{
    Shader::InitFromFiles("smoke.vert", "smoke.frag");
    u_mvpMatrix      = getUniformLocation(mShaderProgram, "u_mvpMatrix");
    a_position       = getAttribLocation(mShaderProgram, "a_position");
    a_texCoord       = getAttribLocation(mShaderProgram, "a_texCoord");
    u_colour         = getUniformLocation(mShaderProgram, "u_colour");
    u_texture        = getUniformLocation(mShaderProgram, "u_texture");
}


