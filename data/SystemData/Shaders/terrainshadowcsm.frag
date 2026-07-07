// CSM terrain shadow-receiver (decal) fragment shader.
//
// This is drawn as a blended pass over the terrain heightfield when CSM is
// enabled. Only DYNAMIC MODEL casters are in the shadow map (the terrain itself
// never casts), so a terrain fragment is only darkened where an aircraft / prop
// occludes the sun above it. That is why the baked panorama terrain self-shadows
// are NOT double-darkened: the terrain is not a caster, so it cannot shadow
// itself here.
#include "common/csm.glsl"

uniform float u_shadowStrength;   // matches the blob-shadow strength
VARYING vec3  v_worldPos;

void main()
{
    float vis    = csmVisibility(v_worldPos);
    float darken = (1.0 - vis) * u_shadowStrength;
    FRAGCOLOR = vec4(0.0, 0.0, 0.0, darken);
}
