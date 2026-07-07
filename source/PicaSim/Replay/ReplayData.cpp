#include "ReplayData.h"

#include "Platform.h"

#include <cstring>
#include <cmath>

//======================================================================================================================
// Small little-endian byte writer/reader helpers. The engine only targets
// little-endian CPUs (x86 / ARM64), so a straight memcpy is fine.
//======================================================================================================================
namespace
{
    void PutU32(std::vector<uint8_t>& b, uint32 v)
    {
        b.push_back((uint8_t)(v & 0xff));
        b.push_back((uint8_t)((v >> 8) & 0xff));
        b.push_back((uint8_t)((v >> 16) & 0xff));
        b.push_back((uint8_t)((v >> 24) & 0xff));
    }
    void PutF32(std::vector<uint8_t>& b, float f)
    {
        uint32 v;
        std::memcpy(&v, &f, 4);
        PutU32(b, v);
    }
    void PutName(std::vector<uint8_t>& b, const std::string& s, int len)
    {
        for (int i = 0; i < len; ++i)
            b.push_back(i < (int)s.size() ? (uint8_t)s[i] : 0);
    }

    uint32 GetU32(const uint8_t* p)
    {
        return (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24);
    }
    float GetF32(const uint8_t* p)
    {
        uint32 v = GetU32(p);
        float f;
        std::memcpy(&f, &v, 4);
        return f;
    }
    std::string GetName(const uint8_t* p, int len)
    {
        int n = 0;
        while (n < len && p[n] != 0)
            ++n;
        return std::string((const char*)p, (size_t)n);
    }
}

//======================================================================================================================
void ReplayData::RebaseTimeToZero()
{
    if (mSnapshots.empty())
        return;
    float t0 = mSnapshots.front().mTime;
    mDurationSeconds = 0.0f;
    for (size_t i = 0; i < mSnapshots.size(); ++i)
    {
        mSnapshots[i].mTime -= t0;
        if (mSnapshots[i].mTime > mDurationSeconds)
            mDurationSeconds = mSnapshots[i].mTime;
    }
}

//======================================================================================================================
bool ReplayData::GetInterpolatedState(double time, ReplayState& outState) const
{
    const size_t n = mSnapshots.size();
    if (n == 0)
        return false;

    if (n == 1 || time <= mSnapshots[0].mTime)
    {
        const ReplaySnapshot& s = mSnapshots[0];
        outState.mPosition = s.mPosition;
        outState.mOrientation = s.mOrientation;
        outState.mLinearVelocity = s.mLinearVelocity;
        outState.mAngularVelocity = s.mAngularVelocity;
        for (int c = 0; c < 4; ++c) outState.mControls[c] = s.mControls[c];
        return true;
    }
    if (time >= mSnapshots[n - 1].mTime)
    {
        const ReplaySnapshot& s = mSnapshots[n - 1];
        outState.mPosition = s.mPosition;
        outState.mOrientation = s.mOrientation;
        outState.mLinearVelocity = s.mLinearVelocity;
        outState.mAngularVelocity = s.mAngularVelocity;
        for (int c = 0; c < 4; ++c) outState.mControls[c] = s.mControls[c];
        return true;
    }

    // Binary search for the segment [i, i+1] containing 'time'.
    size_t lo = 0, hi = n - 1;
    while (hi - lo > 1)
    {
        size_t mid = (lo + hi) / 2;
        if ((double)mSnapshots[mid].mTime <= time)
            lo = mid;
        else
            hi = mid;
    }

    const ReplaySnapshot& a = mSnapshots[lo];
    const ReplaySnapshot& b = mSnapshots[hi];

    float dt = b.mTime - a.mTime;
    float s = (dt > 1e-6f) ? (float)((time - a.mTime) / dt) : 0.0f;
    if (s < 0.0f) s = 0.0f;
    if (s > 1.0f) s = 1.0f;

    // Cubic Hermite basis functions.
    float s2 = s * s;
    float s3 = s2 * s;
    float h00 = 2.0f * s3 - 3.0f * s2 + 1.0f;
    float h10 = s3 - 2.0f * s2 + s;
    float h01 = -2.0f * s3 + 3.0f * s2;
    float h11 = s3 - s2;

    // Tangents are the linear velocities scaled by the segment duration.
    Vector3 m0 = a.mLinearVelocity * dt;
    Vector3 m1 = b.mLinearVelocity * dt;

    outState.mPosition = a.mPosition * h00 + m0 * h10 + b.mPosition * h01 + m1 * h11;

    // Orientation: shortest-arc slerp.
    outState.mOrientation = Quat::Slerp(a.mOrientation, b.mOrientation, s);

    // Velocities and controls: linear.
    outState.mLinearVelocity = a.mLinearVelocity * (1.0f - s) + b.mLinearVelocity * s;
    outState.mAngularVelocity = a.mAngularVelocity * (1.0f - s) + b.mAngularVelocity * s;
    for (int c = 0; c < 4; ++c)
        outState.mControls[c] = a.mControls[c] * (1.0f - s) + b.mControls[c] * s;

    return true;
}

