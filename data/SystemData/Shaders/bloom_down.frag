// Progressive bloom downsample. 13-tap filter (Call of Duty / Jimenez) that
// downsamples with a partial Karis average to suppress fireflies on the first
// mip. u_texelSize is 1/textureResolution of the SOURCE texture.
VARYING vec2 v_texCoord;
uniform sampler2D u_texture;
uniform vec2  u_texelSize;
uniform float u_firstMip;   // 1.0 on the first downsample, else 0.0

vec3 sampleTex(vec2 uv) { return TEXTURE2D(u_texture, uv).rgb; }

float karisWeight(vec3 c) { return 1.0 / (1.0 + max(c.r, max(c.g, c.b))); }

void main()
{
    vec2 uv = v_texCoord;
    vec2 t = u_texelSize;

    vec3 a = sampleTex(uv + vec2(-2.0,  2.0) * t);
    vec3 b = sampleTex(uv + vec2( 0.0,  2.0) * t);
    vec3 c = sampleTex(uv + vec2( 2.0,  2.0) * t);

    vec3 d = sampleTex(uv + vec2(-2.0,  0.0) * t);
    vec3 e = sampleTex(uv + vec2( 0.0,  0.0) * t);
    vec3 f = sampleTex(uv + vec2( 2.0,  0.0) * t);

    vec3 g = sampleTex(uv + vec2(-2.0, -2.0) * t);
    vec3 h = sampleTex(uv + vec2( 0.0, -2.0) * t);
    vec3 i = sampleTex(uv + vec2( 2.0, -2.0) * t);

    vec3 j = sampleTex(uv + vec2(-1.0,  1.0) * t);
    vec3 k = sampleTex(uv + vec2( 1.0,  1.0) * t);
    vec3 l = sampleTex(uv + vec2(-1.0, -1.0) * t);
    vec3 m = sampleTex(uv + vec2( 1.0, -1.0) * t);

    vec3 result;
    if (u_firstMip > 0.5)
    {
        // Weighted (Karis) average of five 2x2 boxes to reduce sparkle.
        vec3 g0 = (j + k + l + m) * 0.25;
        vec3 g1 = (a + b + d + e) * 0.25;
        vec3 g2 = (b + c + e + f) * 0.25;
        vec3 g3 = (d + e + g + h) * 0.25;
        vec3 g4 = (e + f + h + i) * 0.25;
        float w0 = karisWeight(g0);
        float w1 = karisWeight(g1);
        float w2 = karisWeight(g2);
        float w3 = karisWeight(g3);
        float w4 = karisWeight(g4);
        float wsum = w0 + w1 + w2 + w3 + w4;
        result = (g0 * w0 + g1 * w1 + g2 * w2 + g3 * w3 + g4 * w4) / wsum;
    }
    else
    {
        result  = e * 0.125;
        result += (a + c + g + i) * 0.03125;
        result += (b + d + f + h) * 0.0625;
        result += (j + k + l + m) * 0.125;
    }

    FRAGCOLOR = vec4(result, 1.0);
}
