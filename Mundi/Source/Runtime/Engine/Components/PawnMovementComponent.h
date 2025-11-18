#pragma once
#include "MovementComponent.h"
#include "UPawnMovementComponent.generated.h"

class APawn;

UCLASS(Abstract, DisplayName = "폰 이동 컴포넌트", Description = "폰 이동 컴포넌트입니다")
class UPawnMovementComponent : public UMovementComponent
{
	GENERATED_REFLECTION_BODY()

public:
	UPawnMovementComponent() {};
	void InitializeComponent() override;
	// bForce: 입력 무시 상태일 때(아무리 입력해도 폰이 움직이지 않는 상태)도 강제로 이동시키는 플래그 
	virtual void AddInputVector(FVector WorldVector, bool bForce = false);
	bool IsMoveInputIgnored() const;
	// 왜 Add랑 Consume을 나누지?: 여러 인풋이 들어왔을 때 방향벡터들을 더한 후 정규화 해야함.(대각선 이동이 부자연스러워짐)
	// 입력을 받을때마다 이동 처리하면 입력을 받을때마다 충돌처리도 같이 해야함.
	// 근데 입력을 모아뒀다가 하나로 합쳐서 처리하면 한 번만 충돌처리해도 됨.
	virtual FVector ConsumeInputVetor();

protected:
	FVector ControlInputVector;
	TWeakPtr<APawn> PawnOwner;
};
