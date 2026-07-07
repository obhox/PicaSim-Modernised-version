VARYING vec2 v_texCoord;
uniform sampler2D u_texture;
uniform vec4 u_colour;
void main()
{
    FRAGCOLOR = u_colour * TEXTURE2D(u_texture, v_texCoord);
}
