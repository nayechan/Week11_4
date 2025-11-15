#pragma once
#include "Object.h"
#include "AnimationTypes.h"
#include "UAnimInstance.generated.h"

UCLASS(DisplayName="애니메이션 인스턴스", Description="애니메이션 재생 로직")
class UAnimInstance : public UObject
{
public:
	GENERATED_REFLECTION_BODY()

	UAnimInstance() = default;
	virtual ~UAnimInstance() = default;

	// ========================================
	// 메인 업데이트 파이프라인
	// ========================================

	// 최종 업데이트 함수 (하위 클래스에서 오버라이드 금지)
	// 전체 애니메이션 파이프라인을 정의
	void UpdateAnimation(float DeltaSeconds);

	// 포즈 추출 (하위 클래스에서 구현)
	// Unreal 방식: 트리를 순회하며 각 노드가 OutPose.AnimNotifies에 Notify 추가
	virtual void GetAnimationPose(struct FPoseContext& OutPose);

	// ========================================
	// 확장 포인트 (하위 클래스에서 오버라이드)
	// ========================================

	// C++ 네이티브 업데이트 로직
	// 하위 클래스에서 Super::NativeUpdateAnimation() 호출 후 커스텀 로직 구현
	virtual void NativeUpdateAnimation(float DeltaSeconds);

	// TODO: Lua 스크립트 업데이트 (향후 구현)
	// virtual void LuaUpdateAnimation(float DeltaSeconds);

	// ========================================
	// 내부 시스템 함수들
	// ========================================

	// Notify 트리거링 (Unreal 방식)
	// FPoseContext에 수집된 Notify들을 일괄 트리거
	void TriggerAnimNotifies(const struct FPoseContext& Pose);

	// Owner component 접근자
	class USkeletalMeshComponent* GetOwnerComponent() const { return OwnerComponent; }

protected:
	// ⭐ CurrentTime, PreviousTime 제거
	// Unreal 방식: AnimInstance는 전역 시간을 관리하지 않음
	// 각 AnimNode가 자신의 InternalTime을 관리

	class USkeletalMeshComponent* OwnerComponent = nullptr;

	friend class USkeletalMeshComponent;
};
