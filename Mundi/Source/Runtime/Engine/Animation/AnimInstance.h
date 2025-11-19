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
	// 초기화 파이프라인
	// ========================================

	// 최종 초기화 함수 (OwnerComponent 설정 후 1회 호출)
	// Native + Lua 초기화를 순차적으로 실행
	void InitializeAnimation();

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

	// C++ 네이티브 초기화 로직
	// 하위 클래스에서 Super::NativeInitializeAnimation() 호출 후 커스텀 로직 구현
	virtual void NativeInitializeAnimation();

	// C++ 네이티브 업데이트 로직
	// 하위 클래스에서 Super::NativeUpdateAnimation() 호출 후 커스텀 로직 구현
	virtual void NativeUpdateAnimation(float DeltaSeconds);

	// Lua 스크립트 초기화 (ULuaAnimInstance에서 구현)
	virtual void LuaInitializeAnimation();

	// Lua 스크립트 업데이트 (ULuaAnimInstance에서 구현)
	virtual void LuaUpdateAnimation(float DeltaSeconds);

	// ========================================
	// 내부 시스템 함수들
	// ========================================

	// Notify 트리거링 (Unreal 방식)
	// FPoseContext에 수집된 Notify들을 일괄 트리거
	// Duration > 0인 경우 Begin/Tick/End 패턴으로 처리
	void TriggerAnimNotifies(const struct FPoseContext& Pose, float DeltaSeconds);

	// Curve 트리거링 (Unreal 방식)
	// FPoseContext에 수집된 Curve 값들을 일괄 트리거
	void TriggerAnimCurves(const struct FPoseContext& Pose);

	// Owner component 접근자
	class USkeletalMeshComponent* GetOwnerComponent() const { return OwnerComponent; }

	// World 접근 (OwnerComponent를 통해 간접 접근)
	class UWorld* GetWorld() const;

protected:
	// ⭐ CurrentTime, PreviousTime 제거
	// Unreal 방식: AnimInstance는 전역 시간을 관리하지 않음
	// 각 AnimNode가 자신의 InternalTime을 관리

	// Lua에서 접근 가능하도록 UPROPERTY 추가
	UPROPERTY(LuaReadWrite)
	class USkeletalMeshComponent* OwnerComponent = nullptr;

	// Duration이 있는 Notify (NotifyState) 활성 상태 추적
	struct FActiveAnimNotify
	{
		FAnimNotifyEvent Notify;    // 원본 Notify 데이터
		float RemainingTime;        // 남은 지속 시간
	};
	TArray<FActiveAnimNotify> ActiveAnimNotifies;

	friend class USkeletalMeshComponent;
};
