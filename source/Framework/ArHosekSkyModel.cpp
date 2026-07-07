// Compact analytic sky model - see ArHosekSkyModel.h for the (important) note on
// what this is and is not. Preetham/Perez daylight coefficients, computed on the
// CPU and consumed by the sky_hosek fragment shader.

#include "ArHosekSkyModel.h"

#include <cmath>

//======================================================================================================================
// Preetham zenith chromaticity polynomials (the classic 3x4 matrices).
static float ZenithChroma(const float M[3][4], float T, float theta)
{
    const float T2 = T * T;
    const float th = theta;
    const float th2 = th * th;
    const float th3 = th2 * th;

    // [T2 T 1] * M * [th3 th2 th 1]^T
    const float c0 = M[0][0] * th3 + M[0][1] * th2 + M[0][2] * th + M[0][3];
    const float c1 = M[1][0] * th3 + M[1][1] * th2 + M[1][2] * th + M[1][3];
    const float c2 = M[2][0] * th3 + M[2][1] * th2 + M[2][2] * th + M[2][3];
    return T2 * c0 + T * c1 + c2;
}

//======================================================================================================================
void SkyModel_Init(SkyModelState& s, float turbidity, float sunTheta)
{
    const float PIf = 3.14159265358979323846f;
    const float T = turbidity;
    s.turbidity = turbidity;
    s.sunTheta = sunTheta;

    // --- Perez distribution coefficients (Preetham et al.), linear in turbidity.
    // Luminance Y:
    s.perez[0][0] =  0.1787f * T - 1.4630f;   // A
    s.perez[0][1] = -0.3554f * T + 0.4275f;   // B
    s.perez[0][2] = -0.0227f * T + 5.3251f;   // C
    s.perez[0][3] =  0.1206f * T - 2.5771f;   // D
    s.perez[0][4] = -0.0670f * T + 0.3703f;   // E
    // Chromaticity x:
    s.perez[1][0] = -0.0193f * T - 0.2592f;
    s.perez[1][1] = -0.0665f * T + 0.0008f;
    s.perez[1][2] = -0.0004f * T + 0.2125f;
    s.perez[1][3] = -0.0641f * T - 0.8989f;
    s.perez[1][4] = -0.0033f * T + 0.0452f;
    // Chromaticity y:
    s.perez[2][0] = -0.0167f * T - 0.2608f;
    s.perez[2][1] = -0.0950f * T + 0.0092f;
    s.perez[2][2] = -0.0079f * T + 0.2102f;
    s.perez[2][3] = -0.0441f * T - 1.6537f;
    s.perez[2][4] = -0.0109f * T + 0.0529f;

    // --- Zenith absolute values.
    // Zenith luminance Yz (Preetham, in kcd/m^2).
    const float chi = (4.0f / 9.0f - T / 120.0f) * (PIf - 2.0f * sunTheta);
    float Yz = (4.0453f * T - 4.9710f) * tanf(chi) - 0.2155f * T + 2.4192f; // kcd/m^2
    if (Yz < 0.0f)
        Yz = 0.0f;

    // Scale the physical luminance (thousands of cd/m^2) into a comfortable
    // linear-HDR range for the renderer. ~0.6-1.0 at the midday zenith so the
    // sky sits mostly inside [0,1] under the identity tonemap, while still being
    // free to exceed 1 near the sun (bloom/PBR-Neutral tonemap handle that).
    const float kLuminanceScale = 0.075f;
    s.zenith[0] = Yz * kLuminanceScale;

    // Zenith chromaticity xz, yz.
    static const float Mx[3][4] = {
        {  0.00166f, -0.00375f,  0.00209f,  0.0f     },
        { -0.02903f,  0.06377f, -0.03202f,  0.00394f },
        {  0.11693f, -0.21196f,  0.06052f,  0.25886f }
    };
    static const float My[3][4] = {
        {  0.00275f, -0.00610f,  0.00317f,  0.0f     },
        { -0.04214f,  0.08970f, -0.04153f,  0.00516f },
        {  0.15346f, -0.26756f,  0.06670f,  0.26688f }
    };
    s.zenith[1] = ZenithChroma(Mx, T, sunTheta);
    s.zenith[2] = ZenithChroma(My, T, sunTheta);
}
