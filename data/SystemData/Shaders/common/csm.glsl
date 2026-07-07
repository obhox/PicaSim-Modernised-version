// Cascaded Shadow Map receiving helper (fragment stage).
//
// csmVisibility(worldPos) returns the sun (light 0) visibility in [0,1]:
//   1.0 = fully lit, 0.0 = fully shadowed.
//
// Everything is guarded by u_csmEnabled: when it is < 0.5 the function returns
// 1.0 immediately, so shaders that include this file behave EXACTLY as before
// unless CSM is turned on (mShadowMode == 2). Only the SUN direct term should be
// multiplied by this visibility - ambient / SH stays unshadowed.
//
// Cascade selection is done by projecting the world position into each cascade's
// light-clip space and using the first cascade whose projected coordinate lands
// inside the [0,1] shadow-map footprint (a UV-range test). This avoids needing
// the camera view-space depth in the shader and is robust to the engine's matrix
// conventions.
//
// NUM_CASCADES must match ShadowManager::NUM_CASCADES and SHADOW_MAP_SIZE must
// match ShadowManager::SHADOW_SIZE.

#ifndef CSM_GLSL_INCLUDED
#define CSM_GLSL_INCLUDED

#define CSM_NUM_CASCADES 3
#define CSM_SHADOW_SIZE  2048.0

uniform float               u_csmEnabled;      // 0 = off (default), 1 = on
uniform sampler2DArrayShadow u_shadowMap;       // depth array, hardware PCF
uniform mat4                u_cascadeViewProj[CSM_NUM_CASCADES]; // world -> light clip
uniform float               u_csmBias;          // depth bias (tuning knob)

float csmSampleLayer(int layer, vec2 uv, float refDepth)
{
    // 3x3 hardware-PCF box filter.
    float texel = 1.0 / CSM_SHADOW_SIZE;
    float sum = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec2 o = vec2(float(x), float(y)) * texel;
            sum += texture(u_shadowMap, vec4(uv + o, float(layer), refDepth));
        }
    }
    return sum * (1.0 / 9.0);
}

float csmVisibility(vec3 worldPos)
{
    if (u_csmEnabled < 0.5)
        return 1.0;

    for (int i = 0; i < CSM_NUM_CASCADES; ++i)
    {
        vec4 clip = u_cascadeViewProj[i] * vec4(worldPos, 1.0);
        vec3 ndc  = clip.xyz / clip.w;
        vec3 uvw  = ndc * 0.5 + 0.5;

        if (uvw.x > 0.0 && uvw.x < 1.0 &&
            uvw.y > 0.0 && uvw.y < 1.0 &&
            uvw.z > 0.0 && uvw.z < 1.0)
        {
            float refDepth = uvw.z - u_csmBias;
            return csmSampleLayer(i, uvw.xy, refDepth);
        }
    }
    return 1.0; // outside every cascade -> treat as lit
}

#endif // CSM_GLSL_INCLUDED
