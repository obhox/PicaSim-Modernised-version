// Terrain splatting fragment shader (opt-in via <TerrainLayers>).
//
// Blends up to 4 layer textures by world height + geometric slope, with
// triplanar sampling (blend by |normal| axes) to avoid stretching on steep
// slopes, and a distance fade that reverts the high-frequency detail back to
// the original baked terrain look far from the camera.
//
// SHADING: the existing terrain look bakes sun / normal lighting AND the base
// terrain colour into u_lightmap (was texture0 in terrain.frag). We reuse its
// *luminance* as the light+AO term and colour the surface with the splat
// albedo, so hills/valleys stay shaded and the CSM shadow-receiver decal (drawn
// as a separate blended pass over the same mesh) keeps working unchanged.
//
// NORMAL: the terrain vert carries no normal attribute, so the geometric face
// normal is reconstructed from screen-space derivatives of the world position:
// N = normalize(cross(dFdx(worldPos), dFdy(worldPos))). This is faceted per
// triangle (matches the LOD geometry) which is fine for slope selection.

#define MAX_LAYERS 4

uniform sampler2D u_lightmap;               // pre-lit basic texture (RGB = colour*light)
uniform sampler2D u_layerTex[MAX_LAYERS];   // layer albedos
uniform int   u_numLayers;
uniform vec2  u_layerHeight[MAX_LAYERS];    // (min,max) world-Z window
uniform vec2  u_layerSlope[MAX_LAYERS];     // (min,max) slope window, degrees
uniform float u_layerScale[MAX_LAYERS];     // world units per texture tile
uniform vec3  u_cameraPos;                  // world-space camera position
uniform float u_shadeGain;                  // compensates the albedo*luma darkening
uniform float u_detailFadeStart;            // distance where splat detail starts to fade
uniform float u_detailFadeEnd;              // distance where terrain reverts to baked look

VARYING vec2 v_lightCoord;
VARYING vec3 v_worldPos;

// Trapezoidal band: ~1 inside [lo,hi], feathered across ~25% of the window.
float band(float x, float lo, float hi)
{
    float feather = max((hi - lo) * 0.25, 0.001);
    return smoothstep(lo - feather, lo + feather, x) *
           (1.0 - smoothstep(hi - feather, hi + feather, x));
}

// Triplanar sample of one layer using the world position and axis blend weights.
vec3 triplanar(sampler2D tex, vec3 wp, vec3 bw, float scale)
{
    float inv = 1.0 / max(scale, 0.001);
    vec3 cx = TEXTURE2D(tex, wp.yz * inv).rgb;   // x-facing plane
    vec3 cy = TEXTURE2D(tex, wp.zx * inv).rgb;   // y-facing plane
    vec3 cz = TEXTURE2D(tex, wp.xy * inv).rgb;   // z-facing plane (top-down)
    return bw.x * cx + bw.y * cy + bw.z * cz;
}

void main()
{
    vec3 lightRGB = TEXTURE2D(u_lightmap, v_lightCoord).rgb;
    float lightLum = dot(lightRGB, vec3(0.299, 0.587, 0.114));

    // Geometric face normal from world-position derivatives (world +Z up).
    vec3 N = normalize(cross(dFdx(v_worldPos), dFdy(v_worldPos)));
    float nz = abs(N.z);
    float height = v_worldPos.z;
    float slopeDeg = degrees(acos(clamp(nz, 0.0, 1.0)));

    // Triplanar axis weights (flat ground -> ~top-down, matching legacy detail).
    vec3 bw = abs(N);
    bw = pow(bw, vec3(4.0));                 // sharpen so flats stay top-down
    bw /= max(bw.x + bw.y + bw.z, 0.001);

    // Accumulate the layers. Unrolled with CONSTANT sampler-array indices (rather
    // than a dynamic loop index) for maximum GLSL-compiler portability. Each block
    // is gated on u_numLayers so unused layers contribute nothing.
    vec3 albedo = vec3(0.0);
    float totalW = 0.0;

    vec3 firstCol = triplanar(u_layerTex[0], v_worldPos, bw, u_layerScale[0]);
    {
        float w = band(height, u_layerHeight[0].x, u_layerHeight[0].y) *
                  band(slopeDeg, u_layerSlope[0].x, u_layerSlope[0].y);
        albedo += w * firstCol; totalW += w;
    }
    if (u_numLayers > 1)
    {
        vec3 c = triplanar(u_layerTex[1], v_worldPos, bw, u_layerScale[1]);
        float w = band(height, u_layerHeight[1].x, u_layerHeight[1].y) *
                  band(slopeDeg, u_layerSlope[1].x, u_layerSlope[1].y);
        albedo += w * c; totalW += w;
    }
    if (u_numLayers > 2)
    {
        vec3 c = triplanar(u_layerTex[2], v_worldPos, bw, u_layerScale[2]);
        float w = band(height, u_layerHeight[2].x, u_layerHeight[2].y) *
                  band(slopeDeg, u_layerSlope[2].x, u_layerSlope[2].y);
        albedo += w * c; totalW += w;
    }
    if (u_numLayers > 3)
    {
        vec3 c = triplanar(u_layerTex[3], v_worldPos, bw, u_layerScale[3]);
        float w = band(height, u_layerHeight[3].x, u_layerHeight[3].y) *
                  band(slopeDeg, u_layerSlope[3].x, u_layerSlope[3].y);
        albedo += w * c; totalW += w;
    }

    // Normalise; fall back to the base layer where nothing matched (avoids black).
    if (totalW > 0.0001)
        albedo /= totalW;
    else
        albedo = firstCol;

    vec3 near = albedo * lightLum * u_shadeGain;   // splat-coloured, baked-lit
    vec3 far  = lightRGB;                            // original baked terrain look

    float dist = length(v_worldPos - u_cameraPos);
    float fade = smoothstep(u_detailFadeStart, u_detailFadeEnd, dist);
    vec3 col = mix(near, far, fade);

    FRAGCOLOR = vec4(col, 1.0);
}
