uniform mat4 u_mvpMatrix;
uniform mat4 u_textureMatrix0;
uniform mat4 u_textureMatrix1;
ATTRIBUTE vec4 a_position;
VARYING LOWP vec2 v_texCoord1;
VARYING LOWP vec2 v_texCoord0;
void main()
{
    gl_Position = u_mvpMatrix * a_position;
    v_texCoord0 = (u_textureMatrix0 * a_position).xy;
    v_texCoord1 = (u_textureMatrix1 * a_position).xy;
}
