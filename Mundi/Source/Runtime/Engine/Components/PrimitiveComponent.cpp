#include "pch.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"
#include "Actor.h"

IMPLEMENT_CLASS(UPrimitiveComponent)

BEGIN_PROPERTIES(UPrimitiveComponent)
//MARK_AS_COMPONENT("박스 충돌 컴포넌트", "박스 크기의 충돌체를 생성하는 컴포넌트입니다.")
ADD_PROPERTY(bool, bGenerateOverlapEvents, "GenerateOverlapEvents", true, "Overlap 이벤트를 발생하는 boolean값입니다.")
END_PROPERTIES()


void UPrimitiveComponent::SetMaterialByName(uint32 InElementIndex, const FString& InMaterialName)
{
    SetMaterial(InElementIndex, UResourceManager::GetInstance().Load<UMaterial>(InMaterialName));
} 
 
void AActor::OnBeginOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp)
{
    UE_LOG("On Begin Overlap");
}
void AActor::OnHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp)
{
    UE_LOG("On Hit");
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
