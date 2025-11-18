#pragma once
#include "SceneComponent.h"
#include "USpringArmComponent.generated.h"

UCLASS(DisplayName = "스프링 암 컴포넌트", Description = "스프링 암 컴포넌트입니다")
class USpringArmComponent : public USceneComponent
{
public:
	GENERATED_REFLECTION_BODY()

	USpringArmComponent();

	void OnRegister(UWorld* InWorld) override;
	void BeginPlay() override;

	FTransform GetSocketWorldTransform(const FName& InSocketName) const override;

	void TickComponent(float DeltaTime) override;
	
	void RenderDebugVolume(class URenderer* Renderer) const override;

	void EvaluateArm(float DeltaTime);

	void DuplicateSubObjects() override;

private:
	// Cache
	mutable FTransform CachedSocketWorld;
	mutable bool bSocketValid = false;

	// Socket name
	UPROPERTY(LuaReadWrite, EditAnywhere, Category = "[소켓]")
	FName SocketName = "SpringArmSocket";

	// Settings (Property)
	UPROPERTY(LuaReadWrite, EditAnywhere, Category = "[스프링 암 컴포넌트]")
	float TargetArmLength = 10.0f;               // 원하는 암 길이
	UPROPERTY(LuaReadWrite, EditAnywhere, Category = "[스프링 암 컴포넌트]")
	FVector SocketOffset = { 0.0f, 0.0f, 0.0f }; // 암 끝 기준 위치 오프셋
	UPROPERTY(LuaReadWrite, EditAnywhere, Category = "[스프링 암 컴포넌트]")
	FVector TargetOffset = { 0.0f, 0.0f, 0.0f }; // "무엇을 볼지" 피벗 보정(부모 좌표계 기준)
	UPROPERTY(LuaReadWrite, EditAnywhere, Category = "[스프링 암 컴포넌트]")
	float BackoffEpsilon = 0.1f;                 // 히트 시 살짝 당겨서 떨림 방지

	FQuat InitialRotation;
	UPROPERTY(LuaReadWrite, EditAnywhere, Category = "[스프링 암 컴포넌트]")
	bool bUseControllerRotation = true;	// 컨트롤러의 Rotation 사용

	// 랙 상태
	UPROPERTY(LuaReadWrite, EditAnywhere, Category = "[스프링 암 컴포넌트]")
	bool bInitLag = false;
	UPROPERTY(LuaReadWrite, EditAnywhere, Category = "[스프링 암 컴포넌트]")
	bool  bEnableLag = true;
	UPROPERTY(LuaReadWrite, EditAnywhere, Category = "[스프링 암 컴포넌트]")
	float LagSpeed = 15.0f;        // 속도가 높을수록 빠르게 따라옴
	UPROPERTY(LuaReadWrite, EditAnywhere, Category = "[스프링 암 컴포넌트]")
	float MaxLagDistance = 10.0f;  // 목표 위치로부터 최대 이격 거리
	FVector SmoothedSocketPosWS;
};
