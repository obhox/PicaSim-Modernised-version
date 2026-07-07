#include "TelemetryWindow.h"

#include "UIHelpers.h"
#include "PicaStyle.h"

#include "../Aeroplane.h"
#include "../Environment.h"
#include "../Terrain.h"
#include "Platform.h"
#include "Trace.h"

#include "imgui.h"

#include <cstdio>
#include <cmath>
#include <ctime>

//======================================================================================================================
TelemetryWindow::TelemetryWindow()
    : mTotalTime(0.0f)
    , mPrevVel(0, 0, 0)
    , mHavePrevVel(false)
    , mSmoothedGLoad(1.0f)
    , mExportMessageTimer(0.0f)
{
}

//======================================================================================================================
void TelemetryWindow::TakeSample(const Aeroplane* aeroplane, float dt, Sample& out)
{
    // Same accessors the in-flight HUD graphs use (see AeroplaneGraphics.cpp):
    //   airspeed   = |velocity - wind|            (wind sampled at the aircraft)
    //   groundspeed= |velocity|
    //   vario      = velocity.z                    (world-up climb rate)
    //   windSpeed  = |wind|, thermal = wind.z      (updraft component at aircraft)
    const Transform& tm = aeroplane->GetTransform();
    Vector3 pos = tm.GetTrans();
    Vector3 vel = aeroplane->GetVelocity();
    Vector3 wind = Environment::GetInstance().GetWindAtPosition(
        pos, Environment::WIND_TYPE_SMOOTH | Environment::WIND_TYPE_GUSTY);

    out.time        = mTotalTime;
    out.posX        = pos.x;
    out.posY        = pos.y;
    out.posZ        = pos.z;
    out.airspeed    = (vel - wind).GetLength();
    out.groundspeed = vel.GetLength();
    out.vario       = vel.z;
    out.windSpeed   = wind.GetLength();
    out.thermal     = wind.z;

    // Altitude ASL is the world-space height (z-up); AGL subtracts the terrain
    // height directly below the aircraft (same call the camera near-clip uses).
    out.altASL = pos.z;
    float terrainZ = Environment::GetInstance().GetTerrain().GetTerrainHeight(pos.x, pos.y, false);
    out.altAGL = pos.z - terrainZ;

    // Wind direction it blows FROM, as a compass bearing (0 = +Y, clockwise).
    float windDir = RadiansToDegrees(atan2f(-wind.x, -wind.y));
    if (windDir < 0.0f) windDir += 360.0f;
    out.windDir = windDir;

    // Attitude from the body axes. Body-forward is RowX(), body-up is RowZ(),
    // body-left is RowY() (matches the AI controllers' use of RowX() as forward).
    Vector3 fwd  = tm.RowX();
    Vector3 left = tm.RowY();
    Vector3 up   = tm.RowZ();
    float heading = RadiansToDegrees(atan2f(fwd.x, fwd.y));
    if (heading < 0.0f) heading += 360.0f;
    out.heading = heading;
    out.pitch   = RadiansToDegrees(asinf(ClampToRange(fwd.z, -1.0f, 1.0f)));
    out.roll    = RadiansToDegrees(atan2f(left.z, up.z));

    // G-load: finite-difference the world velocity to get acceleration, then the
    // specific force the airframe feels is (accel - gravity). At rest this gives
    // exactly 1g. Only update when we have a valid previous velocity and dt.
    const float g = 9.81f;
    float gLoad = mSmoothedGLoad;
    if (mHavePrevVel && dt > 1.0e-4f)
    {
        Vector3 accel = (vel - mPrevVel) * (1.0f / dt);
        Vector3 specificForce = accel - Vector3(0.0f, 0.0f, -g);
        gLoad = specificForce.GetLength() / g;
    }
    mPrevVel = vel;
    mHavePrevVel = true;
    // Light smoothing so the readout is legible (physics dt jitter is noisy).
    float lerp = dt > 0.0f ? Minimum(1.0f, dt * 8.0f) : 1.0f;
    mSmoothedGLoad += (gLoad - mSmoothedGLoad) * lerp;
    out.gLoad = mSmoothedGLoad;
}

