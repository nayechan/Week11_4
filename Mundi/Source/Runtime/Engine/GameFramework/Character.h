#pragma once
#include "Pawn.h"
#include "ACharacter.generated.h"

class UCapsuleComponent;
class UCharacterMovementComponent;
class USkeletalMeshComponent;

UCLASS(DisplayName = "ACharacter", Description = "Character Actor")
class ACharacter : public APawn
{

	GENERATED_REFLECTION_BODY();
public:
	ACharacter();

	void HandleThrustInput(float InScalar) override;
	void HandleSteerInput(float InScalar) override;
	void DuplicateSubObjects() override;

	bool IsUseControllerRotationYaw() const { return bUseControllerRotationYaw; }

private:
	UPROPERTY()
	UCapsuleComponent* CapsuleComponent = nullptr;

	UPROPERTY()
	UCharacterMovementComponent* CharacterMovementComponent = nullptr; 
	 
	UPROPERTY()
	USkeletalMeshComponent* SkeletalMeshComponent = nullptr;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category = "Parameter", Tooltip = "캐릭터가 항상 카메라가 바라보는 방향을 바라봅니다")
	bool bUseControllerRotationYaw = false;
};