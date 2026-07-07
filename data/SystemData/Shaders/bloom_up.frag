// Progressive bloom upsample. 9-tap tent filter, additively blended (GL_ONE,
// GL_ONE) onto the larger mip. u_texelSize is 1/textureResolution of the SOURCE
// (smaller) texture; u_radius scales the tent spread.
VARYING vec2 v_texCoord;
uniform sampler2D u_texture;
uniform vec2  u_texelSize;
uniform float u_radius;

vec3 sampleTex(vec2 uv) { return TEXTURE2D(u_texture, uv).rgb; }

void main()
{
    vec2 uv = v_texCoord;
    vec2 t = u_texelSize * u_radius;

    vec3 a = sampleTex(uv + vec2(-1.0,  1.0) * t);
    vec3 b = sampleTex(uv + vec2( 0.0,  1.0) * t);
    vec3 c = sampleTex(uv + vec2( 1.0,  1.0) * t);

    vec3 d = sampleTex(uv + vec2(-1.0,  0.0) * t);
    vec3 e = sampleTex(uv + vec2( 0.0,  0.0) * t);
    vec3 f = sampleTex(uv + vec2( 1.0,  0.0) * t);

    vec3 g = sampleTex(uv + vec2(-1.0, -1.0) * t);
    vec3 h = sampleTex(uv + vec2( 0.0, -1.0) * t);
    vec3 i = sampleTex(uv + vec2( 1.0, -1.0) * t);

    // Tent kernel: 1 2 1 / 2 4 2 / 1 2 1, normalised by 1/16.
    vec3 result = e * 4.0;
    result += (b + d + f + h) * 2.0;
    result += (a + c + g + i);
    result *= (1.0 / 16.0);

    FRAGCOLOR = vec4(result, 1.0);
}
