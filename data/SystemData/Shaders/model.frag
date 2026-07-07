#include "common/lighting.glsl"
#include "common/pbr.glsl"
#include "common/csm.glsl"

uniform vec3  u_lightDir[5];
uniform vec4  u_lightDiffuseColour[5];
uniform vec4  u_lightSpecularColour[5];
uniform vec4  u_lightAmbientColour[5];
uniform float u_specularAmount;
uniform float u_specularExponent;

// PBR-lite controls (see common/pbr.glsl). u_usePBR selects between the new
// Cook-Torrance path (default) and the legacy Phong processLight() path.
uniform float u_usePBR;
uniform float u_roughness;
uniform float u_metallic;
uniform vec3  u_shCoeffs[9];
uniform float u_shAmbientScale;

VARYING vec4 v_colour;
VARYING LOWP vec3 v_normal;
VARYING vec3 v_worldPos;

void main()
{
    vec3 normal = normalize(v_normal);

    // Sun visibility from the cascaded shadow map (1.0 when CSM disabled).
    float sunVis = csmVisibility(v_worldPos);

    if (u_usePBR > 0.5)
    {
        vec3 albedo  = v_colour.rgb;
        vec3 V       = vec3(0.0, 0.0, 1.0);
        vec3 F0      = mix(vec3(0.04), albedo, u_metallic);
        vec3 direct  = evalDirectPBR(albedo, u_metallic, u_roughness, F0,
                                     normal, V, u_lightDir[0], u_lightDiffuseColour[0].rgb);
        vec3 ambient = evalSHIrradiance(u_shCoeffs, normal) * albedo * (1.0 - u_metallic) * u_shAmbientScale;
        // Only the direct SUN term is shadowed; SH ambient stays unshadowed.
        FRAGCOLOR = vec4(direct * sunVis + ambient, v_colour.a);
    }
    else
    {
        // Light 0 is the sun - shadow only its contribution.
        FRAGCOLOR  = processLight(v_colour, normal, u_lightDir[0], u_lightDiffuseColour[0], u_lightSpecularColour[0], u_lightAmbientColour[0], u_specularAmount, u_specularExponent) * sunVis;
        FRAGCOLOR += processLight(v_colour, normal, u_lightDir[1], u_lightDiffuseColour[1], u_lightSpecularColour[1], u_lightAmbientColour[1], u_specularAmount, u_specularExponent);
        FRAGCOLOR += processLight(v_colour, normal, u_lightDir[2], u_lightDiffuseColour[2], u_lightSpecularColour[2], u_lightAmbientColour[2], u_specularAmount, u_specularExponent);
        FRAGCOLOR += processLight(v_colour, normal, u_lightDir[3], u_lightDiffuseColour[3], u_lightSpecularColour[3], u_lightAmbientColour[3], u_specularAmount, u_specularExponent);
        FRAGCOLOR += processLight(v_colour, normal, u_lightDir[4], u_lightDiffuseColour[4], u_lightSpecularColour[4], u_lightAmbientColour[4], u_specularAmount, u_specularExponent);
        FRAGCOLOR.a = v_colour.a;
    }
}
