#include "pch.h"
#include "PawnMovementComponent.h"
#include "Pawn.h"

void UPawnMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// InitializeComponent에서 Pawn 설정 해주는 이유: 액터 생성자에서 오너 설정 까먹을 수 있음
	// 컴포넌트가 알아서 처리하도록 하면 코드 반복도 사라지고 사용하기도 편해짐.
	if (Owner)
	{
		if (APawn* Pawn = Cast<APawn>(Owner))
		{
			PawnOwner = Pawn;
		}
	}
}

void UPawnMovementComponent::AddInputVector(FVector WorldVector, bool bForce)
{
	if (!bForce && IsMoveInputIgnored())
	{
		return;
	}

	ControlInputVector += WorldVector;
}

FVector UPawnMovementComponent::ConsumeInputVetor()
{
	FVector Result = ControlInputVector;

	Result.Normalize();

	ControlInputVector = FVector::Zero();
	
	return Result;
}

bool UPawnMovementComponent::IsMoveInputIgnored() const
{
	if (PawnOwner.IsValid())
	{
		return PawnOwner.Get()->IsMoveInputIgnored();
	}

	return false;
}
