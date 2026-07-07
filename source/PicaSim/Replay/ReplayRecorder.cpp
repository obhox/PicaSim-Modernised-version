#include "ReplayRecorder.h"

#include "Aeroplane.h"
#include "Controller.h"
#include "Platform.h"

#include <ctime>
#include <cstdio>

//======================================================================================================================
ReplayRecorder::ReplayRecorder()
    : mRecording(false)
    , mRecordTime(0.0f)
    , mLastSnapshotTime(-1.0e9f)
    , mSnapshotInterval(0.1f)
    , mRingSeconds(0.0f)
    , mMaxSnapshots(0)
{
}

//======================================================================================================================
void ReplayRecorder::StartRecording(const std::string& aeroName,
                                    const std::string& envName,
                                    uint32 settingsDigest,
                                    float snapshotRateHz,
                                    float ringSeconds)
{
    if (snapshotRateHz <= 0.0f)
        snapshotRateHz = 10.0f;

    mData.Clear();
    mData.SetHeaderInfo(aeroName, envName, settingsDigest, snapshotRateHz);

    mSnapshotInterval = 1.0f / snapshotRateHz;
    mRingSeconds = ringSeconds;
    mMaxSnapshots = (ringSeconds > 0.0f)
        ? (size_t)(ringSeconds * snapshotRateHz) + 2
        : 0;

    mRecordTime = 0.0f;
    mLastSnapshotTime = -1.0e9f;
    mRecording = true;
}

//======================================================================================================================
void ReplayRecorder::RecordFrame(float simDeltaTime, const Aeroplane& aeroplane)
{
    if (!mRecording || simDeltaTime <= 0.0f)
        return;

    mRecordTime += simDeltaTime;

    // Only store a snapshot at the target rate.
    if (mRecordTime - mLastSnapshotTime < mSnapshotInterval)
        return;
    mLastSnapshotTime = mRecordTime;

    ReplaySnapshot s;
    s.mTime = mRecordTime;
    const Transform& tm = aeroplane.GetTransform();
    s.mPosition = tm.GetTrans();
    s.mOrientation = Quat(tm);
    s.mLinearVelocity = aeroplane.GetVelocity();
    s.mAngularVelocity = aeroplane.GetAngularVelocity();

    const Controller& c = aeroplane.GetController();
    s.mControls[0] = c.GetControl(Controller::CHANNEL_AILERONS);
    s.mControls[1] = c.GetControl(Controller::CHANNEL_ELEVATOR);
    s.mControls[2] = c.GetControl(Controller::CHANNEL_RUDDER);
    s.mControls[3] = c.GetControl(Controller::CHANNEL_THROTTLE);

    mData.AddSnapshot(s);

    // Bound the ring buffer by dropping the oldest snapshots.
    if (mMaxSnapshots > 0)
    {
        std::vector<ReplaySnapshot>& v = mData.GetSnapshots();
        if (v.size() > mMaxSnapshots)
            v.erase(v.begin(), v.begin() + (v.size() - mMaxSnapshots));
    }
}

//======================================================================================================================
bool ReplayRecorder::Save(const std::string& path) const
{
    if (mData.IsEmpty())
        return false;

    // Save a copy rebased so the first snapshot is at t=0.
    ReplayData copy = mData;
    copy.RebaseTimeToZero();
    return copy.Save(path);
}

//======================================================================================================================
std::string ReplayRecorder::GetReplayDir()
{
    std::string dir = Platform::GetUserDataPath() + "Replays/";
    FileSystem::MakeDirectory(dir);
    return dir;
}

//======================================================================================================================
std::string ReplayRecorder::SaveTimestamped(const char* prefix) const
{
    if (mData.IsEmpty())
        return std::string();

    std::time_t now = std::time(0);
    std::tm* lt = std::localtime(&now);
    char stamp[32];
    if (lt)
        std::snprintf(stamp, sizeof(stamp), "%04d%02d%02d-%02d%02d%02d",
            lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
    else
        std::snprintf(stamp, sizeof(stamp), "%llu", (unsigned long long)now);

    std::string path = GetReplayDir() + prefix + "-" + stamp + ".psrp";
    if (!Save(path))
        return std::string();
    return path;
}
