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
// Procedural "dynamic sky" (Preetham/Hosek-Wilkie-style analytic atmosphere).
// Fullscreen pass; reconstructs world ray directions from the inverse
// rotation-only view-projection and evaluates the sky in the fragment shader.
class ProceduralSkyShader : public Shader
{
public:
    void Init() OVERRIDE;
    int u_invViewProjRot;
    int u_sunDir;
    int u_perezY, u_perezx, u_perezy;   // Perez A..E for CIE Y, x, y (float[5] each)
    int u_zenith;                       // zenith Y, x, y
    int u_sunTheta;
    int u_skyBrightness;
    int u_cloudCover;
    int u_time;
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
    // CSM receiving uniforms (see common/csm.glsl). May be -1 when CSM code is
    // compiled out by the driver as unreachable. u_worldMatrix feeds v_worldPos.
    int u_worldMatrix, u_csmEnabled, u_shadowMap, u_cascadeViewProj, u_csmBias;
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
// Terrain splatting program (opt-in via <TerrainLayers>). Blends up to 4 layer
// textures by height + slope with triplanar sampling and a distance fade. The
// layer uniforms are arrays; the [0] locations index the whole array.
class TerrainSplatShader : public Shader
{
public:
    static const int MAX_LAYERS = 4;
    void Init() OVERRIDE;
    int u_mvpMatrix, u_textureMatrix0, a_position;
    int u_lightmap, u_layerTex, u_numLayers;
    int u_layerHeight, u_layerSlope, u_layerScale;
    int u_cameraPos, u_shadeGain, u_detailFadeStart, u_detailFadeEnd;
};

//======================================================================================================================
// Enhanced water/plain program (opt-in via FrameworkSettings.mEnhancedWater).
// Adds a scrolling ripple normal, fresnel sky tint and a subtle sun glint on top
// of the legacy plain look.
class PlainWaterShader : public Shader
{
public:
    void Init() OVERRIDE;
    int u_mvpMatrix, u_textureMatrix, a_position, a_colour;
    int u_texture, u_cameraPos, u_lightDir, u_skyColour, u_sunColour, u_time;
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

//======================================================================================================================
// Depth-only shadow caster program (renders dynamic model casters into a CSM
// cascade). Position only.
class ShadowCastShader : public Shader
{
public:
    void Init() OVERRIDE;
    int u_mvpMatrix, a_position;
};

//======================================================================================================================
// CSM terrain shadow-receiver (decal) program. Draws the terrain heightfield,
// sampling the shadow map and outputting a blended darkening where dynamic model
// casters occlude the sun.
class TerrainShadowCsmShader : public Shader
{
public:
    void Init() OVERRIDE;
    int u_mvpMatrix, a_position;
    int u_csmEnabled, u_shadowMap, u_cascadeViewProj, u_csmBias, u_shadowStrength;
};



#endif
