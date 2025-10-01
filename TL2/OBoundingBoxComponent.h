#pragma once
#include "ShapeComponent.h"

struct FBox
{
    FVector Min;
    FVector Max;

    FBox() : Min(FVector()), Max(FVector()) {}
    FBox(const FVector& InMin, const FVector& InMax) : Min(InMin), Max(InMax) {}
};

class UOBoundingBoxComponent :
    public UShapeComponent
{
    DECLARE_CLASS(UOBoundingBoxComponent,UShapeComponent)
public:
    UOBoundingBoxComponent();

    // 주어진 로컬 버텍스들로부터 Min/Max 계산
    void SetFromVertices(const std::vector<FVector>& Verts);

    // 월드 좌표계에서의 AABB 반환
    FBox GetWorldBox() const;

    // 로컬 공간에서의 Extent (절반 크기)
    FVector GetExtent() const;

    // 로컬 기준 8개 꼭짓점 반환
    std::vector<FVector> GetLocalCorners() const;

    FBox GetWorldOBBFromAttachParent() const;

	void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) override;
    // Debug 렌더링용
    // void DrawDebug(ID3D11DeviceContext* DC);

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UOBoundingBoxComponent)

private:
    FVector LocalMin;
    FVector LocalMax;
};

