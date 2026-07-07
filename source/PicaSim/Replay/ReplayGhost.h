#ifndef REPLAY_GHOST_H
#define REPLAY_GHOST_H

#include "ReplayData.h"
#include "Controller.h"

#include <string>

class Aeroplane;
struct AeroplaneSettings;
class LoadingScreenHelper;

//======================================================================================================================
// A neutral controller for the ghost. Returns -1 on every channel so that:
//   * throttle is off (no engine visuals / prop blur),
//   * smoke sources map to zero alpha/rate (no particle side effects),
//   * control-surface deflection is irrelevant (the ghost has no physics wings,
//     so surfaces don't move anyway).
//======================================================================================================================
class GhostController : public Controller
{
public:
    float GetControl(Channel /*channel*/) const OVERRIDE { return -1.0f; }
};

//======================================================================================================================
// ReplayGhost owns a graphics-only Aeroplane and drives its transform each frame
// from an interpolated ReplayData at the current playback time. Rendered translucent.
//======================================================================================================================
class ReplayGhost
{
public:
    ReplayGhost();
    ~ReplayGhost();

    // Loads a .psrp file and creates the ghost using planeSettings for the model.
    // (The stored aero name is informational; the caller supplies the plane to draw.)
    bool Load(const std::string& path,
              const AeroplaneSettings& planeSettings,
              LoadingScreenHelper* loadingScreen = 0,
              float ghostAlpha = 0.45f);

    // Creates the ghost from already-loaded ReplayData.
    bool Init(const ReplayData& data,
              const AeroplaneSettings& planeSettings,
              LoadingScreenHelper* loadingScreen = 0,
              float ghostAlpha = 0.45f);

    void Terminate();

    // Advances playback time by dt and drives the ghost transform. Loops by default.
    void Update(float dt);

    // Directly seek/scrub.
    void SetTime(double t) { mPlaybackTime = t; ApplyCurrentState(); }
    double GetTime() const { return mPlaybackTime; }
    double GetDuration() const { return mData.GetDuration(); }

    void SetLooping(bool loop) { mLoop = loop; }
    bool IsValid() const { return mGhost != 0; }

    Aeroplane* GetAeroplane() { return mGhost; }
    const ReplayData& GetData() const { return mData; }

private:
    void ApplyCurrentState();

    ReplayData      mData;
    GhostController mController;
    Aeroplane*      mGhost;
    double          mPlaybackTime;
    bool            mLoop;
};

#endif // REPLAY_GHOST_H
