// Compact FXAA (luma-based, in the spirit of FXAA 3.11 console/quality). Runs
// on the tonemapped LDR image. u_texelSize is 1/textureResolution.
VARYING vec2 v_texCoord;
uniform sampler2D u_texture;
uniform vec2 u_texelSize;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main()
{
    vec2 uv = v_texCoord;
    vec2 t = u_texelSize;

    vec3 rgbM  = TEXTURE2D(u_texture, uv).rgb;
    vec3 rgbNW = TEXTURE2D(u_texture, uv + vec2(-1.0, -1.0) * t).rgb;
    vec3 rgbNE = TEXTURE2D(u_texture, uv + vec2( 1.0, -1.0) * t).rgb;
    vec3 rgbSW = TEXTURE2D(u_texture, uv + vec2(-1.0,  1.0) * t).rgb;
    vec3 rgbSE = TEXTURE2D(u_texture, uv + vec2( 1.0,  1.0) * t).rgb;

    float lM  = luma(rgbM);
    float lNW = luma(rgbNW);
    float lNE = luma(rgbNE);
    float lSW = luma(rgbSW);
    float lSE = luma(rgbSE);

    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));

    const float EDGE_MIN = 1.0 / 24.0;
    const float EDGE_MUL = 1.0 / 8.0;
    const float SPAN_MAX = 8.0;

    if ((lMax - lMin) < max(EDGE_MIN, lMax * EDGE_MUL))
    {
        FRAGCOLOR = vec4(rgbM, 1.0);
        return;
    }

    vec2 dir;
    dir.x = -((lNW + lNE) - (lSW + lSE));
    dir.y =  ((lNW + lSW) - (lNE + lSE));

    float dirReduce = max((lNW + lNE + lSW + lSE) * 0.25 * EDGE_MUL, 1.0 / 128.0);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-SPAN_MAX), vec2(SPAN_MAX)) * t;

    vec3 rgbA = 0.5 * (TEXTURE2D(u_texture, uv + dir * (1.0 / 3.0 - 0.5)).rgb +
                       TEXTURE2D(u_texture, uv + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (TEXTURE2D(u_texture, uv + dir * -0.5).rgb +
                                     TEXTURE2D(u_texture, uv + dir *  0.5).rgb);

    float lB = luma(rgbB);
    FRAGCOLOR = vec4((lB < lMin || lB > lMax) ? rgbA : rgbB, 1.0);
}
