// Depth-only shadow caster fragment shader. Colour writes are disabled (the
// cascade FBO has GL_NONE draw buffer), so this only needs to be a valid, cheap
// fragment stage - depth is written automatically.
void main()
{
    FRAGCOLOR = vec4(1.0);
}
