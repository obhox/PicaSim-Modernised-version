#ifndef SHADERS_H
#define SHADERS_H

#include "Graphics.h"
#include <string>
#include <vector>
#include <cstdint>

//======================================================================================================================
class Shader
{
public:
    Shader() : mShaderProgram(0), mVertexShaderStr(0), mFragmentShaderStr(0) {}
    virtual ~Shader() {}
    // Legacy: compile directly from in-memory source strings (retained for any
    // external callers; the built-in shaders now load from disk).
    virtual void Init(const char* vertexShaderStr, const char* fragmentShaderStr);
    // Load, preprocess and compile the named on-disk shader files (e.g.
    // "model.vert" / "model.frag"), recording their source files for hot-reload.
    void InitFromFiles(const char* vertexFileName, const char* fragmentFileName);
    virtual void Init() = 0;
    void Terminate();
    void Use() const;

    // Debug hot-reload: has any source file changed on disk since last compile?
    bool NeedsReload() const;
    // Re-run this shader's Init() (reloads from disk and re-queries locations).
    void Reload();
protected:
    friend class ShaderManager;
    GLuint      mShaderProgram;
    const char* mVertexShaderStr;
    const char* mFragmentShaderStr;

    // Hot-reload bookkeeping (populated by InitFromFiles).
    std::string mVertexFileName;
    std::string mFragmentFileName;
    std::vector<std::pair<std::string, int64_t> > mSourceFileTimes;
};

//======================================================================================================================
class SimpleShader : public Shader
{
public:
    void Init() OVERRIDE;
    int u_mvpMatrix, a_position, a_colour;
};

//======================================================================================================================
class ControllerShader : public Shader
{
public:
    void Init() OVERRIDE;
    int u_mvpMatrix, a_position;
    int u_colour;
};

//======================================================================================================================
class SkyboxShader : public Shader
{
public:
    void Init() OVERRIDE;
    int u_mvpMatrix, a_position, a_texCoord;
    int u_texture;
};

//======================================================================================================================
class OverlayShader : public Shader
{
public:
    void Init() OVERRIDE;
    int u_mvpMatrix, a_position, a_texCoord;
    int u_texture, u_colour;
};

struct LightShaderInfo
{
    LightShaderInfo() : u_lightDir(-1), u_lightAmbientColour(-1), u_lightDiffuseColour(-1), u_lightSpecularColour(-1) {}
    int u_lightDir;
    int u_lightAmbientColour;
    int u_lightDiffuseColour;
    int u_lightSpecularColour;
};

//======================================================================================================================
class ModelShader : public Shader
{
public:
    void Init() OVERRIDE;
    void SetupVars();
    LightShaderInfo lightShaderInfo[5];
    int u_mvpMatrix, u_normalMatrix;
    int u_specularAmount, u_specularExponent, a_position, a_normal, a_colour;
    // PBR-lite uniforms (may be -1 if the driver strips them). See common/pbr.glsl.
    int u_usePBR, u_roughness, u_metallic, u_shCoeffs, u_shAmbientScale;
};

//======================================================================================================================
class TexturedModelShader : public ModelShader
{
public:
    void Init() OVERRIDE;
    int a_texCoord, u_texture, u_texBias;
};

//======================================================================================================================
class TexturedModelSeparateSpecularShader : public TexturedModelShader
{
public:
    void Init() OVERRIDE;
};

//======================================================================================================================
class PlainShader : public Shader
{
public:
    void Init() OVERRIDE;
    int u_mvpMatrix, u_textureMatrix, a_position, a_colour;
    int u_texture;
};

//======================================================================================================================
class TerrainShader : public Shader
{
public:
    void Init() OVERRIDE;
    int u_mvpMatrix, u_textureMatrix0, u_textureMatrix1, a_position;
    int u_texture0, u_texture1;
};

//======================================================================================================================
class TerrainPanoramaShader : public Shader
{
public:
    void Init() OVERRIDE;
    int u_mvpMatrix, a_position;
};

//======================================================================================================================
class ShadowShader : public Shader
{
public:
    void Init() OVERRIDE;
    int u_mvpMatrix, u_textureMatrix, a_position, a_texCoord;
    int u_texture, u_colour;
};

//======================================================================================================================
class SmokeShader : public Shader
{
public:
    void Init() OVERRIDE;
    int u_mvpMatrix, a_position, u_colour, a_texCoord;
    int u_texture;
};



#endif
