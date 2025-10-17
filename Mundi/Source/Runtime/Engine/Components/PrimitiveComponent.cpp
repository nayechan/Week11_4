#include "pch.h"
#include "PrimitiveComponent.h"
#include "SceneComponent.h"

IMPLEMENT_CLASS(UPrimitiveComponent)

void UPrimitiveComponent::SetMaterial(const FString& FilePath)
{
    Material = UResourceManager::GetInstance().Load<UMaterial>(FilePath);
}

void UPrimitiveComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
}

void UPrimitiveComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    // UPrimitiveComponent는 기본적으로 직렬화할 추가 데이터가 없음
    // Material은 파생 클래스에서 개별적으로 관리
    // 파생 클래스에서 필요한 데이터를 직렬화하도록 오버라이드
}
