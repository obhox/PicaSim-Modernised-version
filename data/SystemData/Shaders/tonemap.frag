#include "common/tonemap.glsl"

// Resolves the HDR scene texture to the LDR default framebuffer. Applies an
// exposure multiplier, optionally adds bloom, then the PBR Neutral tonemap.
//
// u_uvOffset / u_uvScale select the sub-rectangle of the HDR texture that maps
// to this viewport (used for stereo; (0,0)/(1,1) for a full-screen resolve).
VARYING vec2 v_texCoord;
uniform sampler2D u_hdrTexture;
uniform sampler2D u_bloomTexture;
uniform vec2  u_uvOffset;
uniform vec2  u_uvScale;
uniform float u_exposure;
uniform float u_bloomIntensity;   // 0.0 => bloom disabled
uniform float u_tonemap;          // 0.0 => identity (preserve LDR look), 1.0 => PBR Neutral
uniform sampler2D u_aoTexture;    // screen-space ambient occlusion (1 = unoccluded)
uniform float u_useAO;            // 0.0 => AO disabled

void main()
{
    vec2 uv = u_uvOffset + v_texCoord * u_uvScale;

    vec3 hdr = TEXTURE2D(u_hdrTexture, uv).rgb;
    // Darken crevices/contact areas with the blurred AO factor. Applied before
    // bloom/exposure so occluded areas also bloom less.
    if (u_useAO > 0.5)
        hdr *= TEXTURE2D(u_aoTexture, uv).r;
    hdr *= u_exposure;

    if (u_bloomIntensity > 0.0)
    {
        vec3 bloom = TEXTURE2D(u_bloomTexture, uv).rgb;
        hdr += bloom * u_bloomIntensity;
    }

    // Default (u_tonemap == 0): identity clamp - the existing content is
    // already display-referred LDR, so this preserves the look exactly. The
    // PBR Neutral operator is opt-in (and becomes the default once linear/HDR
    // lighting lands in a later phase).
    vec3 mapped = (u_tonemap > 0.5) ? PBRNeutralToneMapping(hdr) : clamp(hdr, 0.0, 1.0);
    FRAGCOLOR = vec4(mapped, 1.0);
}
