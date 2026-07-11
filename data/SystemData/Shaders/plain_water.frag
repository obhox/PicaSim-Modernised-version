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
const float RIPPLE_STRENGTH  = 0.14;  // how much the ripple tilts the normal
const float FRESNEL_STRENGTH = 0.60;  // max sky reflection at grazing angles
const float GLINT_STRENGTH   = 0.75;  // reflected-sun intensity
const float GLINT_EXPONENT   = 200.0; // glint tightness

void main()
{
    vec4 base = TEXTURE2D(u_texture, v_texCoord) * v_colour;

    // Deepen the head-on water a touch (cooler / darker) so it reads as water
    // rather than flat terrain colour; the sky reflection builds up on top.
    vec3 deep = base.rgb * vec3(0.74, 0.84, 0.95);

    // Procedural ripple normal (water plane up = +Z) - a few octaves for glitter.
    vec2 p = v_worldPos.xy;
    float t = u_time;
    vec2 grad = vec2(0.0);
    grad += vec2(0.30, 0.00) * sin(dot(p, vec2( 0.080, 0.050)) + t * 0.9);
    grad += vec2(0.00, 0.24) * sin(dot(p, vec2(-0.040, 0.090)) + t * 1.3);
    grad += 0.14 * cos(dot(p, vec2(0.150, -0.110)) + t * 1.9);
    grad += 0.09 * vec2(sin(dot(p, vec2(0.31, 0.02)) + t * 2.7),
                        cos(dot(p, vec2(0.03, 0.29)) + t * 2.3));
    vec3 N = normalize(vec3(-grad.x * RIPPLE_STRENGTH, -grad.y * RIPPLE_STRENGTH, 1.0));

    vec3 V = normalize(u_cameraPos - v_worldPos);
    float fres = pow(1.0 - clamp(dot(N, V), 0.0, 1.0), 5.0);

    // Reflect the sky/horizon colour, strongest at grazing angles.
    vec3 col = mix(deep, u_skyColour, clamp(fres * FRESNEL_STRENGTH + 0.05, 0.0, 0.85));

    // Reflected sun: a proper mirror streak along the reflection vector (not just
    // a Blinn lobe), fresnel-boosted so the glitter path brightens to the horizon.
    vec3 L = normalize(-u_lightDir);
    vec3 R = reflect(-V, N);
    float sun = pow(max(dot(R, L), 0.0), GLINT_EXPONENT) * GLINT_STRENGTH;
    col += u_sunColour * sun * (0.4 + fres);

    FRAGCOLOR = vec4(col, base.a);
}
