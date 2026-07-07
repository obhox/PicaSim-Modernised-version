// Fullscreen triangle for the procedural dynamic sky, generated from
// gl_VertexID (no vertex buffer / attributes). Passes the clip-space position
// through so the fragment shader can reconstruct a world-space ray direction
// via the inverse rotation-only view-projection matrix.
VARYING vec2 v_ndc;
void main()
{
    vec2 pos = vec2( (gl_VertexID == 1) ? 3.0 : -1.0,
                     (gl_VertexID == 2) ? 3.0 : -1.0 );
    v_ndc = pos;
    gl_Position = vec4(pos, 0.0, 1.0);
}
