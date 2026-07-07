#include "DamageManager.h"

#include "Aeroplane.h"
#include "PicaSim.h"
#include "Wing.h"

//======================================================================================================================
DamageManager::DamageManager()
    : mAeroplane(0)
    , mWings(0)
    , mAeroplaneBody(0)
    , mImpactThreshold(FLT_MAX)
    , mMaxBreaks(4)
{
}

//======================================================================================================================
DamageManager::~DamageManager()
{
    // Debris bodies must already have been removed from the world via Terminate().
    // (We can't safely touch the dynamics world from a destructor at shutdown.)
}

//======================================================================================================================
void DamageManager::Init(Aeroplane* aeroplane, std::vector<Wing*>* wings, btRigidBody* aeroplaneBody, float impactImpulseThreshold)
{
    mAeroplane = aeroplane;
    mWings = wings;
    mAeroplaneBody = aeroplaneBody;
    mImpactThreshold = impactImpulseThreshold;
    mBrokenComponents.clear();
    mDebris.clear();
}

//======================================================================================================================
void DamageManager::Terminate()
{
    ClearDebris();
    mBrokenComponents.clear();
    mAeroplane = 0;
    mWings = 0;
    mAeroplaneBody = 0;
}

//======================================================================================================================
void DamageManager::ClearDebris()
{
    // Called only from Reset()/Terminate() during normal operation, when the
    // EntityManager (and thus the dynamics world) is still alive.
    for (size_t i = 0 ; i != mDebris.size() ; ++i)
    {
        Debris& d = mDebris[i];
        if (d.mBody)
        {
            EntityManager::GetInstance().GetDynamicsWorld().removeRigidBody(d.mBody);
            delete d.mBody;
            d.mBody = 0;
        }
        delete d.mShape;
        d.mShape = 0;
    }
    mDebris.clear();
}

//======================================================================================================================
void DamageManager::Reset()
{
    // Restore every wing's aerodynamic effectiveness -> fresh, undamaged aircraft.
    if (mWings)
    {
        for (size_t i = 0 ; i != mWings->size() ; ++i)
            (*mWings)[i]->SetEffectiveness(1.0f);
    }
    mBrokenComponents.clear();
    ClearDebris();
}

//======================================================================================================================
bool DamageManager::IsEnabled() const
{
    if (!PicaSim::IsCreated())
        return false;
    return PicaSim::GetInstance().GetSettings().mOptions.mFrameworkSettings.mCrashDamage;
}

//======================================================================================================================
bool DamageManager::IsDebrisBody(const btCollisionObject* body) const
{
    for (size_t i = 0 ; i != mDebris.size() ; ++i)
    {
        if (mDebris[i].mBody == body)
            return true;
    }
    return false;
}

//======================================================================================================================
bool DamageManager::IsComponentBroken(const std::string& name) const
{
    if (name.empty())
        return false;
    for (size_t i = 0 ; i != mBrokenComponents.size() ; ++i)
    {
        if (mBrokenComponents[i] == name)
            return true;
    }
    return false;
}

//======================================================================================================================
Wing* DamageManager::FindNearestBreakableWing(const Vector3& contactWorld, const Transform& objectTM) const
{
    if (!mWings)
        return 0;

    Wing* best = 0;
    float bestDistSq = FLT_MAX;
    for (size_t i = 0 ; i != mWings->size() ; ++i)
    {
        Wing* wing = (*mWings)[i];
        // Skip panels that have already broken off.
        if (wing->GetEffectiveness() <= 0.0f)
            continue;
        // Panel centre in world space (same placement the graphics uses).
        Vector3 centreWorld = objectTM.TransformVec(wing->GetTMLocal().GetTrans());
        float distSq = (centreWorld - contactWorld).GetLengthSquared();
        if (distSq < bestDistSq)
        {
            bestDistSq = distSq;
            best = wing;
        }
    }
    return best;
}

