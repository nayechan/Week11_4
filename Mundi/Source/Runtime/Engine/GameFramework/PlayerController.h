#pragma once

#include "Actor.h"
#include "APlayerController.generated.h"
class APawn;
class APlayerCameraManager;

UCLASS(DisplayName = "APlayerController", Description = "APlayerController 액터")
class APlayerController : public AActor
{
	GENERATED_REFLECTION_BODY()
public:

	APlayerController();
	~APlayerController();

	void Possess(APawn* InPawn);
	
	void BeginPlay() override;
	void Tick(float DeltaSecond) override;

	APlayerCameraManager* GetPlayerCameraManager();
	const FQuat& GetControlRotation() { return ControlRotation; }

	void ProcessInput();
	void ProcessMouseInput();

private:

	// 빙의 대상
	TWeakPtr<APawn> Pawn;

	FQuat ControlRotation;
	float MouseSensitivity = 0.1f;  // 기존 World에서 사용하던 값으로 조정
	float CameraYawDeg = 0.0f;   
	float CameraPitchDeg = 0.0f; 

	APlayerCameraManager* PlayerCameraManager = nullptr;
};