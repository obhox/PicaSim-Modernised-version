VARYING vec2 v_texCoord;
uniform sampler2D u_texture;
void main()
{
    FRAGCOLOR = TEXTURE2D(u_texture, v_texCoord);
}
