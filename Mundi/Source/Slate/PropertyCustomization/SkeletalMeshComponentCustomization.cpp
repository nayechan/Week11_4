#include "pch.h"
#include "SkeletalMeshComponentCustomization.h"
#include "PropertyRenderer.h"
#include "SkeletalMeshComponent.h"
#include "SkeletalMesh.h"
#include "AnimSequence.h"
#include "ResourceManager.h"
#include "GlobalConsole.h"

IDetailCustomization* FSkeletalMeshComponentCustomization::MakeInstance()
{
	return new FSkeletalMeshComponentCustomization();
}

void FSkeletalMeshComponentCustomization::CustomizeDetails(UObject* Object)
{
	USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Object);
	if (!SkelComp)
	{
		UE_LOG("FSkeletalMeshComponentCustomization: Object is not USkeletalMeshComponent");
		return;
	}

	// Skeleton 추출
	TargetSkeleton = nullptr;
	if (SkelComp->GetSkeletalMesh())
	{
		TargetSkeleton = const_cast<FSkeleton*>(SkelComp->GetSkeletalMesh()->GetSkeleton());
	}

	// 현재 AnimToPlay 유효성 검사
	if (SkelComp->AnimToPlay)
	{
		UAnimSequence* CurrentAnim = SkelComp->AnimToPlay;

		// TargetSkeleton이 없거나 다른 Skeleton을 사용하면 초기화
		if (!TargetSkeleton || CurrentAnim->Skeleton != TargetSkeleton)
		{
			SkelComp->AnimToPlay = nullptr;
			UE_LOG("AnimToPlay cleared: incompatible with current SkeletalMesh");
		}
	}

	// 필터 델리게이트 생성
	FOnShouldFilterAsset FilterDelegate;
	FilterDelegate.Bind([this](const FString& AssetPath) -> bool {
		return OnShouldFilterAnimAsset(AssetPath);
	});

	// PropertyRenderer 정적 메서드로 필터와 함께 프로퍼티 렌더링 요청
	UPropertyRenderer::RenderPropertiesWithCustomization(SkelComp, FilterDelegate);
}

bool FSkeletalMeshComponentCustomization::OnShouldFilterAnimAsset(const FString& AssetPath)
{
	// 빈 경로 ("None")는 항상 표시
	if (AssetPath.empty())
	{
		return false;
	}

	// TargetSkeleton이 없으면 모든 애니메이션 숨김 (None만 표시)
	if (!TargetSkeleton)
	{
		return true;
	}

	// 애니메이션 로드 및 Skeleton 확인
	UAnimSequence* Anim = UResourceManager::GetInstance().Get<UAnimSequence>(AssetPath);
	if (!Anim)
	{
		// 로드 실패 시 숨김
		return true;
	}

	// Skeleton 포인터 비교
	if (Anim->Skeleton == TargetSkeleton)
	{
		return false;  // 같은 Skeleton이면 표시
	}

	// 다른 Skeleton이면 숨김
	return true;
}
