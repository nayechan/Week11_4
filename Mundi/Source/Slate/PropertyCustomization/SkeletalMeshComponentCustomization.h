#pragma once

#include "IDetailCustomization.h"
#include "PropertyCustomizationHelpers.h"

// Forward declarations
class USkeletalMeshComponent;
struct FSkeleton;

/**
 * SkeletalMeshComponent 전용 Property Customization
 * AnimToPlay 프로퍼티를 Skeleton 기준으로 필터링
 */
class FSkeletalMeshComponentCustomization : public IDetailCustomization
{
public:
	/**
	 * Factory 메서드
	 */
	static IDetailCustomization* MakeInstance();

	/**
	 * IDetailCustomization 인터페이스 구현
	 */
	virtual void CustomizeDetails(UObject* Object) override;

private:
	/**
	 * AnimToPlay 필터링 콜백
	 * @param AssetPath - 애니메이션 경로
	 * @return true면 필터링(숨김), false면 표시
	 */
	bool OnShouldFilterAnimAsset(const FString& AssetPath);

	/**
	 * Skeleton 선택 UI 렌더링
	 */
	void RenderSkeletonSelector();

	/**
	 * Skeleton 변경 콜백
	 * @param NewSkeleton - 새로운 Skeleton 포인터
	 */
	void OnSkeletonChanged(FSkeleton* NewSkeleton);

private:
	// 현재 편집 중인 컴포넌트의 Skeleton 포인터
	FSkeleton* TargetSkeleton = nullptr;

	// 현재 편집 중인 컴포넌트
	USkeletalMeshComponent* CurrentComponent = nullptr;
};
