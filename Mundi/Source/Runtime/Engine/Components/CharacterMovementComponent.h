#pragma once
#include "PawnMovementComponent.h"
#include "UCharacterMovementComponent.generated.h"

UCLASS(DisplayName = "캐릭터 이동 컴포넌트", Description = "캐릭터 이동 컴포넌트입니다")
class UCharacterMovementComponent : public UPawnMovementComponent
{
	
public:
	GENERATED_REFLECTION_BODY()
	UCharacterMovementComponent() {};
	void TickComponent(float DeltaSeconds) override;

private:
	void ApplyVelocityBraking(float DeltaSeconds);

	UPROPERTY(LuaReadWrite, EditAnywhere, Category = "Parameter", Tooltip = "캐릭터를 움직일 때 적용되는 가속도를 결정합니다.")
	float MaxAcceleration = 10.0f;

	// 초당 속도 감소 비율(현재 속도에 고정된 비율로 역방향 가속도를 만들어냄)
	// 1초 뒤에 멈춰야 할 것 같지만 속도가 줄어들면서 역방향 가속도도 줄어들기 때문에 영원히 멈추지 않음
	UPROPERTY(LuaReadWrite, EditAnywhere, Category = "Parameter", Tooltip = "현재 속도에 따라 캐릭터에 적용되는 역방향 가속도 비율을 결정합니다")
	float Friction = 1.0f;

	// 영원히 멈추지 않는 현상을 해결하기 위해 아래의 고정 감속 계수를 씀
	// 현재 속도에 상관 없이 선형으로 속도 감소시킴. 이건 정말 1초 뒤에 속도가 1만큼 줄어들음.
	UPROPERTY(LuaReadWrite, EditAnywhere, Category = "Parameter", Tooltip = "현재 속도에 상관 없이 캐릭터에 적용되는 역방향 가속도입니다")
	float BrakingDeceleration = 1.0f;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category = "Parameter", Tooltip = "이동 방향으로 회전하는 속도를 조절합니다")
	float InterpFactor = 1000.0f;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category = "Parameter", Tooltip = "현재 플레이어의 이동 방향으로 서서히 회전합니다")
	bool bOrientRotationToMovement = true;
};