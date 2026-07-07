#include "ReplayGhost.h"

#include "Aeroplane.h"
#include "AeroplaneGraphics.h"
#include "GameSettings.h"

//======================================================================================================================
ReplayGhost::ReplayGhost()
    : mGhost(0)
    , mPlaybackTime(0.0)
    , mLoop(true)
{
}

//======================================================================================================================
ReplayGhost::~ReplayGhost()
{
    Terminate();
}

//======================================================================================================================
bool ReplayGhost::Load(const std::string& path,
                       const AeroplaneSettings& planeSettings,
                       LoadingScreenHelper* loadingScreen,
                       float ghostAlpha)
{
    ReplayData data;
    if (!data.Load(path))
        return false;
    return Init(data, planeSettings, loadingScreen, ghostAlpha);
}

//======================================================================================================================
bool ReplayGhost::Init(const ReplayData& data,
                       const AeroplaneSettings& planeSettings,
                       LoadingScreenHelper* loadingScreen,
                       float ghostAlpha)
{
    Terminate();

    mData = data;
    if (mData.IsEmpty())
        return false;

    mPlaybackTime = 0.0;

    // Position the ghost at the first recorded snapshot so the model loads in place.
    Vector3 startPos = mData.GetSnapshot(0).mPosition;

    mGhost = new Aeroplane(mController);
    mGhost->Init(planeSettings, &startPos, loadingScreen, /*graphicsOnly=*/true);

    if (mGhost->GetGraphics())
        mGhost->GetGraphics()->SetGhostAlpha(ghostAlpha);

    ApplyCurrentState();
    return true;
}

//======================================================================================================================
void ReplayGhost::Terminate()
{
    if (mGhost)
    {
        mGhost->Terminate();
        delete mGhost;
        mGhost = 0;
    }
}

//======================================================================================================================
void ReplayGhost::Update(float dt)
{
    if (!mGhost)
        return;

    mPlaybackTime += dt;

    double duration = mData.GetDuration();
    if (duration > 0.0)
    {
        if (mLoop)
        {
            while (mPlaybackTime > duration)
                mPlaybackTime -= duration;
            while (mPlaybackTime < 0.0)
                mPlaybackTime += duration;
        }
        else
        {
            if (mPlaybackTime > duration) mPlaybackTime = duration;
            if (mPlaybackTime < 0.0)      mPlaybackTime = 0.0;
        }
    }

    ApplyCurrentState();
}

//======================================================================================================================
void ReplayGhost::ApplyCurrentState()
{
    if (!mGhost)
        return;

    ReplayState state;
    if (!mData.GetInterpolatedState(mPlaybackTime, state))
        return;

    mGhost->SetGraphicsTransform(state.GetTransform(), state.mLinearVelocity, state.mAngularVelocity);
}
