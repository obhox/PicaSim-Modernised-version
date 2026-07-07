// Depth-only shadow caster vertex shader. Used by ShadowManager to render the
// dynamic model casters into each cascade of the shadow-map array. The engine's
// matrix stack is loaded so that GL_MODELVIEW = cascade world->light-clip matrix
// and GL_PROJECTION = identity, so u_mvpMatrix already carries the full
// world->light-clip transform once the caster has pushed its world matrix.
uniform mat4 u_mvpMatrix;
ATTRIBUTE vec4 a_position;
void main()
{
    gl_Position = u_mvpMatrix * a_position;
}
