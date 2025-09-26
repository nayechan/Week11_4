#include"pch.h"
#include "AABoundingBoxComponent.h"
#include "SelectionManager.h"
#include "Line.h"   

UAABoundingBoxComponent::UAABoundingBoxComponent()
    : LocalMin(FVector{}), LocalMax(FVector{})
{
}

using std::max;
using std::sqrt;

void UAABoundingBoxComponent::SetFromVertices(const TArray<FVector>& Verts)
{
    if (Verts.empty()) return;

    LocalMin = LocalMax = Verts[0];
    for (const auto& v : Verts)
    {
        LocalMin = LocalMin.ComponentMin(v);
        LocalMax = LocalMax.ComponentMax(v);
    }
}

void UAABoundingBoxComponent::SetFromVertices(const TArray<FNormalVertex>& Verts)
{
    if (Verts.empty()) return;

    LocalMin = LocalMax = Verts[0].pos;
    for (const auto& v : Verts)
    {
        LocalMin = LocalMin.ComponentMin(v.pos);
        LocalMax = LocalMax.ComponentMax(v.pos);
    }
}

void UAABoundingBoxComponent::Render(URenderer* Renderer, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix)
{
    if (USelectionManager::GetInstance().GetSelectedActor() == GetOwner())
    {
        TArray<FVector> Start;
        TArray<FVector> End;
        TArray<FVector4> Color;

        FBound WorldBound = GetWorldBoundFromCube();
        CreateLineData(WorldBound.Min, WorldBound.Max, Start, End, Color);
        Renderer->AddLines(Start, End, Color);
    }
}

FBound UAABoundingBoxComponent::GetWorldBoundFromCube() const
{
    auto corners = GetLocalCorners();

    FMatrix WorldMat = GetOwner()->GetWorldMatrix();
    FVector4 MinW = corners[0] * WorldMat;
    FVector4 MaxW = MinW;

    for (auto& c : corners)
    {
        FVector4 wc = c * WorldMat;
        MinW = MinW.ComponentMin(wc);
        MaxW = MaxW.ComponentMax(wc);
    }

    return FBound({ MinW.X, MinW.Y, MinW.Z }, { MaxW.X, MaxW.Y, MaxW.Z});
}

FBound UAABoundingBoxComponent::GetWorldBoundFromSphere() const
{
    //중심 이동
    const FVector WorldCenter = GetOwner()->GetActorLocation();
    const FMatrix WorldMat = GetOwner()->GetWorldMatrix();

    //월드 축 기준 스케일 값 계산
    FVector WorldScaleExtents;
    WorldScaleExtents.X = sqrtf(
        WorldMat.M[0][0] * WorldMat.M[0][0] +
        WorldMat.M[1][0] * WorldMat.M[1][0] + 
        WorldMat.M[2][0] * WorldMat.M[2][0]
    );
    WorldScaleExtents.Y = sqrtf(
        WorldMat.M[0][1] * WorldMat.M[0][1] +
        WorldMat.M[1][1] * WorldMat.M[1][1] +
        WorldMat.M[2][1] * WorldMat.M[2][1]
    );
    WorldScaleExtents.Z = sqrtf(
        WorldMat.M[0][2] * WorldMat.M[0][2] +
        WorldMat.M[1][2] * WorldMat.M[1][2] +
        WorldMat.M[2][2] * WorldMat.M[2][2]
    );

    //최종 AABB
    FVector Min = WorldCenter - WorldScaleExtents;
    FVector Max = WorldCenter + WorldScaleExtents;
    return FBound(Min, Max);
}

TArray<FVector4> UAABoundingBoxComponent::GetLocalCorners() const
{
    return 
    {
        {LocalMin.X, LocalMin.Y, LocalMin.Z, 1},
        {LocalMax.X, LocalMin.Y, LocalMin.Z, 1},
        {LocalMin.X, LocalMax.Y, LocalMin.Z, 1},
        {LocalMax.X, LocalMax.Y, LocalMin.Z, 1},
        {LocalMin.X, LocalMin.Y, LocalMax.Z, 1},
        {LocalMax.X, LocalMin.Y, LocalMax.Z, 1},
        {LocalMin.X, LocalMax.Y, LocalMax.Z, 1},
        {LocalMax.X, LocalMax.Y, LocalMax.Z, 1}
    };
}

void UAABoundingBoxComponent::CreateLineData(
    const FVector& Min, const FVector& Max,
    OUT TArray<FVector>& Start,
    OUT TArray<FVector>& End,
    OUT TArray<FVector4>& Color)
{
    // 8개 꼭짓점 정의
    const FVector v0(Min.X, Min.Y, Min.Z);
    const FVector v1(Max.X, Min.Y, Min.Z);
    const FVector v2(Max.X, Max.Y, Min.Z);
    const FVector v3(Min.X, Max.Y, Min.Z);
    const FVector v4(Min.X, Min.Y, Max.Z);
    const FVector v5(Max.X, Min.Y, Max.Z);
    const FVector v6(Max.X, Max.Y, Max.Z);
    const FVector v7(Min.X, Max.Y, Max.Z);

    // 선 색상 정의
    const FVector4 LineColor(1.0f, 1.0f, 0.0f, 1.0f); // 노란색

    // --- 아래쪽 면 ---
    Start.Add(v0); End.Add(v1); Color.Add(LineColor);
    Start.Add(v1); End.Add(v2); Color.Add(LineColor);
    Start.Add(v2); End.Add(v3); Color.Add(LineColor);
    Start.Add(v3); End.Add(v0); Color.Add(LineColor);

    // --- 위쪽 면 ---
    Start.Add(v4); End.Add(v5); Color.Add(LineColor);
    Start.Add(v5); End.Add(v6); Color.Add(LineColor);
    Start.Add(v6); End.Add(v7); Color.Add(LineColor);
    Start.Add(v7); End.Add(v4); Color.Add(LineColor);

    // --- 옆면 기둥 ---
    Start.Add(v0); End.Add(v4); Color.Add(LineColor);
    Start.Add(v1); End.Add(v5); Color.Add(LineColor);
    Start.Add(v2); End.Add(v6); Color.Add(LineColor);
    Start.Add(v3); End.Add(v7); Color.Add(LineColor);
}
