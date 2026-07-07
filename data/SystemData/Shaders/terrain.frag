VARYING vec2 v_texCoord0;
VARYING vec2 v_texCoord1;
uniform sampler2D u_texture0;
uniform sampler2D u_texture1;
void main()
{
    FRAGCOLOR = TEXTURE2D(u_texture0, v_texCoord0);
    FRAGCOLOR *= TEXTURE2D(u_texture1, v_texCoord1);
}
