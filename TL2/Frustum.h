#pragma once
#include "Vector.h"
#include "AABoundingBoxComponent.h"
#include "CameraComponent.h"


class UCameraComponent;

struct Plane
{
    // unit vector
    FVector4 Normal = { 0.f, 1.f, 0.f , 0.f };

    // 평면이 원점에서 법선 N 방향으로 얼마만큼 떨어져 있는지
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
bool Intersects(const Plane& P, const FVector4& Center, const FVector4& Extents);