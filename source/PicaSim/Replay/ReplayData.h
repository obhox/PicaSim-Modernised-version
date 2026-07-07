#ifndef REPLAY_DATA_H
#define REPLAY_DATA_H

#include "Framework.h"

#include <string>
#include <vector>

//======================================================================================================================
// A single recorded kinematic snapshot of an aeroplane.
//
// Snapshots are stored at a low rate (~10 Hz). Playback interpolates between
// them (Hermite on position using the linear velocity as the tangent, slerp on
// orientation) rather than re-simulating, so scrubbing/seeking is trivial and
// deterministic across runs (Bullet float determinism is not reliable).
//======================================================================================================================
struct ReplaySnapshot
{
    ReplaySnapshot()
        : mTime(0.0f), mPosition(0, 0, 0), mOrientation()
        , mLinearVelocity(0, 0, 0), mAngularVelocity(0, 0, 0)
    {
        mControls[0] = mControls[1] = mControls[2] = mControls[3] = 0.0f;
    }

    float   mTime;             // Seconds since the start of the recording
    Vector3 mPosition;         // World-space object position
    Quat    mOrientation;      // World-space orientation
    Vector3 mLinearVelocity;   // World-space linear velocity (also the Hermite tangent)
    Vector3 mAngularVelocity;  // World-space angular velocity
    // Visual control state: aileron, elevator, rudder, throttle (each -1..1).
    float   mControls[4];
};

//======================================================================================================================
// An interpolated pose+velocity produced by ReplayData::GetInterpolatedState().
//======================================================================================================================
struct ReplayState
{
    ReplayState() : mPosition(0, 0, 0), mOrientation(), mLinearVelocity(0, 0, 0), mAngularVelocity(0, 0, 0)
    {
        mControls[0] = mControls[1] = mControls[2] = mControls[3] = 0.0f;
    }

    Transform GetTransform() const
    {
        Transform tm(mOrientation.GetNormalised());
        tm.SetTrans(mPosition);
        return tm;
    }

    Vector3 mPosition;
    Quat    mOrientation;
    Vector3 mLinearVelocity;
    Vector3 mAngularVelocity;
    float   mControls[4];
};

//======================================================================================================================
// ReplayData: header + time-indexed snapshot frames, plus (de)serialization to a
// chunked binary ".psrp" file and an interpolator.
//
// File format (little-endian, fixed layout so a truncated file is still usable):
//   Header (fixed 152 bytes):
//     char   magic[4]        = "PSRP"
//     uint32 version         = REPLAY_VERSION
//     uint32 snapshotCount
//     float  durationSeconds
//     float  snapshotRateHz
//     uint32 settingsDigest
//     char   aeroName[64]    (null-padded)
//     char   envName[64]     (null-padded)
//   Frames (snapshotCount x fixed 72 bytes each), in time order:
//     float time
//     float px,py,pz
//     float qx,qy,qz,qw
//     float vx,vy,vz
//     float avx,avy,avz
//     float ctrl[4]
//
// Because frames are fixed size and appended after a fixed header, a load can
// recompute the real frame count from the file size and use the smaller of that
// and the header count. A crash mid-write therefore yields a loadable file
// (truncated to whole frames).
//======================================================================================================================
class ReplayData
{
public:
    static const uint32 REPLAY_MAGIC   = 0x50525350u; // 'P','S','R','P' little-endian
    static const uint32 REPLAY_VERSION = 1;
    static const int    HEADER_SIZE    = 4 + 4 + 4 + 4 + 4 + 4 + 64 + 64; // 152
    static const int    FRAME_SIZE     = 18 * 4;                          // 72
    static const int    NAME_LEN       = 64;

    ReplayData() : mDurationSeconds(0.0f), mSnapshotRateHz(10.0f), mSettingsDigest(0) {}

    void Clear() { mSnapshots.clear(); mDurationSeconds = 0.0f; }

    // ---- Header accessors ----
    void SetHeaderInfo(const std::string& aeroName, const std::string& envName, uint32 digest, float snapshotRateHz)
    {
        mAeroName = aeroName;
        mEnvName = envName;
        mSettingsDigest = digest;
        mSnapshotRateHz = snapshotRateHz;
    }
    const std::string& GetAeroName() const { return mAeroName; }
    const std::string& GetEnvName() const { return mEnvName; }
    uint32 GetSettingsDigest() const { return mSettingsDigest; }
    float GetSnapshotRateHz() const { return mSnapshotRateHz; }
    float GetDuration() const { return mDurationSeconds; }

    // ---- Snapshot access ----
    void AddSnapshot(const ReplaySnapshot& s)
    {
        mSnapshots.push_back(s);
        if (s.mTime > mDurationSeconds)
            mDurationSeconds = s.mTime;
    }
    size_t GetNumSnapshots() const { return mSnapshots.size(); }
    const ReplaySnapshot& GetSnapshot(size_t i) const { return mSnapshots[i]; }
    std::vector<ReplaySnapshot>& GetSnapshots() { return mSnapshots; }
    const std::vector<ReplaySnapshot>& GetSnapshots() const { return mSnapshots; }
    bool IsEmpty() const { return mSnapshots.empty(); }

    // Rebase all snapshot times so the first snapshot is at t=0 and recompute duration.
    void RebaseTimeToZero();

    // ---- Interpolation ----
    // Returns the interpolated state at the given time (clamped to [0, duration]).
    // Hermite on position (linear velocity as tangent), slerp on orientation,
    // linear on velocities/controls. Returns false only if there are no snapshots.
    bool GetInterpolatedState(double time, ReplayState& outState) const;

    // ---- Serialization ----
    bool Save(const std::string& path) const;
    bool Load(const std::string& path);

private:
    std::string mAeroName;
    std::string mEnvName;
    float  mDurationSeconds;
    float  mSnapshotRateHz;
    uint32 mSettingsDigest;

    std::vector<ReplaySnapshot> mSnapshots;
};

#endif // REPLAY_DATA_H
