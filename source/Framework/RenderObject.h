#ifndef RENDEROBJECT_H
#define RENDEROBJECT_H

#include "Helpers.h"

/// Base class from which all objects that expect to be rendered should derive
class RenderObject
{
public:
    RenderObject() : mShadowAmount(1.0f) {}

    /// Unregister from RenderManager before deleting
    virtual ~RenderObject() {}

    virtual void RenderUpdate(class Viewport* viewport, int renderLevel) = 0;

    /// CSM depth-only caster draw. Called once per cascade (with the cascade's
    /// world->light-clip matrix on the engine matrix stack and the shadowcast
    /// program bound) for objects registered as shadow casters. Default: no-op, so
    /// only objects that override this cast CSM shadows. The blob-shadow path is
    /// unaffected.
    virtual void RenderShadowCast() {}

    virtual const Transform& GetTM() const {return Transform::g_Identity;}

    virtual float GetRenderBoundingRadius() const {return FLT_MAX;}

    virtual bool GetShadowVisible() const {return true;}

    void SetShadowAmount(float amount) {mShadowAmount = amount;}

protected:
    float mShadowAmount;
};

#endif
