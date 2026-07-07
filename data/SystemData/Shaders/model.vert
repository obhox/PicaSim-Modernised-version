uniform mat4 u_mvpMatrix;
uniform mat3 u_normalMatrix;
ATTRIBUTE vec4 a_position;
ATTRIBUTE vec3 a_normal;
ATTRIBUTE vec4 a_colour;
VARYING LOWP vec4 v_colour;
VARYING LOWP vec3 v_normal;
void main()
{
    gl_Position = u_mvpMatrix * a_position;
    v_normal    = u_normalMatrix * a_normal;
    v_colour    = a_colour;
}
