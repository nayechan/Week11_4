#include "pch.h"

#include "PlayerController.h"
#include "PlayerCameraManager.h"
#include "InputManager.h"
#include "Pawn.h"

APlayerController::APlayerController()
	:PlayerCameraManager(NewObject<APlayerCameraManager>())
{
}

APlayerController::~APlayerController()
{
	if (PlayerCameraManager)
	{
		//PlayerCameraManager->Destroy();
		ObjectFactory::DeleteObject(PlayerCameraManager);
	}
}

// WeakPtr = 연산자가 APawn정의를 알아야 해서 cpp에 작성
void APlayerController::Possess(APawn* InPawn)
{
	Pawn = InPawn;
}

void APlayerController::BeginPlay()
{
	Super::BeginPlay();

	PlayerCameraManager->BeginPlay();
}

void APlayerController::Tick(float DeltaSecond)
{
	ProcessInput();
	if (PlayerCameraManager)
	{
		PlayerCameraManager->Tick(DeltaSecond);
	}
}

APlayerCameraManager* APlayerController::GetPlayerCameraManager()
{
	return PlayerCameraManager;
}

void APlayerController::ProcessInput()
{
	if (!Pawn.IsValid())
	{
		return;
	}

	UInputManager& InputManager = UInputManager::GetInstance();


	// 마우스 회전
	ProcessMouseInput();
	
	if (InputManager.IsKeyDown('W'))
	{
		Pawn->HandleThrustInput(1.0f);
	}
	if (InputManager.IsKeyReleased('W'))
	{
		Pawn->HandleThrustInput(0.0f);
	}
	if (InputManager.IsKeyDown('S'))
	{
		Pawn->HandleThrustInput(-1.0f);
	}
	if (InputManager.IsKeyReleased('S'))
	{
		Pawn->HandleThrustInput(0.0f);
	}

	if (InputManager.IsKeyDown('D'))
	{
		Pawn->HandleSteerInput(1.0f);
	}
	if (InputManager.IsKeyReleased('D'))
	{
		Pawn->HandleSteerInput(0.0f);
	}
	if (InputManager.IsKeyDown('A'))
	{
		Pawn->HandleSteerInput(-1.0f);
	}
	if (InputManager.IsKeyReleased('A'))
	{
		Pawn->HandleSteerInput(0.0f);
	}

	// 부스터 제어 키
	if (InputManager.IsKeyPressed(VK_SPACE))
	{
		Pawn->HandleBoosterInput();
	}
}

static inline float Clamp(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

void APlayerController::ProcessMouseInput()
{
	UInputManager& InputManager = UInputManager::GetInstance();
	if (InputManager.IsMouseButtonDown(EMouseButton::RightButton))
	{
		FVector2D MouseDelta = InputManager.GetMouseDelta();

		if (MouseDelta.X == 0.0f && MouseDelta.Y == 0.0f) return;

		CameraYawDeg += MouseDelta.X * MouseSensitivity;
		CameraPitchDeg += MouseDelta.Y * MouseSensitivity;

		// 각도 정규화 및 Pitch 제한
		CameraYawDeg = NormalizeAngleDeg(CameraYawDeg); // -180 ~ 180 범위로 정규화
		CameraPitchDeg = Clamp(CameraPitchDeg, -89.0f, 89.0f); // Pitch는 짐벌락 방지를 위해 제한

		FQuat PitchQuat = FQuat::FromAxisAngle(FVector(0, 1, 0), DegreesToRadians(CameraPitchDeg));
		FQuat YawQuat = FQuat::FromAxisAngle(FVector(0, 0, 1), DegreesToRadians(CameraYawDeg));

		FQuat FinalRotation = YawQuat * PitchQuat;
		FinalRotation.Normalize();
		ControlRotation = FinalRotation;
	}
}
