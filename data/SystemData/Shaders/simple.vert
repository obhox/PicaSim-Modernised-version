uniform mat4 u_mvpMatrix;
ATTRIBUTE vec4 a_position;
ATTRIBUTE vec4 a_colour;
VARYING vec4 v_colour;
void main()
{
    gl_Position = u_mvpMatrix * a_position;
    v_colour = a_colour;
}
