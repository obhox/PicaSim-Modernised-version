VARYING vec2 v_texCoord;
uniform sampler2D u_texture;
uniform LOWP vec4 u_colour;
void main()
{
    FRAGCOLOR = TEXTURE2D(u_texture, v_texCoord);
    FRAGCOLOR *= u_colour;
}
