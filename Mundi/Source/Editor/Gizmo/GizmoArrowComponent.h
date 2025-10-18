#pragma once
#include "StaticMeshComponent.h"
class UGizmoArrowComponent : public UStaticMeshComponent
{
public:
    DECLARE_CLASS(UGizmoArrowComponent, UStaticMeshComponent)
    UGizmoArrowComponent();
    
    void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

protected:
    ~UGizmoArrowComponent() override;
    
public:
    const FVector& GetDirection() const { return Direction; }
    const FVector& GetColor() const { return Color; }

    void SetDirection(const FVector& InDirection) { Direction = InDirection; }
    void SetColor(const FVector& InColor) { Color = InColor; }

    // Gizmo visual state
    void SetAxisIndex(uint32 InAxisIndex) { AxisIndex = InAxisIndex; }
    void SetDefaultScale(const FVector& InSize) { DefaultScale = InSize; }
    void SetHighlighted(bool bInHighlighted, uint32 InAxisIndex) { bHighlighted = bInHighlighted; AxisIndex = InAxisIndex; }
    bool IsHighlighted() const { return bHighlighted; }
    uint32 GetAxisIndex() const { return AxisIndex; }

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UGizmoArrowComponent)
protected:
    float ComputeScreenConstantScale(const FSceneView* View, float TargetPixels) const;

protected:
    FVector Direction;
    FVector DefaultScale{ 1.f,1.f,1.f };
    FVector Color;
    bool bHighlighted = false;
    uint32 AxisIndex = 0;
    
    // 기즈모가 항상 사용할 고정 머티리얼입니다.
    UMaterial* GizmoMaterial = nullptr;
};

