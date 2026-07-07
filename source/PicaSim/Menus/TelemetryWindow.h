#ifndef TELEMETRY_WINDOW_H
#define TELEMETRY_WINDOW_H

#include "Helpers.h"

#include <vector>
#include <string>

class Aeroplane;

//======================================================================================================================
// TelemetryWindow - a self-contained, additive in-flight Dear ImGui window that
// displays live flight telemetry for the player aeroplane.
//
// It only READS existing flight state (via the same accessors the HUD/physics
// use) and never modifies the simulation. It keeps a rolling in-memory history
// for the sparkline plots and can export that history to a CSV file under
// <UserData>/Telemetry/.
//
// Draw() must be called from inside an already-begun ImGui frame (see
// PicaSim::DrawTelemetryOverlay). Nothing is created/drawn unless the caller
// chooses to, so the feature is completely default-off.
//======================================================================================================================
class TelemetryWindow
{
public:
    TelemetryWindow();

    // Samples the aeroplane's telemetry for this frame and draws the window.
    // 'dt' is the frame delta time (seconds), used for the G-load finite-difference.
    void Draw(const Aeroplane* aeroplane, float dt);

private:
    // A single sampled telemetry row (also the CSV schema).
    struct Sample
    {
        float time;
        float posX, posY, posZ;
        float altASL, altAGL;
        float airspeed, groundspeed;
        float vario;
        float gLoad;
        float windSpeed, windDir;
        float thermal;
        float heading, pitch, roll;
    };

    void   TakeSample(const Aeroplane* aeroplane, float dt, Sample& out);
    std::string ExportCsv();

    static const size_t HISTORY_MAX = 1024; // ~17s at 60fps

    std::vector<Sample> mHistory;    // full rows (for CSV)
    std::vector<float>  mVarioPlot;  // parallel tails for ImGui::PlotLines
    std::vector<float>  mAirspeedPlot;
    std::vector<float>  mAltitudePlot;

    float   mTotalTime;

    // G-load is derived by finite-differencing the world velocity.
    Vector3 mPrevVel;
    bool    mHavePrevVel;
    float   mSmoothedGLoad;

    // Transient "exported to <path>" status message.
    std::string mLastExportPath;
    float       mExportMessageTimer;
};

#endif // TELEMETRY_WINDOW_H