//======================================================================================================================
bool ReplayData::Save(const std::string& path) const
{
    std::vector<uint8_t> buf;
    buf.reserve(HEADER_SIZE + mSnapshots.size() * FRAME_SIZE);

    // Header
    PutU32(buf, REPLAY_MAGIC);
    PutU32(buf, REPLAY_VERSION);
    PutU32(buf, (uint32)mSnapshots.size());
    PutF32(buf, mDurationSeconds);
    PutF32(buf, mSnapshotRateHz);
    PutU32(buf, mSettingsDigest);
    PutName(buf, mAeroName, NAME_LEN);
    PutName(buf, mEnvName, NAME_LEN);

    // Frames
    for (size_t i = 0; i < mSnapshots.size(); ++i)
    {
        const ReplaySnapshot& s = mSnapshots[i];
        PutF32(buf, s.mTime);
        PutF32(buf, s.mPosition.x); PutF32(buf, s.mPosition.y); PutF32(buf, s.mPosition.z);
        PutF32(buf, s.mOrientation.x); PutF32(buf, s.mOrientation.y); PutF32(buf, s.mOrientation.z); PutF32(buf, s.mOrientation.s);
        PutF32(buf, s.mLinearVelocity.x); PutF32(buf, s.mLinearVelocity.y); PutF32(buf, s.mLinearVelocity.z);
        PutF32(buf, s.mAngularVelocity.x); PutF32(buf, s.mAngularVelocity.y); PutF32(buf, s.mAngularVelocity.z);
        PutF32(buf, s.mControls[0]); PutF32(buf, s.mControls[1]); PutF32(buf, s.mControls[2]); PutF32(buf, s.mControls[3]);
    }

    return FileSystem::WriteFile(path, buf.data(), buf.size());
}

//======================================================================================================================
bool ReplayData::Load(const std::string& path)
{
    std::vector<uint8_t> buf;
    if (!FileSystem::ReadFile(path, buf))
        return false;
    if (buf.size() < (size_t)HEADER_SIZE)
        return false;

    const uint8_t* p = buf.data();
    if (GetU32(p + 0) != REPLAY_MAGIC)
        return false;
    uint32 version = GetU32(p + 4);
    if (version != REPLAY_VERSION)
        return false;

    uint32 headerCount = GetU32(p + 8);
    mDurationSeconds    = GetF32(p + 12);
    mSnapshotRateHz     = GetF32(p + 16);
    mSettingsDigest     = GetU32(p + 20);
    mAeroName = GetName(p + 24, NAME_LEN);
    mEnvName  = GetName(p + 24 + NAME_LEN, NAME_LEN);

    // Recover the real frame count from the file size (crash-truncation safe).
    size_t availBytes = buf.size() - HEADER_SIZE;
    uint32 availFrames = (uint32)(availBytes / FRAME_SIZE);
    uint32 count = headerCount < availFrames ? headerCount : availFrames;

    mSnapshots.clear();
    mSnapshots.reserve(count);
    const uint8_t* f = p + HEADER_SIZE;
    float maxTime = 0.0f;
    for (uint32 i = 0; i < count; ++i, f += FRAME_SIZE)
    {
        ReplaySnapshot s;
        s.mTime = GetF32(f + 0);
        s.mPosition = Vector3(GetF32(f + 4), GetF32(f + 8), GetF32(f + 12));
        s.mOrientation = Quat(GetF32(f + 28), GetF32(f + 16), GetF32(f + 20), GetF32(f + 24)); // (s, x, y, z)
        s.mLinearVelocity = Vector3(GetF32(f + 32), GetF32(f + 36), GetF32(f + 40));
        s.mAngularVelocity = Vector3(GetF32(f + 44), GetF32(f + 48), GetF32(f + 52));
        s.mControls[0] = GetF32(f + 56);
        s.mControls[1] = GetF32(f + 60);
        s.mControls[2] = GetF32(f + 64);
        s.mControls[3] = GetF32(f + 68);
        mSnapshots.push_back(s);
        if (s.mTime > maxTime)
            maxTime = s.mTime;
    }
    if (maxTime > mDurationSeconds || mSnapshots.size() != headerCount)
        mDurationSeconds = maxTime;

    return !mSnapshots.empty();
}
