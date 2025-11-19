#include "pch.h"
#include "Actor.h"
#include "Character.h"
#include "SkeletalMeshComponent.h"
#include "CapsuleComponent.h"
#include "CharacterMovementComponent.h"
#include "PlayerController.h"
#include "AudioComponent.h"

ACharacter::ACharacter()
{
	
	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("SkeletalMeshComponent");
	// 언리얼에서는 Capsule이 루트인데 현재 문디엔진에 적용하기 힘든 구조라서 SkeletalMesh를 씀
	RootComponent = SkeletalMeshComponent;

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>("CapsuleComponent");
	CharacterMovementComponent = CreateDefaultSubobject<UCharacterMovementComponent>("CharacterMovementComponent");
	// MovementComponent InitalizeComponent함수에서 알아서 오너 엑터 찾아서 루트컴포넌트를 UpdatedComponnet로 설정함.
	//MovementComponent->SetUpdatedComponent(SkeletalMeshComponent);

	AudioComponent = CreateDefaultSubobject<UAudioComponent>("AudioComponent");
	AudioComponent->SetupAttachment(RootComponent);
	CapsuleComponent->SetupAttachment(RootComponent);
}

void ACharacter::HandleThrustInput(float InScalar)
{
	if (CharacterMovementComponent)
	{
		if (Controller.IsValid())
		{
			FVector Forward = Controller.Get()->GetControlRotation().RotateVector(FVector(1.0f, 0.0f, 0.0f));
			Forward.Z = 0.0f;
			CharacterMovementComponent->AddInputVector(Forward * InScalar);
		}
	}
	
}

void ACharacter::HandleSteerInput(float InScalar)
{
	if (CharacterMovementComponent)
	{
		if (Controller.IsValid())
		{
			FVector Right = Controller.Get()->GetControlRotation().RotateVector(FVector(0.0f, 1.0f, 0.0f));
			Right.Z = 0.0f;
			CharacterMovementComponent->AddInputVector(Right * InScalar);
		}
	}
}

void ACharacter::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	for (UActorComponent* Component : OwnedComponents)
	{
		if (USkeletalMeshComponent* Skeletal = Cast<USkeletalMeshComponent>(Component))
		{
			SkeletalMeshComponent = Skeletal;
		}
		else if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Component))
		{
			CapsuleComponent = Capsule;
		}
		else if (UCharacterMovementComponent* MovementComponent = Cast<UCharacterMovementComponent>(Component))
		{
			CharacterMovementComponent = MovementComponent;
		}
		else if (UAudioComponent* OwnedAudioComponent = Cast<UAudioComponent>(Component))
		{
			AudioComponent = OwnedAudioComponent;
		}
	}
}

void ACharacter::HandleAnimNotify(const FAnimNotifyEvent& Notify)
{
	if (CharacterMovementComponent->GetSpeed() <= 0.1f)
	{
		return;
	}
	if (Notify.NotifyName == FName("Step"))
	{
		AudioComponent->Play();
	}
}

float ACharacter::GetSpeed()
{
	if (CharacterMovementComponent)
	{
		//TODO : Speed 0~1 사이로 정규화하기
		return CharacterMovementComponent->GetSpeed();
	}
	return 0.0f;
}
