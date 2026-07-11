uniform mat4 u_mvpMatrix;
uniform mat3 u_normalMatrix;
uniform mat4 u_worldMatrix;   // model-space -> world-space (for CSM lookup)
ATTRIBUTE vec4 a_position;
ATTRIBUTE vec3 a_normal;
ATTRIBUTE vec4 a_colour;
ATTRIBUTE vec2 a_texCoord;
ATTRIBUTE vec3 a_tangent;
VARYING LOWP vec4 v_colour;
VARYING LOWP vec3 v_normal;
VARYING LOWP vec2 v_texCoord;
VARYING vec3 v_worldPos;
VARYING vec3 v_tangent;
void main()
{
    gl_Position = u_mvpMatrix * a_position;
    v_normal    = u_normalMatrix * a_normal;
    // Tangent transformed into the same (view) space as v_normal. Zero for meshes
    // without a normal map; the fragment shader only uses it when u_useNormalMap>0.
    v_tangent   = u_normalMatrix * a_tangent;
    v_colour    = a_colour;
    v_texCoord  = a_texCoord;
    v_worldPos  = (u_worldMatrix * a_position).xyz;
}
