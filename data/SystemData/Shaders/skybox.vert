uniform mat4 u_mvpMatrix;
ATTRIBUTE vec4 a_position;
ATTRIBUTE vec2 a_texCoord;
VARYING vec2 v_texCoord;
void main()
{
    gl_Position = u_mvpMatrix * a_position;
    v_texCoord = a_texCoord;
}