//======================================================================================================================
std::string TelemetryWindow::ExportCsv()
{
    if (mHistory.empty())
        return std::string();

    std::string dir = Platform::GetUserDataPath() + "Telemetry/";
    FileSystem::MakeDirectory(dir);

    std::time_t now = std::time(0);
    std::tm* lt = std::localtime(&now);
    char stamp[32];
    if (lt)
        std::snprintf(stamp, sizeof(stamp), "%04d%02d%02d-%02d%02d%02d",
            lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
    else
        std::snprintf(stamp, sizeof(stamp), "%llu", (unsigned long long)now);

    std::string path = dir + "telemetry-" + stamp + ".csv";

    FILE* f = fopen(path.c_str(), "wb");
    if (!f)
    {
        TRACE("TelemetryWindow: failed to open %s for writing", path.c_str());
        return std::string();
    }

    fprintf(f, "time,posX,posY,posZ,altASL,altAGL,airspeed,groundspeed,vario,gLoad,"
               "windSpeed,windDir,thermal,heading,pitch,roll\n");
    for (size_t i = 0; i < mHistory.size(); ++i)
    {
        const Sample& s = mHistory[i];
        fprintf(f, "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%.3f,%.2f,%.2f,%.2f\n",
            s.time, s.posX, s.posY, s.posZ, s.altASL, s.altAGL,
            s.airspeed, s.groundspeed, s.vario, s.gLoad,
            s.windSpeed, s.windDir, s.thermal, s.heading, s.pitch, s.roll);
    }
    fclose(f);

    TRACE("TelemetryWindow: exported %zu samples to %s", mHistory.size(), path.c_str());
    return path;
}

//======================================================================================================================
void TelemetryWindow::Draw(const Aeroplane* aeroplane, float dt)
{
    if (!aeroplane)
        return;

    mTotalTime += dt;

    // --- Sample and store history ---
    Sample s;
    TakeSample(aeroplane, dt, s);

    mHistory.push_back(s);
    mVarioPlot.push_back(s.vario);
    mAirspeedPlot.push_back(s.airspeed);
    mAltitudePlot.push_back(s.altASL);
    if (mHistory.size() > HISTORY_MAX)
    {
        mHistory.erase(mHistory.begin());
        mVarioPlot.erase(mVarioPlot.begin());
        mAirspeedPlot.erase(mAirspeedPlot.begin());
        mAltitudePlot.erase(mAltitudePlot.begin());
    }

    // --- Draw the window ---
    float scale = UIHelpers::GetFontScale();

    ImGui::PushFont(UIHelpers::GetFont());
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::SetNextWindowSize(ImVec2(300.0f * scale, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(16.0f * scale, 96.0f * scale), ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, PicaStyle::Common::CornerRadius);

    if (ImGui::Begin("Telemetry", nullptr, ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::Text("Airspeed    %6.1f m/s", s.airspeed);
        ImGui::Text("Groundspeed %6.1f m/s", s.groundspeed);
        ImGui::Separator();
        ImGui::Text("Alt ASL     %6.1f m", s.altASL);
        ImGui::Text("Alt AGL     %6.1f m", s.altAGL);
        ImGui::Text("Vario       %+6.2f m/s", s.vario);
        ImGui::Text("G-load      %6.2f g", s.gLoad);
        ImGui::Separator();
        ImGui::Text("Wind        %5.1f m/s @ %5.1f deg", s.windSpeed, s.windDir);
        ImGui::Text("Thermal     %+6.2f m/s", s.thermal);
        ImGui::Separator();
        ImGui::Text("Heading %5.1f  Pitch %+5.1f  Roll %+5.1f", s.heading, s.pitch, s.roll);

        ImGui::Spacing();

        // Sparkline plots over the rolling history.
        if (!mVarioPlot.empty())
        {
            const ImVec2 plotSize(-1.0f, 40.0f * scale);
            char overlay[32];

            std::snprintf(overlay, sizeof(overlay), "%.2f m/s", s.vario);
            ImGui::PlotLines("##vario", &mVarioPlot[0], (int)mVarioPlot.size(),
                0, overlay, -5.0f, 5.0f, plotSize);
            ImGui::TextDisabled("Vario");

            std::snprintf(overlay, sizeof(overlay), "%.1f m/s", s.airspeed);
            ImGui::PlotLines("##airspeed", &mAirspeedPlot[0], (int)mAirspeedPlot.size(),
                0, overlay, FLT_MAX, FLT_MAX, plotSize);
            ImGui::TextDisabled("Airspeed");

            std::snprintf(overlay, sizeof(overlay), "%.0f m", s.altASL);
            ImGui::PlotLines("##altitude", &mAltitudePlot[0], (int)mAltitudePlot.size(),
                0, overlay, FLT_MAX, FLT_MAX, plotSize);
            ImGui::TextDisabled("Altitude ASL");
        }

        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::Button("Export CSV"))
        {
            std::string path = ExportCsv();
            if (!path.empty())
            {
                mLastExportPath = path;
                mExportMessageTimer = 5.0f;
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(%d samples)", (int)mHistory.size());

        if (mExportMessageTimer > 0.0f)
        {
            mExportMessageTimer -= dt;
            ImGui::TextWrapped("Saved: %s", mLastExportPath.c_str());
        }
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopFont();
}
