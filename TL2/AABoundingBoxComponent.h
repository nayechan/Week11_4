#pragma once
#include "ShapeComponent.h"

#pragma once
#include "ShapeComponent.h"

struct FBound
{
    FVector Min;
    FVector Max;

    FBound() : Min(FVector()), Max(FVector()) {}
    FBound(const FVector& InMin, const FVector& InMax) : Min(InMin), Max(InMax) {}
};

class ULine;
class UAABoundingBoxComponent :
    public UShapeComponent
{
    DECLARE_CLASS(UAABoundingBoxComponent, UShapeComponent)
public:
    UAABoundingBoxComponent();

    // 주어진 로컬 버텍스들로부터 Min/Max 계산
    void SetFromVertices(const TArray<FVector>& Verts);
    void SetFromVertices(const TArray<FNormalVertex>& Verts);

    void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) override;

    // 월드 좌표계에서의 AABB 반환
    FBound GetWorldBoundFromCube() const;
    FBound GetWorldBoundFromSphere() const;

    TArray<FVector4> GetLocalCorners() const;

    void SetPrimitiveType(EPrimitiveType InType) { PrimitiveType = InType; }

private:
    void CreateLineData(
        const FVector& Min, const FVector& Max,
        OUT TArray<FVector>& Start,
        OUT TArray<FVector>& End,
        OUT TArray<FVector4>& Color);

    FVector LocalMin;
    FVector LocalMax;

    EPrimitiveType PrimitiveType = EPrimitiveType::Default;
};

