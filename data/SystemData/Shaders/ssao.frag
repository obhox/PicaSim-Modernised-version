// Screen-space ambient occlusion. Reconstructs a view-space position for every
// pixel from the scene depth texture and a handful of camera scalars (no matrix
// plumbing needed), estimates a geometric normal from screen-space derivatives,
// and accumulates hemisphere occlusion. Output is a single-channel AO factor
// (1 = unoccluded) written to all channels; a separate blur pass smooths it.

VARYING vec2 v_texCoord;

uniform sampler2D u_depthTex;
uniform vec2  u_texelSize;     // 1/width, 1/height of the AO/depth target
uniform float u_near;
uniform float u_far;
uniform float u_tanHalfFovY;   // tan(verticalFOV/2)
uniform float u_aspect;        // width/height
uniform float u_radius;        // occlusion radius (world/view metres)
uniform float u_intensity;     // 0..~2 darkening strength

// Fixed hemisphere kernel (z > 0), lengths biased toward the origin.
const int KN = 12;
const vec3 KERNEL[12] = vec3[12](
    vec3( 0.20, 0.10, 0.30), vec3(-0.25, 0.18, 0.20), vec3( 0.05,-0.30, 0.35),
    vec3(-0.15,-0.20, 0.55), vec3( 0.40, 0.05, 0.45), vec3(-0.35,-0.10, 0.60),
    vec3( 0.10, 0.45, 0.50), vec3( 0.30,-0.35, 0.65), vec3(-0.45, 0.30, 0.55),
    vec3( 0.55, 0.20, 0.70), vec3(-0.20,-0.55, 0.75), vec3( 0.00, 0.15, 0.90)
);

float hash12(vec2 p)
{
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}

float linearDist(float d)
{
    float ndc = d * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - ndc * (u_far - u_near));
}

vec3 viewPos(vec2 uv)
{
    float dist = linearDist(TEXTURE2D(u_depthTex, uv).r);
    float x = (uv.x * 2.0 - 1.0) * u_tanHalfFovY * u_aspect * dist;
    float y = (uv.y * 2.0 - 1.0) * u_tanHalfFovY * dist;
    return vec3(x, y, -dist);
}

vec2 projectUV(vec3 P)
{
    float invz = 1.0 / max(-P.z, 1e-4);
    vec2 ndc = vec2(P.x / (u_tanHalfFovY * u_aspect) * invz,
                    P.y /  u_tanHalfFovY             * invz);
    return ndc * 0.5 + 0.5;
}

void main()
{
    vec2 uv = v_texCoord;
    float depth = TEXTURE2D(u_depthTex, uv).r;
    if (depth >= 0.9999)          // sky / far plane - no occlusion
    {
        FRAGCOLOR = vec4(1.0);
        return;
    }

    vec3 P = viewPos(uv);
    vec3 N = normalize(cross(dFdx(P), dFdy(P)));
    if (dot(N, -P) < 0.0) N = -N; // face the camera

    // Random in-plane rotation to break up banding, then build a TBN.
    float rnd = hash12(gl_FragCoord.xy) * 6.2831853;
    vec3 randVec = vec3(cos(rnd), sin(rnd), 0.0);
    vec3 T = normalize(randVec - N * dot(randVec, N));
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);

    float bias = 0.02 * u_radius + 0.02;
    float occ = 0.0;
    for (int i = 0; i < KN; ++i)
    {
        vec3 samplePos = P + (TBN * KERNEL[i]) * u_radius;
        vec2 sUV = projectUV(samplePos);
        if (sUV.x < 0.0 || sUV.x > 1.0 || sUV.y < 0.0 || sUV.y > 1.0)
            continue;
        float sceneZ = viewPos(sUV).z;                 // scene surface at sample uv (view z, negative)
        // Tight range check so a much-closer foreground surface doesn't halo the
        // background (classic SSAO artifact at depth discontinuities).
        float rangeCheck = smoothstep(0.0, 1.0, (u_radius * 0.5) / max(abs(P.z - sceneZ), 1e-3));
        occ += (sceneZ >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }

    float ao = 1.0 - (occ / float(KN)) * u_intensity;
    FRAGCOLOR = vec4(clamp(ao, 0.0, 1.0));
}
