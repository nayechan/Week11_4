#pragma once
struct FAABB;
struct FOBB;
struct FBoundingSphere;

namespace Collision
{
    bool Intersects(const FAABB& Aabb, const FOBB& Obb);

    bool Intersect(const FAABB& Aabb, const FBoundingSphere& Sphere);

	bool Intersect(const FOBB& Obb, const FBoundingSphere& Sphere);
}