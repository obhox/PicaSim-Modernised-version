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

    // Cumulus clouds: domain-warped fbm on a virtual cloud plane, cheaply shaded
    // by sampling the density a step toward the sun (lit sun-facing edges, cool
    // self-shadowed bases). Fades out near the horizon; vanishes at cloudCover 0.
    if (u_cloudCover > 0.001 && dir.z > 0.015)
    {
        // Ray -> plane above the viewer.
        vec2 base = dir.xy / max(dir.z, 0.05);
        vec2 uv = base * 0.9 + vec2(0.010, 0.008) * u_time;

        // Domain warp gives billowy, non-streaky shapes.
        vec2 warp = vec2(fbm(uv + vec2(1.7, 9.2)), fbm(uv + vec2(8.3, 2.8)));
        float dens = fbm(uv * 1.3 + warp * 0.6);

        // Coverage: more cloudCover -> lower threshold -> puffier sky.
        float thr = mix(0.60, 0.28, clamp(u_cloudCover, 0.0, 1.0));
        float cloud = smoothstep(thr, thr + 0.24, dens);

        // Cheap directional lighting: density a step toward the (plane-projected)
        // sun. If the sun side is thinner, this bit is a lit edge.
        vec2 sunUV = u_sunDir.xy / max(u_sunDir.z, 0.15);
        vec2 ldir = normalize(sunUV - base + vec2(1e-3, 1e-3));
        float densL = fbm((uv + ldir * 0.16) * 1.3 + warp * 0.6);
        float light = clamp((dens - densL) * 2.6 + 0.52, 0.0, 1.0);

        // Sunlit warm-white top vs cool grey base.
        vec3 lit    = mix(vec3(0.86, 0.89, 0.96), vec3(1.03, 1.00, 0.95),
                          clamp(u_sunDir.z * 2.0, 0.0, 1.0));
        vec3 shadow = vec3(0.44, 0.49, 0.60);
        vec3 cloudCol = mix(shadow, lit, light) * (0.55 + 0.55 * u_skyBrightness);

        float fade = smoothstep(0.015, 0.20, dir.z);
        rgb = mix(rgb, cloudCol, clamp(cloud * fade, 0.0, 1.0) * 0.94);
    }

    FRAGCOLOR = vec4(rgb, 1.0);
}
