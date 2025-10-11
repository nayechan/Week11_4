#pragma once
#include "PrimitiveComponent.h"
#include "AABB.h"
#include "Vector.h"

// Forward declarations
struct FOBB;
class UTexture;
struct FDecalProjectionData;

/**
 * UDecalComponent - Projection Decal implementation
 * 
 * Decal volume은 USceneComponent의 Location, Rotation, Scale로 정의됩니다:
 * - Location: Decal의 중심 위치
 * - Rotation: Decal의 투영 방향 (Forward = -Z 방향으로 투영)
 * - Scale: Decal volume의 크기 (X=Width, Y=Height, Z=Depth)
 */
class UDecalComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UDecalComponent, USceneComponent)
    
    UDecalComponent();
    
protected:
    ~UDecalComponent() override = default;

public:
    // Render API
    void RenderAffectedPrimitives(URenderer* Renderer, UPrimitiveComponent* Target, const FMatrix& View, const FMatrix& Proj);
    void RenderDebugVolume(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) const;
    
    // Decal Resource API
    void SetDecalTexture(UTexture* InTexture);
    void SetDecalTexture(const FString& TexturePath);
    UTexture* GetDecalTexture() const { return DecalTexture; }

    // Decal Property API
    void SetVisibility(bool bVisible) { bIsVisible = bVisible; }
    bool IsVisible() const { return bIsVisible; }
    void SetOpacity(float Opacity) { DecalOpacity = FMath::Clamp(Opacity, 0.0f, 1.0f); }
    float GetOpacity() const { return DecalOpacity; }
    
    // Decal Volume & Bounds API
    FAABB GetWorldAABB() const;
    FOBB GetOBB() const;
    
    // Projection & UV Mapping API
    FMatrix GetDecalProjectionMatrix() const;
    
    // Affected Objects Management API
    // TArray<UStaticMeshComponent*> FindAffectedComponents() const;
    // void AddAffectedComponent(UStaticMeshComponent* Component);
    // void RemoveAffectedComponent(UStaticMeshComponent* Component);
    // void ClearAffectedComponents();
    // const TArray<UStaticMeshComponent*>& GetAffectedComponents() const { return AffectedComponents; }
    // void RefreshAffectedComponents();
    
    // Serialization API
    void Serialize(bool bIsLoading, struct FDecalData& InOut);

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UDecalComponent)

    // Tick
    virtual void TickComponent(float DeltaTime) override;

private:
    UTexture* DecalTexture = nullptr;
    
    bool bIsVisible = true;
    float DecalOpacity = 1.0f;

    // for PIE Tick
    float FadeSpeed = 0.5f;   // 초당 변화 속도 (0.5 = 2초에 완전 페이드)
    int FadeDirection = -1;   // -1 = 감소 중, +1 = 증가 중

    // TArray<UStaticMeshComponent*> AffectedComponents;
};