//======================================================================================================================
void DamageManager::BreakWing(Wing* wing, const Transform& objectTM, const Vector3& comWorldPos, const Vector3& vel, const Vector3& angVel)
{
    // 1) Aerodynamic effect: this panel now produces no lift/drag. This is the
    //    ONLY change to the flight model, and only ever happens when damage is on.
    wing->SetEffectiveness(0.0f);
    mBrokenComponents.push_back(wing->GetName());

    // 2) Visual/physical break-off: spawn a lightweight tumbling debris box that
    //    approximates the panel, at the panel's current world pose.
    Transform worldTM = wing->GetTMLocal() * objectTM;
    Vector3 extents = wing->GetExtents();

    btVector3 halfExtents(
        Maximum(fabsf(extents.x) * 0.5f, 0.01f),
        Maximum(fabsf(extents.y) * 0.5f, 0.01f),
        Maximum(fabsf(extents.z) * 0.5f, 0.01f));

    btCollisionShape* shape = new btBoxShape(halfExtents);
    float mass = Maximum(wing->GetMass(), 0.001f);
    btVector3 inertia(0, 0, 0);
    shape->calculateLocalInertia(mass, inertia);

    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, 0, shape, inertia);
    rbInfo.m_startWorldTransform = TransformToBulletTransform(worldTM);
    rbInfo.m_friction = 0.6f;
    rbInfo.m_restitution = 0.1f;
    btRigidBody* body = new btRigidBody(rbInfo);

    // Inherit the aircraft's velocity at the panel, plus some random tumble.
    Vector3 panelPos = worldTM.GetTrans();
    Vector3 velAtPanel = vel + angVel.Cross(panelPos - comWorldPos);
    body->setLinearVelocity(Vector3ToBulletVector3(velAtPanel));
    Vector3 spin = angVel + Vector3(RangedRandom(-5.0f, 5.0f), RangedRandom(-5.0f, 5.0f), RangedRandom(-5.0f, 5.0f));
    body->setAngularVelocity(Vector3ToBulletVector3(spin));

    EntityManager::GetInstance().GetDynamicsWorld().addRigidBody(body);

    Debris d;
    d.mComponentName = wing->GetName();
    d.mWorldTM = worldTM;
    d.mExtents = Vector3(fabsf(extents.x), fabsf(extents.y), fabsf(extents.z));
    d.mBody = body;
    d.mShape = shape;
    d.mSettleTime = 0.0f;
    mDebris.push_back(d);
}

//======================================================================================================================
void DamageManager::ProcessImpacts(float deltaTime, const Transform& objectTM, const Vector3& comWorldPos, const Vector3& vel, const Vector3& angVel)
{
    if (deltaTime <= 0.0f)
        return;
    if (!IsEnabled() || !mAeroplaneBody)
        return;
    if ((int) mBrokenComponents.size() >= mMaxBreaks)
        return;

    btDynamicsWorld& world = EntityManager::GetInstance().GetDynamicsWorld();
    btDispatcher* dispatcher = world.getDispatcher();
    int numManifolds = dispatcher->getNumManifolds();
    for (int i = 0 ; i < numManifolds ; ++i)
    {
        btPersistentManifold* manifold = dispatcher->getManifoldByIndexInternal(i);
        const btCollisionObject* obA = manifold->getBody0();
        const btCollisionObject* obB = manifold->getBody1();

        if (obA != mAeroplaneBody && obB != mAeroplaneBody)
            continue;
        // Ignore contacts between the aeroplane and our own debris (no chain reactions).
        if (IsDebrisBody(obA) || IsDebrisBody(obB))
            continue;

        int numContacts = manifold->getNumContacts();
        for (int j = 0 ; j < numContacts ; ++j)
        {
            btManifoldPoint& cp = manifold->getContactPoint(j);
            // getAppliedImpulse() reflects the normal impulse the solver applied at
            // this contact in the last substep - a robust proxy for impact severity.
            if (cp.getAppliedImpulse() > mImpactThreshold)
            {
                Vector3 contactWorld = BulletVector3ToVector3(cp.getPositionWorldOnB());
                Wing* wing = FindNearestBreakableWing(contactWorld, objectTM);
                if (wing)
                {
                    BreakWing(wing, objectTM, comWorldPos, vel, angVel);
                    if ((int) mBrokenComponents.size() >= mMaxBreaks)
                        return;
                }
                break; // at most one break per manifold per frame
            }
        }
    }
}

//======================================================================================================================
void DamageManager::UpdateDebris(float deltaTime)
{
    for (size_t i = 0 ; i != mDebris.size() ; ++i)
    {
        Debris& d = mDebris[i];
        if (d.mBody)
            d.mWorldTM = BulletTransformToTransform(d.mBody->getWorldTransform());
        d.mSettleTime += deltaTime;
    }
}
