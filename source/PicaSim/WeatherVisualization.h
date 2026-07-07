#ifndef WEATHERVISUALIZATION_H
#define WEATHERVISUALIZATION_H

#include "RenderObject.h"
#include "Helpers.h"

#include <vector>

/// Optional training/debug overlay that lets a slope/thermal soaring pilot SEE
/// the otherwise-invisible airflow. It is purely a visualization: it only QUERIES
/// the wind/thermal field (Environment::GetWindAtPosition and
/// ThermalManager::GetThermalVisInfo) and never modifies physics.
///
/// Everything is gated behind the Options flags mShowWindStreamlines /
/// mShowThermals / mShowTurbulence, which all default false. With all three off
/// RenderUpdate() returns immediately and nothing is built or drawn, so there is
/// zero behavioural or performance impact on normal flight.
///
/// All geometry is drawn as alpha/additively-blended coloured lines via
/// SHADER_SIMPLE (position + colour), depth-test on, depth-write off, so it reads
/// as an overlay laid over the world.
class WeatherVisualization : public RenderObject
{
public:
    void Init();
    void Terminate();

    void RenderUpdate(class Viewport* viewport, int renderLevel) OVERRIDE;

private:
    void BuildWindStreamlines(const Vector3& centre);
    void BuildThermals(class Viewport* viewport);
    void BuildTurbulence(const Vector3& centre);

    // Append one coloured line segment (GL_LINES) to the shared buffers.
    void AddSegment(const Vector3& a, const Vector3& b, const Vector4& colA, const Vector4& colB);

    // Append a horizontal ring (as GL_LINES segments) to the shared buffers.
    void AddRing(const Vector3& centre, float radius, const Vector4& col, int divisions);

    // Draws the accumulated GL_LINES segments with the given blend mode and clears them.
    void Flush(bool additive);

    // Reusable per-frame buffers (interleaved as parallel arrays, GL_LINES pairs).
    std::vector<Vector3> mPoints;
    std::vector<Vector4> mColours;
};

#endif
