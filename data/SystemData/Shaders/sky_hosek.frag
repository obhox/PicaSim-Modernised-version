// Procedural dynamic sky - analytic Preetham/Hosek-Wilkie-style atmosphere.
// Evaluates a Perez luminance/chromaticity distribution (CIE xyY) per view
// direction, converts to linear RGB, and adds an analytic sun disc + limb glow
// and an optional cheap scrolling cirrus layer. Outputs HDR into the scene
// target (values may exceed 1 near the sun - bloom / PBR-Neutral tonemap handle
// the highlight, and under the identity tonemap they simply clip to white).

VARYING vec2 v_ndc;

uniform mat4  u_invViewProjRot;  // inverse of the rotation-only view-projection
uniform vec3  u_sunDir;          // unit vector towards the sun (world space, +Z up)
uniform float u_perezY[5];       // Perez A..E for luminance Y
uniform float u_perezx[5];       // Perez A..E for chroma x
uniform float u_perezy[5];       // Perez A..E for chroma y
uniform vec3  u_zenith;          // zenith (Y, x, y)
uniform float u_sunTheta;        // solar zenith angle (radians)
uniform float u_skyBrightness;   // overall scale (dims toward dusk)
uniform float u_cloudCover;      // 0..1 cirrus amount
uniform float u_time;            // seconds (cloud scroll)

// Perez F(theta,gamma) with coefficients A..E.
float perezF(float A, float B, float C, float D, float E, float cosTheta, float gamma)
{
    float ct = max(cosTheta, 0.02);   // avoid blow-up at / below the horizon
    return (1.0 + A * exp(B / ct)) *
           (1.0 + C * exp(D * gamma) + E * cos(gamma) * cos(gamma));
}

float evalChannel(float p[5], float zenithVal, float cosTheta, float gamma, float cosSunTheta)
{
    float num = perezF(p[0], p[1], p[2], p[3], p[4], cosTheta, gamma);
    float den = perezF(p[0], p[1], p[2], p[3], p[4], cosSunTheta, u_sunTheta);
    return zenithVal * num / max(den, 1e-4);
}

// Cheap 2D value noise (hash-based) for the cirrus layer.
float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}
float valueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i + vec2(0.0, 0.0));
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
float fbm(vec2 p)
{
    float v = 0.0;
    float amp = 0.5;
    for (int i = 0; i < 4; ++i)
    {
        v += amp * valueNoise(p);
        p *= 2.0;
        amp *= 0.5;
    }
    return v;
}

void main()
{
    // Reconstruct a world-space ray direction for this pixel.
    vec4 farP = u_invViewProjRot * vec4(v_ndc, 1.0, 1.0);
    vec3 dir = normalize(farP.xyz / farP.w);

    float cosTheta   = dir.z;                       // +Z is up
    float cosSunTh   = cos(u_sunTheta);
    float cosGamma   = clamp(dot(dir, u_sunDir), -1.0, 1.0);
    float gamma      = acos(cosGamma);

    // Perez in xyY.
    float Y = evalChannel(u_perezY, u_zenith.x, cosTheta, gamma, cosSunTh);
    float cx = evalChannel(u_perezx, u_zenith.y, cosTheta, gamma, cosSunTh);
    float cy = evalChannel(u_perezy, u_zenith.z, cosTheta, gamma, cosSunTh);

    Y = max(Y, 0.0);
    cx = clamp(cx, 0.05, 0.75);
    cy = clamp(cy, 0.05, 0.75);

    // xyY -> XYZ -> linear sRGB.
    float X = (cx / cy) * Y;
    float Z = ((1.0 - cx - cy) / cy) * Y;
    vec3 rgb;
    rgb.r =  3.2406 * X - 1.5372 * Y - 0.4986 * Z;
    rgb.g = -0.9689 * X + 1.8758 * Y + 0.0415 * Z;
    rgb.b =  0.0557 * X - 0.2040 * Y + 1.0570 * Z;
    rgb = max(rgb, vec3(0.0));
    rgb *= u_skyBrightness;

    // Fade to a dim ground haze below the horizon (avoids the Perez singularity
    // showing through and gives a soft horizon line).
    float horizon = smoothstep(-0.08, 0.02, dir.z);
    vec3 groundHaze = vec3(0.10, 0.11, 0.13) * u_skyBrightness;
    rgb = mix(groundHaze, rgb, horizon);

    // Analytic sun disc + limb glow (only above the horizon).
    if (u_sunDir.z > -0.15)
    {
        float sunDisc = smoothstep(0.99965, 0.99992, cosGamma);   // ~0.5 deg disc
        float glow    = pow(max(cosGamma, 0.0), 900.0) * 0.6      // tight halo
                      + pow(max(cosGamma, 0.0), 40.0) * 0.10;     // broad glow
        vec3 sunTint = mix(vec3(1.0, 0.55, 0.30), vec3(1.0, 0.97, 0.92),
                           clamp(u_sunDir.z * 2.0, 0.0, 1.0));
        rgb += sunTint * (sunDisc * 45.0 + glow) * horizon * max(u_skyBrightness, 0.05);
    }

    // Cheap cirrus: scrolling fbm in the upper hemisphere, fading out near the
    // horizon and vanishing when cloudCover is 0.
    if (u_cloudCover > 0.001 && dir.z > 0.02)
    {
        // Project the ray onto a virtual cloud plane above the viewer.
        vec2 uv = dir.xy / max(dir.z, 0.05);
        uv = uv * 0.5 + vec2(0.02, 0.015) * u_time;
        float n = fbm(uv * 1.5);
        float coverage = smoothstep(1.0 - u_cloudCover, 1.0, n);
        float fade = smoothstep(0.02, 0.35, dir.z);
        // Cirrus catches the sun colour a little; mostly bright white.
        vec3 cloudCol = vec3(0.9, 0.92, 0.95) * (0.6 + 0.6 * u_skyBrightness);
        rgb = mix(rgb, cloudCol, coverage * fade * 0.85);
    }

    FRAGCOLOR = vec4(rgb, 1.0);
}
