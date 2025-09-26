#pragma once
#include "Vector.h"
#include "AABoundingBoxComponent.h"
#include "CameraComponent.h"


class UCameraComponent;

struct Plane
{
    // unit vector
    FVector Normal = { 0.f, 1.f, 0.f };

    // 원점으로부터 평면까지의 거리이다.
    float Distance = 0.f;
};

struct Frustum
{
    Plane TopFace;
    Plane BottomFace;
    Plane RightFace;
    Plane LeftFace;
    Plane NearFace;
    Plane FarFace;
};

Frustum CreateFrustumFromCamera(const UCameraComponent& Camera, float OverrideAspect = -1.0f);
bool IsAABBVisible(const Frustum& Frustum, const FBound& Bound);
bool Intersects(const Plane& P, const FVector& Center, const FVector& Extents);