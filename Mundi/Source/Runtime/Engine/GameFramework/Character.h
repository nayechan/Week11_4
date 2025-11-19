#pragma once
#include "Pawn.h"
#include "ACharacter.generated.h"

class UCapsuleComponent;
class UCharacterMovementComponent;
class USkeletalMeshComponent;
class UCharacterAnimInstance;

UCLASS(DisplayName = "ACharacter", Description = "Character Actor")
class ACharacter : public APawn
{

	GENERATED_REFLECTION_BODY();
public:
	ACharacter();

	void HandleThrustInput(float InScalar) override;
	void HandleSteerInput(float InScalar) override;
	void DuplicateSubObjects() override;
	
	// 캐릭터 무브먼트 컴포넌트와 소통
	float GetSpeed();

	// 컨트롤러와 소통
	bool IsUseControllerRotationYaw() const { return bUseControllerRotationYaw; }

private:
	UCapsuleComponent* CapsuleComponent = nullptr;

	UCharacterMovementComponent* CharacterMovementComponent = nullptr; 
	 
	USkeletalMeshComponent* SkeletalMeshComponent = nullptr;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category = "Parameter", Tooltip = "캐릭터가 항상 카메라가 바라보는 방향을 바라봅니다")
	bool bUseControllerRotationYaw = false;
};