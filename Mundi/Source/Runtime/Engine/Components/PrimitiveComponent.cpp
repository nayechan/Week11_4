#include "pch.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"
#include "Actor.h"

IMPLEMENT_CLASS(UPrimitiveComponent)


void UPrimitiveComponent::SetMaterialByName(uint32 InElementIndex, const FString& InMaterialName)
{
    SetMaterial(InElementIndex, UResourceManager::GetInstance().Load<UMaterial>(InMaterialName));
} 
 
void AActor::OnBeginOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp)
{
}
void AActor::OnHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp)
{
}
void UPrimitiveComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

void UPrimitiveComponent::OnSerialized()
{
    Super::OnSerialized();
}

bool UPrimitiveComponent::IsOverlappingActor(const AActor* Other) const
{
    if (!Other)
    {
        return false;
    }

    const TArray<FOverlapInfo>& Infos = GetOverlapInfos();
    for (const FOverlapInfo& Info : Infos)
    {
        if (Info.Other)
        {
            if (AActor* Owner = Info.Other->GetOwner())
            {
                if (Owner == Other)
                {
                    return true;
                }
            }
        }
    }
    return false;
}
