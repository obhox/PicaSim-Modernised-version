// 4x4 box blur of the raw SSAO texture to hide the sampling noise. Single
// channel in/out (AO factor).
VARYING vec2 v_texCoord;
uniform sampler2D u_aoTex;
uniform vec2 u_texelSize;

void main()
{
    float sum = 0.0;
    for (int y = -2; y < 2; ++y)
        for (int x = -2; x < 2; ++x)
            sum += TEXTURE2D(u_aoTex, v_texCoord + vec2(float(x), float(y)) * u_texelSize).r;
    FRAGCOLOR = vec4(sum / 16.0);
}
