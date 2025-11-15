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

	// 애니메이션 업데이트
	virtual void NativeUpdateAnimation(float DeltaSeconds);

	// Notify 트리거링
	virtual void TriggerAnimNotifies(float DeltaSeconds);

	// 현재 시간 접근자
	float GetCurrentTime() const { return CurrentTime; }
	void SetCurrentTime(float InTime) { CurrentTime = InTime; }

	// Owner component 접근자
	class USkeletalMeshComponent* GetOwnerComponent() const { return OwnerComponent; }

protected:
	float CurrentTime = 0.0f;
	float PreviousTime = 0.0f;

	class USkeletalMeshComponent* OwnerComponent = nullptr;

	friend class USkeletalMeshComponent;
};
