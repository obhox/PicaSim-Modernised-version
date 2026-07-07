// Enhanced water/plain vertex shader (opt-in via FrameworkSettings.mEnhancedWater).
// Same transform + texcoord as plain.vert, but also forwards the world position
// so the fragment shader can compute the ripple normal, view vector and fresnel.
uniform mat4 u_mvpMatrix;
uniform mat4 u_textureMatrix;
ATTRIBUTE vec4 a_position;
ATTRIBUTE LOWP vec4 a_colour;
VARYING LOWP vec2 v_texCoord;
VARYING LOWP vec4 v_colour;
VARYING vec3 v_worldPos;
void main()
{
    gl_Position = u_mvpMatrix * a_position;
    v_texCoord  = (u_textureMatrix * a_position).xy;
    v_colour    = a_colour;
    v_worldPos  = a_position.xyz;
}
