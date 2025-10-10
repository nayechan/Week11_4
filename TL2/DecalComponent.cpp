#include "pch.h"
#include "DecalComponent.h"
#include "OBB.h"

void UDecalComponent::Serialize(bool bIsLoading, FDecalData& InOut)
{
}

void UDecalComponent::DuplicateSubObjects()
{
    
}

void UDecalComponent::RenderAffectedPrimitives(URenderer* Renderer, UPrimitiveComponent* Target, const FMatrix& View, const FMatrix& Proj)
{
    // TODO: 실제 렌더 부분
    //Renderer->GetRHIDevice()->Update
}

void UDecalComponent::RenderDebugVolume(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) const
{
}

void UDecalComponent::SetDecalTexture(UTexture* InTexture)
{
}

void UDecalComponent::SetDecalTexture(const FString& TexturePath)
{
}

FAABB UDecalComponent::GetWorldAABB() const
{
    return FAABB();
}

FOBB UDecalComponent::GetOBB() const
{
    return FOBB();
}

FMatrix UDecalComponent::GetDecalProjectionMatrix() const
{
    return FMatrix();
}
