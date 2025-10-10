#include "pch.h"
#include "OBB.h"
        
FOBB::FOBB() : Center(0.f, 0.f, 0.f)
    , HalfExtent(0.f, 0.f, 0.f)
    , Axes{ {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f} } {}

FOBB::FOBB(const FAABB& LocalAABB, const FMatrix& WorldMatrix)
{
    const FVector LocalCenter = LocalAABB.GetCenter();
    const FVector LocalHalfExtent = LocalAABB.GetHalfExtent();

    const FVector4 Center4(LocalCenter.X, LocalCenter.Y, LocalCenter.Z, 1.0f);
    const FVector4 WorldCenter4 = Center4 * WorldMatrix;
    Center = FVector(WorldCenter4.X, WorldCenter4.Y, WorldCenter4.Z);

    const FVector4 WorldAxisX4 = FVector4(1.0f, 0.0f, 0.0f, 0.0f) * WorldMatrix;
    const FVector4 WorldAxisY4 = FVector4(0.0f, 1.0f, 0.0f, 0.0f) * WorldMatrix;
    const FVector4 WorldAxisZ4 = FVector4(0.0f, 0.0f, 1.0f, 0.0f) * WorldMatrix;

    const FVector WorldAxisX(WorldAxisX4.X, WorldAxisX4.Y, WorldAxisX4.Z);
    const FVector WorldAxisY(WorldAxisY4.X, WorldAxisY4.Y, WorldAxisY4.Z);
    const FVector WorldAxisZ(WorldAxisZ4.X, WorldAxisZ4.Y, WorldAxisZ4.Z);

    const float AxisXLength = WorldAxisX.Size();
    const float AxisYLength = WorldAxisY.Size();
    const float AxisZLength = WorldAxisZ.Size();

    Axes[0] = (AxisXLength > KINDA_SMALL_NUMBER) ? WorldAxisX * (1.0f / AxisXLength) : FVector{0.f, 0.f, 0.f};
    Axes[1] = (AxisYLength > KINDA_SMALL_NUMBER) ? WorldAxisY * (1.0f / AxisYLength) : FVector{0.f, 0.f, 0.f};
    Axes[2] = (AxisZLength > KINDA_SMALL_NUMBER) ? WorldAxisZ * (1.0f / AxisZLength) : FVector{0.f, 0.f, 0.f};
}

FOBB::FOBB(const FVector& InCenter, const FVector& InHalfExtent, const FVector (&InAxes)[3])
    : Center(InCenter), HalfExtent(InHalfExtent)
{
    for (int i = 0; i < 3; ++i)
        Axes[i] = InAxes[i];
}

FVector FOBB::GetCenter() const
{
    return Center;
}

FVector FOBB::GetHalfExtent() const
{
    return HalfExtent;
}

bool FOBB::Contains(const FVector& Point) const
{
    const FVector VectorToPoint = Point - Center;
    const float Limits[3] = { HalfExtent.X, HalfExtent.Y, HalfExtent.Z };

    for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
    {
        const FVector& Axis = Axes[AxisIndex];
        const float Limit = Limits[AxisIndex];

        if (Axis.SizeSquared() <= KINDA_SMALL_NUMBER)
        {
            // Axis is zero vector -> Invalid OBB
            return false;
        }

        const float Projection = FVector::Dot(VectorToPoint, Axis);
        if (Projection > Limit || Projection < -Limit)
        {
            return false;
        }
    }
    return true;
}


bool FOBB::Contains(const FOBB& Other) const
{
    const TArray<FVector> Corners = Other.GetCorners();
    for (const FVector& Corner : Corners)
    {
        if (!Contains(Corner))
        {
            return false;
        }
    }
    return true;
}

TArray<FVector> FOBB::GetCorners() const
{
    TArray<FVector> Corners;
    Corners.reserve(8);

    const FVector LocalOffsets[8] =
    {
        { +HalfExtent.X, +HalfExtent.Y, +HalfExtent.Z },
        { +HalfExtent.X, +HalfExtent.Y, -HalfExtent.Z },
        { +HalfExtent.X, -HalfExtent.Y, +HalfExtent.Z },
        { +HalfExtent.X, -HalfExtent.Y, -HalfExtent.Z },
        { -HalfExtent.X, +HalfExtent.Y, +HalfExtent.Z },
        { -HalfExtent.X, +HalfExtent.Y, -HalfExtent.Z },
        { -HalfExtent.X, -HalfExtent.Y, +HalfExtent.Z },
        { -HalfExtent.X, -HalfExtent.Y, -HalfExtent.Z }
    };

    for (const FVector& Offset : LocalOffsets)
    {
        Corners.emplace_back(
            Center
            + Axes[0] * Offset.X
            + Axes[1] * Offset.Y
            + Axes[2] * Offset.Z);
    }

    return Corners;
}

bool FOBB::Intersects(const FOBB& Other) const
{
    UE_LOG("WARNING: Method 'FOBB::Intersects' is not Implemented.");
    return false;
}

bool FOBB::IntersectsRay(const FRay& InRay, float& OutEnterDistance, float& OutExitDistance) const
{
    UE_LOG("WARNING: Method 'FOBB::IntersectsRay' is not Implemented.");
    return false;
}