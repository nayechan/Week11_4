#include "pch.h"
#include "CharacterMovementComponent.h"
#include "PlayerController.h"
#include "SceneComponent.h"
#include "Character.h"

void UCharacterMovementComponent::TickComponent(float DeltaSeconds)
{

	FVector InputVector = ConsumeInputVetor();

	Acceleration = InputVector * MaxAcceleration;

	ApplyVelocityBraking(DeltaSeconds);

	Velocity = Velocity + Acceleration * DeltaSeconds;

	if (PawnOwner.IsValid())
	{  
		ACharacter* Character = static_cast<ACharacter*>(PawnOwner.Get());
		if (Character->IsUseControllerRotationYaw())
		{
			FVector Euler = Character->Controller.Get()->GetControlRotation().ToEulerZYXDeg();
			Euler.Y = 0.0f;
			FQuat YawQuat = FQuat::MakeFromEulerZYX(Euler);
			UpdatedComponent->SetWorldRotation(YawQuat);
		}
		else if(Velocity.Size() >0.3f)
		{
			FVector TargetDirection = Velocity.GetSafeNormal();
			FVector OriginDirection = UpdatedComponent->GetWorldRotation().RotateVector(FVector(1.0f, 0.0f, 0.0f));
			// atan2가 -180 ~ 180이라서 0~360도 변환
			float YawTarget = RadiansToDegrees(std::atan2(TargetDirection.Y, TargetDirection.X));
			float YawOrigin = RadiansToDegrees(std::atan2(OriginDirection.Y, OriginDirection.X));

			YawTarget += (YawTarget < 0.0f) ? 360.0f : 0.0f;
			YawOrigin += (YawOrigin < 0.0f) ? 360.0f : 0.0f;
			float YawInterpolated = FMath::RInterpTo(YawOrigin, YawTarget, DeltaSeconds, InterpFactor);

			UpdatedComponent->SetWorldRotation(FQuat::FromAxisAngle(FVector(0, 0, 1), DegreesToRadians(YawInterpolated)));
		}
	}

	MoveUpdatedComponent(DeltaSeconds);
}

void UCharacterMovementComponent::ApplyVelocityBraking(float DeltaSeconds)
{
	FVector OldVelocity = Velocity;
	FVector FrictionVector = -Velocity * Friction;
	FVector BrakingVector = -Velocity * BrakingDeceleration;

	Velocity = Velocity + (FrictionVector + BrakingVector) * DeltaSeconds;

	if (FVector::Dot(OldVelocity, Velocity) <= 0.0f)
	{
		Velocity = FVector::Zero();
	}
}
