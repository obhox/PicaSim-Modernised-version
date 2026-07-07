// Terrain splatting vertex shader (opt-in via <TerrainLayers>).
//
// The heightfield vertices are already in world space (terrain modelview =
// camera view only), so a_position.xyz IS the world position, with world +Z up
// and a_position.z the terrain height. We forward the world position so the
// fragment shader can derive the height, the geometric slope (via screen-space
// derivatives) and the triplanar sample coordinates. u_textureMatrix0 maps the
// world x/y onto the pre-lit basic/lightmap texture, exactly as terrain.vert.
uniform mat4 u_mvpMatrix;
uniform mat4 u_textureMatrix0;
ATTRIBUTE vec4 a_position;
VARYING vec2 v_lightCoord;
VARYING vec3 v_worldPos;
void main()
{
    gl_Position  = u_mvpMatrix * a_position;
    v_lightCoord = (u_textureMatrix0 * a_position).xy;
    v_worldPos   = a_position.xyz;
}
