VARYING vec2 v_texCoord;
uniform sampler2D u_texture;
VARYING vec4 v_colour;
void main()
{
    FRAGCOLOR = TEXTURE2D(u_texture, v_texCoord);
    FRAGCOLOR *= v_colour;
}
