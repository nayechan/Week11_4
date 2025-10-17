#pragma once
#include "SceneComponent.h"
#include "Material.h"

// 전방 선언
struct FSceneCompData;

class URenderer;

class UPrimitiveComponent :public USceneComponent
{
public:
    DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

    UPrimitiveComponent() = default;
    virtual ~UPrimitiveComponent() = default;

    virtual void SetMaterial(const FString& FilePath);
    virtual UMaterial* GetMaterial() { return Material; }

    virtual void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) {}

    void SetCulled(bool InCulled)
    {
        bIsCulled = InCulled;
    }

    bool GetCulled() const
    {
        return bIsCulled;
    }

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UPrimitiveComponent)

    // ───── 직렬화 ────────────────────────────
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
    UMaterial* Material = nullptr;
    bool bIsCulled = false;
};