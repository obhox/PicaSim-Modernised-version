#ifndef DAMAGE_MANAGER_H
#define DAMAGE_MANAGER_H

#include "Framework.h"

#include <vector>
#include <string>

class Aeroplane;
class Wing;

/// Owns the opt-in "crash damage" (break-off components) state for a single
/// aeroplane: which wing panels have broken off, the lightweight tumbling debris
/// rigid bodies that replace them, and the reset that restores a fresh aircraft
/// on relaunch.
///
/// Everything here is gated on FrameworkSettings::mCrashDamage. When that is
/// false IsEnabled() is false, ProcessImpacts() does nothing, no panel is ever
/// broken and no debris is created, so the flight physics are byte-identical to
/// before this class existed (the only per-frame cost being that each wing's
/// effectiveness stays at its 1.0 default, taking the guarded no-op path in the
/// aero code).
class DamageManager
{
public:
    /// One broken-off panel, drawn as a tumbling box approximation of the panel.
    struct Debris
    {
        std::string        mComponentName; // matches the graphical box / model component name (may be empty)
        Transform          mWorldTM;       // world transform, refreshed from the body each physics step
        Vector3            mExtents;        // box extents (panel size)
        class btRigidBody* mBody;
        class btCollisionShape* mShape;
        float              mSettleTime;
    };
    typedef std::vector<Debris> DebrisList;

    DamageManager();
    ~DamageManager();

    /// wings/aeroplaneBody are owned by AeroplanePhysics and must outlive this.
    void Init(Aeroplane* aeroplane, std::vector<Wing*>* wings, class btRigidBody* aeroplaneBody, float impactImpulseThreshold);
    void Terminate();

    /// Restores every component + effectiveness=1.0 and removes all debris, so a
    /// relaunch is a fresh, undamaged aircraft.
    void Reset();

    /// True only when crash damage is enabled in the framework settings.
    bool IsEnabled() const;

    /// Post-physics: scan the aeroplane body's contact manifolds for a hard
    /// impact (applied impulse over the per-aircraft threshold) and, if found,
    /// break the nearest intact wing panel nearest that contact.
    void ProcessImpacts(float deltaTime, const Transform& objectTM, const Vector3& comWorldPos, const Vector3& vel, const Vector3& angVel);

    /// Post-physics: refresh debris transforms from their rigid bodies.
    void UpdateDebris(float deltaTime);

    bool IsComponentBroken(const std::string& name) const;
    const DebrisList& GetDebris() const {return mDebris;}

private:
    Wing* FindNearestBreakableWing(const Vector3& contactWorld, const Transform& objectTM) const;
    void  BreakWing(Wing* wing, const Transform& objectTM, const Vector3& comWorldPos, const Vector3& vel, const Vector3& angVel);
    bool  IsDebrisBody(const class btCollisionObject* body) const;
    void  ClearDebris();

    Aeroplane*         mAeroplane;
    std::vector<Wing*>* mWings;
    class btRigidBody* mAeroplaneBody;
    float              mImpactThreshold;
    int                mMaxBreaks;

    std::vector<std::string> mBrokenComponents;
    DebrisList         mDebris;
};

#endif
