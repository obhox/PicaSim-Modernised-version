uniform mat4 u_mvpMatrix;
uniform mat4 u_textureMatrix;
ATTRIBUTE vec4 a_position;
ATTRIBUTE vec4 a_texCoord;
VARYING LOWP vec2 v_texCoord;
void main()
{
    gl_Position = u_mvpMatrix * a_position;
    v_texCoord = (u_textureMatrix * a_texCoord).xy;
}
