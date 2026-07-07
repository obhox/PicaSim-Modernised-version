// Cook-Torrance GGX "PBR-lite" used by the aircraft model shaders.
//
// All vectors are expressed in VIEW space, matching the model shaders' view-
// space normals and the view-space light directions (u_lightDir[i]). The view
// direction V is the fixed (0,0,1) - the camera looks down -Z in view space -
// so no camera-position uniform is needed, exactly as the old Phong path.

#ifndef PBR_GLSL
#define PBR_GLSL

const float PBR_PI = 3.14159265359;

// GGX / Trowbridge-Reitz normal distribution.
float D_GGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / max(PBR_PI * d * d, 1e-7);
}

float G_SchlickGGX(float NdotX, float k)
{
    return NdotX / max(NdotX * (1.0 - k) + k, 1e-7);
}

// Smith geometry term (Schlick-GGX), direct-lighting roughness remap.
float G_SmithGGX(float NdotV, float NdotL, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return G_SchlickGGX(NdotV, k) * G_SchlickGGX(NdotL, k);
}

// Fresnel (Schlick).
vec3 F_Schlick(float VdotH, vec3 F0)
{
    float f = pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
    return F0 + (vec3(1.0) - F0) * f;
}

// Outgoing radiance for a single directional light.
// Returns (Lambert diffuse * (1-metallic) + Cook-Torrance specular) * NdotL * lightColour.
vec3 evalDirectPBR(vec3 albedo, float metallic, float roughness, vec3 F0,
                   vec3 N, vec3 V, vec3 L, vec3 lightColour)
{
    float NdotL = dot(N, L);
    if (NdotL <= 0.0)
        return vec3(0.0);
    NdotL = clamp(NdotL, 0.0, 1.0);

    vec3  H     = normalize(L + V);
    float NdotV = clamp(dot(N, V), 1e-4, 1.0);
    float NdotH = clamp(dot(N, H), 0.0, 1.0);
    float VdotH = clamp(dot(V, H), 0.0, 1.0);

    float D = D_GGX(NdotH, roughness);
    float G = G_SmithGGX(NdotV, NdotL, roughness);
    vec3  F = F_Schlick(VdotH, F0);

    vec3 spec = (D * G) * F / max(4.0 * NdotV * NdotL, 1e-4);

    vec3 kd      = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kd * albedo / PBR_PI;

    return (diffuse + spec) * lightColour * NdotL;
}

// Standard 9-coefficient SH irradiance reconstruction (Ramamoorthi & Hanrahan
// 2001), normalised (divided by PI) so that a constant-radiance environment A
// returns A - i.e. the result is ready to be multiplied straight by albedo.
//
// Coefficient ordering:
//   sh[0]=L00
//   sh[1]=L1-1(y) sh[2]=L10(z) sh[3]=L11(x)
//   sh[4]=L2-2(xy) sh[5]=L2-1(yz) sh[6]=L20 sh[7]=L21(xz) sh[8]=L22(x^2-y^2)
vec3 evalSHIrradiance(vec3 sh[9], vec3 N)
{
    const float c1 = 0.429043;
    const float c2 = 0.511664;
    const float c3 = 0.743125;
    const float c4 = 0.886227;
    const float c5 = 0.247708;
    float x = N.x, y = N.y, z = N.z;
    vec3 E =
          c1 * sh[8] * (x * x - y * y)
        + c3 * sh[6] * (z * z)
        + c4 * sh[0]
        - c5 * sh[6]
        + 2.0 * c1 * (sh[4] * x * y + sh[7] * x * z + sh[5] * y * z)
        + 2.0 * c2 * (sh[3] * x + sh[1] * y + sh[2] * z);
    return max(E / PBR_PI, vec3(0.0));
}

#endif // PBR_GLSL
