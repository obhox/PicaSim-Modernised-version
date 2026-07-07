// Fullscreen triangle generated entirely from gl_VertexID - no vertex buffer or
// attributes required. Three vertices at (-1,-1),(3,-1),(-1,3) cover the whole
// clip rectangle; v_texCoord runs 0..2 so the visible [0,1] region maps to the
// full texture.
VARYING vec2 v_texCoord;
void main()
{
    vec2 pos = vec2( (gl_VertexID == 1) ? 3.0 : -1.0,
                     (gl_VertexID == 2) ? 3.0 : -1.0 );
    v_texCoord = (pos + 1.0) * 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
