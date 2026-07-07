uniform mat4 u_mvpMatrix;
uniform mat4 u_textureMatrix;
ATTRIBUTE vec4 a_position;
ATTRIBUTE LOWP vec4 a_colour;
VARYING LOWP vec2 v_texCoord;
VARYING LOWP vec4 v_colour;
void main()
{
    gl_Position = u_mvpMatrix * a_position;
    v_texCoord = (u_textureMatrix * a_position).xy;
    v_colour = a_colour;
}
