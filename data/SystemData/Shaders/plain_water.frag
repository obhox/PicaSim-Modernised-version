// Enhanced water/plain fragment shader (opt-in via FrameworkSettings.mEnhancedWater).
//
// Starts from EXACTLY the legacy plain look (detail texture * vertex colour) and
// adds, subtly:
//   * a cheap procedural scrolling ripple normal (sum of sines - no texture dep),
//   * a fresnel term tinting toward a sky/horizon colour at grazing angles,
//   * a small specular sun glint.
// Looking straight down the fresnel ~= 0, so still water is near-identical to the
// legacy plain; the tint and glint only build up toward the horizon. This shader
// is only ever selected when mEnhancedWater is set, so the default path (plain.frag)
// is untouched and bit-for-bit unchanged.
uniform sampler2D u_texture;
uniform vec3  u_cameraPos;    // world-space camera
uniform vec3  u_lightDir;     // light travel direction (dirToLight = -u_lightDir)
uniform vec3  u_skyColour;    // horizon/sky tint reflected at grazing angles
uniform vec3  u_sunColour;    // glint colour
uniform float u_time;         // seconds, for ripple scrolling

VARYING vec2 v_texCoord;
VARYING vec4 v_colour;
VARYING vec3 v_worldPos;

// Tuning knobs (kept in-shader to avoid extra CPU plumbing).
const float RIPPLE_STRENGTH = 0.10;  // how much the ripple tilts the normal
const float FRESNEL_STRENGTH = 0.35; // max sky tint at grazing angles
const float GLINT_STRENGTH = 0.35;   // sun glint intensity
const float GLINT_EXPONENT = 120.0;  // glint tightness

void main()
{
    vec4 base = TEXTURE2D(u_texture, v_texCoord) * v_colour;

    // Procedural ripple normal (water plane up = +Z).
    vec2 p = v_worldPos.xy;
    float t = u_time;
    vec2 grad = vec2(0.0);
    grad += vec2(0.30, 0.00) * sin(dot(p, vec2( 0.080, 0.050)) + t * 0.9);
    grad += vec2(0.00, 0.24) * sin(dot(p, vec2(-0.040, 0.090)) + t * 1.3);
    grad += 0.14 * cos(dot(p, vec2(0.150, -0.110)) + t * 1.9);
    vec3 N = normalize(vec3(-grad.x * RIPPLE_STRENGTH, -grad.y * RIPPLE_STRENGTH, 1.0));

    vec3 V = normalize(u_cameraPos - v_worldPos);
    float fres = pow(1.0 - clamp(dot(N, V), 0.0, 1.0), 5.0);

    vec3 col = mix(base.rgb, u_skyColour, fres * FRESNEL_STRENGTH);

    // Subtle sun glint.
    vec3 L = normalize(-u_lightDir);
    vec3 H = normalize(L + V);
    float glint = pow(max(dot(N, H), 0.0), GLINT_EXPONENT) * GLINT_STRENGTH;
    col += u_sunColour * glint;

    FRAGCOLOR = vec4(col, base.a);
}
