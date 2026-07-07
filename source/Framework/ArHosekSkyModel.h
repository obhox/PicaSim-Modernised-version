#ifndef AR_HOSEK_SKY_MODEL_H
#define AR_HOSEK_SKY_MODEL_H

// ---------------------------------------------------------------------------
// Compact analytic sky-radiance model, exposed through a Hosek-Wilkie-style
// "state + evaluate" API.
//
// IMPORTANT / HONESTY NOTE:
//   This is an ORIGINAL, self-contained analytic implementation based on the
//   Preetham et al. daylight model (a Perez luminance/chromaticity distribution
//   driven by turbidity and solar elevation). It is *not* the tabulated
//   Hosek & Wilkie radiance dataset from Wenzel Jakob's reference distribution:
//   that dataset is ~2 MB of coefficients and cannot be bundled here offline.
//
//   The public interface deliberately mirrors the shape of the Hosek "cooked
//   state" (per-channel distribution coefficients + a radiance/zenith term) so
//   that the real ArHosekSkyModelData_*.h tables can be dropped in later by
//   replacing the body of SkyModel_Init(); the renderer and the sky_hosek
//   fragment shader only ever touch the sky through SkyModelState, so nothing
//   downstream needs to change.
//
//   The model is evaluated in CIE xyY (channel 0 = luminance Y, 1 = chroma x,
//   2 = chroma y) which the shader converts to linear RGB. This gives the
//   characteristic deep-blue zenith / bright warm horizon gradient and a
//   sun-adjacent glow, all as physically-plausible HDR values.
// ---------------------------------------------------------------------------

struct SkyModelState
{
    // Perez distribution coefficients A..E for each of the three CIE channels:
    //   perez[0] = luminance Y, perez[1] = chroma x, perez[2] = chroma y.
    float perez[3][5];

    // Absolute value at the zenith for each channel: zenith[0] = Yz (a linear
    // HDR luminance, pre-scaled to a comfortable display range), zenith[1] = xz,
    // zenith[2] = yz.
    float zenith[3];

    float sunTheta;    // solar zenith angle, radians (0 = sun overhead)
    float turbidity;   // atmospheric turbidity used to build the coefficients
};

// Rebuilds the sky state.
//   turbidity : 1 (perfectly clear) .. ~10 (very hazy). 2-4 is a clear day.
//   sunTheta  : solar zenith angle in radians (0 = overhead, PI/2 = horizon).
void SkyModel_Init(SkyModelState& state, float turbidity, float sunTheta);

#endif
