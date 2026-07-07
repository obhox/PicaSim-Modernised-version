// CSM terrain shadow-receiver (decal) vertex shader. The heightfield vertices
// are already in world space (terrain modelview = camera view only), so the
// world position needed for the shadow lookup is just a_position.
uniform mat4 u_mvpMatrix;
ATTRIBUTE vec4 a_position;
VARYING vec3 v_worldPos;
void main()
{
    gl_Position = u_mvpMatrix * a_position;
    v_worldPos  = a_position.xyz;
}
