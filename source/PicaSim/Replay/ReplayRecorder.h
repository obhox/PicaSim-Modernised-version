#ifndef REPLAY_RECORDER_H
#define REPLAY_RECORDER_H

#include "ReplayData.h"

#include <string>

class Aeroplane;

//======================================================================================================================
// ReplayRecorder samples an Aeroplane's kinematic state at a fixed low rate
// (~10 Hz) into a bounded in-memory ring buffer. It is designed to be cheap and
// completely side-effect-free: when not recording, Update() does nothing, and
// while recording it only reads already-computed state (no physics work).
//
// Two use modes:
//   * Always-on ring buffer: StartRecording() with ringSeconds>0 keeps only the
//     last N seconds, so a "replay last flight / crash-cam" is always available.
//   * Explicit capture: StartRecording() with ringSeconds<=0 grows unbounded
//     until Stop()/Save().
//======================================================================================================================
class ReplayRecorder
{
public:
    ReplayRecorder();

    // Begin (or restart) recording. ringSeconds<=0 means unbounded.
    void StartRecording(const std::string& aeroName,
                        const std::string& envName,
                        uint32 settingsDigest,
                        float snapshotRateHz = 10.0f,
                        float ringSeconds = 60.0f);

    void Stop() { mRecording = false; }
    bool IsRecording() const { return mRecording; }

    // Called each frame after physics with the simulation dt (0 when paused) and
    // the aeroplane whose state should be sampled. Cheap no-op when not recording
    // or when dt <= 0.
    void RecordFrame(float simDeltaTime, const Aeroplane& aeroplane);

    // Serialize the current buffer to a .psrp file (times rebased to start at 0).
    bool Save(const std::string& path) const;

    bool HasData() const { return !mData.IsEmpty(); }
    const ReplayData& GetData() const { return mData; }

    // Convenience: builds a timestamped path under <UserData>/Replays/ and saves.
    // Returns the written path (empty on failure).
    std::string SaveTimestamped(const char* prefix = "flight") const;

    // Ensures <UserData>/Replays/ exists and returns its path (with trailing sep).
    static std::string GetReplayDir();

private:
    ReplayData mData;
    bool   mRecording;
    float  mRecordTime;       // Accumulated recording time (seconds)
    float  mLastSnapshotTime; // Time of the last stored snapshot
    float  mSnapshotInterval; // 1 / rate
    float  mRingSeconds;      // <=0 means unbounded
    size_t mMaxSnapshots;     // Derived from ring seconds & rate (0 = unbounded)
};

#endif // REPLAY_RECORDER_H
