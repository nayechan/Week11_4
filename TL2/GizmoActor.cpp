#include "pch.h"
#include "GridActor.h"
#include "GizmoActor.h"
#include "RenderSettings.h"
#include "GizmoArrowComponent.h"
#include "GizmoScaleComponent.h"
#include "GizmoRotateComponent.h"
#include "CameraActor.h"
#include "SelectionManager.h"
#include "InputManager.h"
#include "UI/UIManager.h"
#include"FViewport.h"
#include "Picking.h"
#include <EditorEngine.h>

AGizmoActor::AGizmoActor()
{
	Name = "Gizmo Actor";

	//======= Arrow Component 생성 =======
	/*ArrowX = NewObject<UGizmoArrowComponent>();
	ArrowY = NewObject<UGizmoArrowComponent>();
	ArrowZ = NewObject<UGizmoArrowComponent>();*/

	ArrowX = CreateDefaultSubobject<UGizmoArrowComponent>("GizmoArrowComponent");
	ArrowY = CreateDefaultSubobject<UGizmoArrowComponent>("GizmoArrowComponent");
	ArrowZ = CreateDefaultSubobject<UGizmoArrowComponent>("GizmoArrowComponent");

	ArrowX->SetDirection(FVector(1.0f, 0.0f, 0.0f));//빨 
	ArrowY->SetDirection(FVector(0.0f, 1.0f, 0.0f));//초
	ArrowZ->SetDirection(FVector(0.0f, 0.0f, 1.0f));//파

	ArrowX->SetColor(FVector(1.0f, 0.0f, 0.0f));
	ArrowY->SetColor(FVector(0.0f, 1.0f, 0.0f));
	ArrowZ->SetColor(FVector(0.0f, 0.0f, 1.0f));

	ArrowX->SetupAttachment(RootComponent);
	ArrowY->SetupAttachment(RootComponent);
	ArrowZ->SetupAttachment(RootComponent);

	ArrowX->SetRelativeScale({ 1, 1, 3 });
	ArrowY->SetRelativeScale({ 1, 1, 3 });
	ArrowZ->SetRelativeScale({ 1, 1, 3 });

	if (ArrowX) ArrowX->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0, 0, 0)));
	if (ArrowY) ArrowY->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0, 0, 90)));
	if (ArrowZ) ArrowZ->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0, -90, 0)));


	AddComponent(ArrowX);
	AddComponent(ArrowY);
	AddComponent(ArrowZ);
	GizmoArrowComponents.Add(ArrowX);
	GizmoArrowComponents.Add(ArrowY);
	GizmoArrowComponents.Add(ArrowZ);

	//======= Rotate Component 생성 =======
	RotateX = NewObject<UGizmoRotateComponent>();
	RotateY = NewObject<UGizmoRotateComponent>();
	RotateZ = NewObject<UGizmoRotateComponent>();

	RotateX->SetDirection(FVector(1.0f, 0.0f, 0.0f));
	RotateY->SetDirection(FVector(0.0f, 1.0f, 0.0f));
	RotateZ->SetDirection(FVector(0.0f, 0.0f, 1.0f));

	RotateX->SetColor(FVector(1.0f, 0.0f, 0.0f));
	RotateY->SetColor(FVector(0.0f, 1.0f, 0.0f));
	RotateZ->SetColor(FVector(0.0f, 0.0f, 1.0f));

	RotateX->SetupAttachment(RootComponent);
	RotateY->SetupAttachment(RootComponent);
	RotateZ->SetupAttachment(RootComponent);

	RotateX->SetRelativeScale({ 0.02f, 0.02f, 0.02f });
	RotateY->SetRelativeScale({ 0.02f, 0.02f, 0.02f });
	RotateZ->SetRelativeScale({ 0.02f, 0.02f, 0.02f });

	AddComponent(RotateX);
	AddComponent(RotateY);
	AddComponent(RotateZ);
	GizmoRotateComponents.Add(RotateX);
	GizmoRotateComponents.Add(RotateY);
	GizmoRotateComponents.Add(RotateZ);

	if (RotateX) RotateX->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0, 90, 0)));
	if (RotateY) RotateY->SetRelativeRotation(FQuat::MakeFromEuler(FVector(90, 0, 0)));
	if (RotateZ) RotateZ->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0, 0, 0)));

	//======= Scale Component 생성 =======
	ScaleX = NewObject<UGizmoScaleComponent>();
	ScaleY = NewObject<UGizmoScaleComponent>();
	ScaleZ = NewObject<UGizmoScaleComponent>();

	ScaleX->SetDirection(FVector(1.0f, 0.0f, 0.0f));
	ScaleY->SetDirection(FVector(0.0f, 1.0f, 0.0f));
	ScaleZ->SetDirection(FVector(0.0f, 0.0f, 1.0f));

	ScaleX->SetColor(FVector(1.0f, 0.0f, 0.0f));
	ScaleY->SetColor(FVector(0.0f, 1.0f, 0.0f));
	ScaleZ->SetColor(FVector(0.0f, 0.0f, 1.0f));

	ScaleX->SetupAttachment(RootComponent);
	ScaleY->SetupAttachment(RootComponent);
	ScaleZ->SetupAttachment(RootComponent);

	ScaleX->SetRelativeScale({ 0.02f, 0.02f, 0.02f });
	ScaleY->SetRelativeScale({ 0.02f, 0.02f, 0.02f });
	ScaleZ->SetRelativeScale({ 0.02f, 0.02f, 0.02f });

	if (ScaleX) ScaleX->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0, 90, 0)));
	if (ScaleY) ScaleY->SetRelativeRotation(FQuat::MakeFromEuler(FVector(-90, 0, 0)));
	if (ScaleZ) ScaleZ->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0, 0, 0)));

	AddComponent(ScaleX);
	AddComponent(ScaleY);
	AddComponent(ScaleZ);
	GizmoScaleComponents.Add(ScaleX);
	GizmoScaleComponents.Add(ScaleY);
	GizmoScaleComponents.Add(ScaleZ);

	CurrentMode = EGizmoMode::Translate;

	// 매니저 참조 초기화
	SelectionManager = &USelectionManager::GetInstance();
	InputManager = &UInputManager::GetInstance();
	UIManager = &UUIManager::GetInstance();
}

void AGizmoActor::Tick(float DeltaSeconds)
{
	if (!SelectionManager) SelectionManager = &USelectionManager::GetInstance();
	if (!InputManager) InputManager = &UInputManager::GetInstance();
	if (!UIManager) UIManager = &UUIManager::GetInstance();

	// 컴포넌트 활성화 상태 업데이트    
	if (SelectionManager->HasSelection() && CameraActor)
	{
		TargetActor = SelectionManager->GetSelectedActor();

		// 기즈모 위치를 선택된 액터 위치로 업데이트
		if (TargetActor)
		{
			SetSpaceWorldMatrix(CurrentSpace, TargetActor);
			SetActorLocation(TargetActor->GetActorLocation());
		}
	}
	else
	{
		TargetActor = nullptr;
	}
	UpdateComponentVisibility();
}

void AGizmoActor::Render(ACameraActor* Camera, FViewport* Viewport)
{
	UpdateConstantScreenScale(Camera, Viewport);

	TArray<USceneComponent*>* Components = GetGizmoComponents();
	if (!Components) return;


	float ViewportAspectRatio = static_cast<float>(Viewport->GetSizeX()) / static_cast<float>(Viewport->GetSizeY());
	if (Viewport->GetSizeY() == 0) ViewportAspectRatio = 1.0f; // 0으로 나누기 방지

	EViewModeIndex ViewModeIndex = World->GetRenderSettings().GetViewModeIndex();


	//@TODO 기즈모 액터에서 렌더러 접근 수정 필요
	extern UEditorEngine GEngine;
	URenderer* Renderer = GEngine.GetRenderer();


	if (!SELECTION.HasSelection()) return;
	FMatrix ViewMatrix = Camera->GetViewMatrix();
	FMatrix ProjectionMatrix = Camera->GetProjectionMatrix(ViewportAspectRatio, Viewport);
	FMatrix ModelMatrix;
	FVector rgb(1.0f, 1.0f, 1.0f);
	FVector2D ViewportSize(static_cast<float>(Viewport->GetSizeX()), static_cast<float>(Viewport->GetSizeY()));
	FVector2D ViewportOffset(static_cast<float>(Viewport->GetStartX()), static_cast<float>(Viewport->GetStartY()));
	FVector2D ViewportMousePos(static_cast<float>(UInputManager::GetInstance().GetMousePosition().X) + ViewportOffset.X,
		static_cast<float>(UInputManager::GetInstance().GetMousePosition().Y) + ViewportOffset.Y);



	for (int32 i = 0; i < Components->Num(); ++i)
	{
		USceneComponent* Component = (*Components)[i];
		if (!Component) continue;
		// 컴포넌트 활성 상태 확인
		if (UActorComponent* ActorComp = Cast<UActorComponent>(Component))
		{
			if (!ActorComp->IsActive()) continue;
		}

		ModelMatrix = Component->GetWorldMatrix();
		Renderer->UpdateConstantBuffer(ModelMatrix, ViewMatrix, ProjectionMatrix);


		if (GizmoAxis == i + 1)
		{
			Renderer->UpdateHighLightConstantBuffer(true, rgb, i + 1, 1, 0, 1);
		}
		else
		{
			Renderer->UpdateHighLightConstantBuffer(true, rgb, i + 1, 0, 0, 1);
		}


		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
		{
			Renderer->SetViewModeType(EViewModeIndex::VMI_Unlit);
			Renderer->OMSetDepthStencilState(EComparisonFunc::Always);
			Renderer->OMSetBlendState(true); // 필요 시

			Primitive->Render(Renderer, ViewMatrix, ProjectionMatrix);
			// 상태 복구
			Renderer->OMSetBlendState(false);
			Renderer->OMSetDepthStencilState(EComparisonFunc::LessEqual);
			Renderer->SetViewModeType(ViewModeIndex);
		}
	}
	//Renderer->UpdateHighLightConstantBuffer(false, rgb, 0, 0, 0, 0);

	// 알파 블랜딩을 위한 blendstate
	Renderer->OMSetBlendState(true);
}

AGizmoActor::~AGizmoActor()
{
	// Components are centrally destroyed by AActor's destructor
	ArrowX = ArrowY = ArrowZ = nullptr;
	ScaleX = ScaleY = ScaleZ = nullptr;
	RotateX = RotateY = RotateZ = nullptr;
}

void AGizmoActor::SetMode(EGizmoMode NewMode)
{
	CurrentMode = NewMode;
}
EGizmoMode  AGizmoActor::GetMode()
{
	return CurrentMode;
}

void AGizmoActor::SetSpaceWorldMatrix(EGizmoSpace NewSpace, AActor* PickedActor)
{
	SetSpace(NewSpace);

	if (NewSpace == EGizmoSpace::World)
	{

		// 월드 고정 → 기즈모 축은 항상 X/Y/Z
		   // 월드 고정 → 기즈모 축은 항상 X/Y/Z
		if (ArrowX) ArrowX->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0, 0, 0)));
		if (ArrowY) ArrowY->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0, 0, 90)));
		if (ArrowZ) ArrowZ->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0, -90, 0)));

		if (ScaleX) ScaleX->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0, 90, 0)));
		if (ScaleY) ScaleY->SetRelativeRotation(FQuat::MakeFromEuler(FVector(-90, 0, 0)));
		if (ScaleZ) ScaleZ->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0, 0, 0)));

		if (RotateX) RotateX->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0, 90, 0)));
		if (RotateY) RotateY->SetRelativeRotation(FQuat::MakeFromEuler(FVector(90, 0, 0)));
		if (RotateZ) RotateZ->SetRelativeRotation(FQuat::MakeFromEuler(FVector(0, 0, 0)));
	}
	else if (NewSpace == EGizmoSpace::Local)
	{
		if (!PickedActor)
			return;

		// 타겟 액터 회전 가져오기
		FQuat TargetRot = PickedActor->GetActorRotation();

		// ───────── Translate Gizmo ─────────
	   // ArrowX->AddRelativeRotation(AC);
		   // 월드 고정 → 기즈모 축은 항상 X/Y/Z
		if (ArrowX) ArrowX->SetRelativeRotation(TargetRot * FQuat::MakeFromEuler(FVector(0, 0, 0)));
		if (ArrowY) ArrowY->SetRelativeRotation(TargetRot * FQuat::MakeFromEuler(FVector(0, 0, 90)));
		if (ArrowZ) ArrowZ->SetRelativeRotation(TargetRot * FQuat::MakeFromEuler(FVector(0, -90, 0)));

		if (ScaleX) ScaleX->SetRelativeRotation(TargetRot * FQuat::MakeFromEuler(FVector(0, 90, 0)));
		if (ScaleY) ScaleY->SetRelativeRotation(TargetRot * FQuat::MakeFromEuler(FVector(-90, 0, 0)));
		if (ScaleZ) ScaleZ->SetRelativeRotation(TargetRot * FQuat::MakeFromEuler(FVector(0, 0, 0)));

		if (RotateX) RotateX->SetRelativeRotation(TargetRot * FQuat::MakeFromEuler(FVector(0, 90, 0)));
		if (RotateY) RotateY->SetRelativeRotation(TargetRot * FQuat::MakeFromEuler(FVector(90, 0, 0)));
		if (RotateZ) RotateZ->SetRelativeRotation(TargetRot * FQuat::MakeFromEuler(FVector(0, 0, 0)));
	}

}


void AGizmoActor::NextMode(EGizmoMode GizmoMode)
{
	CurrentMode = GizmoMode;
}

TArray<USceneComponent*>* AGizmoActor::GetGizmoComponents()
{
	switch (CurrentMode)
	{
	case EGizmoMode::Translate:
		return &GizmoArrowComponents;
	case EGizmoMode::Rotate:
		return &GizmoRotateComponents;
	case EGizmoMode::Scale:
		return &GizmoScaleComponents;
	}
	return nullptr;
}

EGizmoMode AGizmoActor::GetGizmoMode() const
{
	return CurrentMode;
}

// 개선된 축 투영 함수 - 수직 각도에서도 안정적
static FVector2D GetStableAxisDirection(const FVector& WorldAxis, const ACameraActor* Camera)
{
	const FVector CameraRight = Camera->GetRight();
	const FVector CameraUp = Camera->GetUp();
	const FVector CameraForward = Camera->GetForward();

	// 축과 카메라 forward의 각도 확인 (수직도 측정)
	float ForwardDot = FVector::Dot(WorldAxis.GetNormalized(), CameraForward.GetNormalized());
	float PerpendicularThreshold = 0.95f; // cos(약 18도)

	// 거의 수직인 경우 (축이 스크린과 거의 평행)
	if (std::fabs(ForwardDot) > PerpendicularThreshold)
	{
		// 가장 큰 성분을 가진 카메라 축 성분 사용
		float RightComponent = std::fabs(FVector::Dot(WorldAxis, CameraRight));
		float UpComponent = std::fabs(FVector::Dot(WorldAxis, CameraUp));

		if (RightComponent > UpComponent)
		{
			// Right 성분이 더 클 때: X축 우선 (원래 부호 유지)
			float Sign = FVector::Dot(WorldAxis, CameraRight) > 0 ? 1.0f : -1.0f;
			return FVector2D(Sign, 0.0f);
		}
		else
		{
			// Up 성분이 더 클 때: Y축 우선 (DirectX는 Y가 아래쪽이므로 반전)
			float Sign = FVector::Dot(WorldAxis, CameraUp) > 0 ? -1.0f : 1.0f;
			return FVector2D(0.0f, Sign);
		}
	}

	// 일반적인 경우: 스크린 투영 사용
	float RightDot = FVector::Dot(WorldAxis, CameraRight);
	float UpDot = FVector::Dot(WorldAxis, CameraUp);

	// DirectX 스크린 좌표계 고려 (Y축 반전)
	FVector2D ScreenDirection = FVector2D(RightDot, -UpDot);

	// 안전한 정규화 (최소 길이 보장)
	float Length = ScreenDirection.Length();
	if (Length > 0.001f)
	{
		return ScreenDirection * (1.0f / Length);
	}

	// 예외 상황: 기본 X축 방향
	return FVector2D(1.0f, 0.0f);
}

void AGizmoActor::OnDrag(AActor* Target, uint32 GizmoAxis, float MouseDeltaX, float MouseDeltaY, const ACameraActor* Camera, FViewport* Viewport)
{
	if (!Target || !Camera)
		return;

	FVector2D MouseDelta = FVector2D(MouseDeltaX, MouseDeltaY);

	FVector Axis{};
	FVector GizmoPosition = GetActorLocation();

	// ────────────── World / Local 축 선택 ──────────────
	if (CurrentSpace == EGizmoSpace::World)
	{
		switch (GizmoAxis)
		{
		case 1: Axis = FVector(1, 0, 0); break;
		case 2: Axis = FVector(0, 1, 0); break;
		case 3: Axis = FVector(0, 0, 1); break;
		}
	}
	else if (CurrentSpace == EGizmoSpace::Local)
	{
		switch (GizmoAxis)
		{
		case 1: Axis = Target->GetActorRight();   break; // Local X
		case 2: Axis = Target->GetActorForward(); break; // Local Y
		case 3: Axis = Target->GetActorUp();      break; // Local Z
		}
	}

	// ────────────── 모드별 처리 ──────────────
	switch (CurrentMode)
	{
	case EGizmoMode::Translate:
	{
		FVector2D ScreenAxis = GetStableAxisDirection(Axis, Camera);
		float px = (MouseDelta.X * ScreenAxis.X + MouseDelta.Y * ScreenAxis.Y);
		float h = Viewport ? static_cast<float>(Viewport->GetSizeY()) : UInputManager::GetInstance().GetScreenSize().Y;
		if (h <= 0.0f) h = 1.0f;
		float w = Viewport ? static_cast<float>(Viewport->GetSizeX()) : UInputManager::GetInstance().GetScreenSize().X;
		float aspect = w / h;
		FMatrix Proj = Camera->GetProjectionMatrix(aspect, Viewport);
		bool bOrtho = std::fabs(Proj.M[3][3] - 1.0f) < KINDA_SMALL_NUMBER;
		float worldPerPixel = 0.0f;
		if (bOrtho)
		{
			float halfH = 1.0f / Proj.M[1][1];
			worldPerPixel = (2.0f * halfH) / h;
		}
		else
		{
			float yScale = Proj.M[1][1];
			FVector camPos = Camera->GetActorLocation();
			FVector gizPos = GetActorLocation();
			FVector camF = Camera->GetForward();
			float z = FVector::Dot(gizPos - camPos, camF);
			if (z < 1.0f) z = 1.0f;
			worldPerPixel = (2.0f * z) / (h * yScale);
		}
		float Movement = px * worldPerPixel;
		FVector CurrentLocation = Target->GetActorLocation();
		Target->SetActorLocation(CurrentLocation + Axis * Movement);

		break;
	}
	case EGizmoMode::Scale:
	{
		// Determine axis for screen projection
		if (CurrentSpace == EGizmoSpace::World)
		{
			switch (GizmoAxis)
			{
			case 1: Axis = FVector(1, 0, 0); break;
			case 2: Axis = FVector(0, 1, 0); break;
			case 3: Axis = FVector(0, 0, 1); break;
			}
		}
		else if (CurrentSpace == EGizmoSpace::Local)
		{
			switch (GizmoAxis)
			{
			case 1: Axis = Target->GetActorRight();   break; // Local X
			case 2: Axis = Target->GetActorForward(); break; // Local Y
			case 3: Axis = Target->GetActorUp();      break; // Local Z
			}
		}

		FVector2D ScreenAxis = GetStableAxisDirection(Axis, Camera);
		float px = (MouseDelta.X * ScreenAxis.X + MouseDelta.Y * ScreenAxis.Y);
		float h = Viewport ? static_cast<float>(Viewport->GetSizeY()) : UInputManager::GetInstance().GetScreenSize().Y;
		if (h <= 0.0f) h = 1.0f;
		float w = Viewport ? static_cast<float>(Viewport->GetSizeX()) : UInputManager::GetInstance().GetScreenSize().X;
		float aspect = w / h;
		FMatrix Proj = Camera->GetProjectionMatrix(aspect, Viewport);
		bool bOrtho = std::fabs(Proj.M[3][3] - 1.0f) < KINDA_SMALL_NUMBER;
		float worldPerPixel = 0.0f;
		if (bOrtho)
		{
			float halfH = 1.0f / Proj.M[1][1];
			worldPerPixel = (2.0f * halfH) / h;
		}
		else
		{
			float yScale = Proj.M[1][1];
			FVector camPos = Camera->GetActorLocation();
			FVector gizPos = GetActorLocation();
			FVector camF = Camera->GetForward();
			float z = FVector::Dot(gizPos - camPos, camF);
			if (z < 1.0f) z = 1.0f;
			worldPerPixel = (2.0f * z) / (h * yScale);
		}
		float Movement = px * worldPerPixel;
		FVector NewScale = Target->GetActorScale();

		// Apply movement to the correct local axis based on which gizmo was dragged
		switch (GizmoAxis)
		{
		case 1: // X Axis
			NewScale.X += Movement;
			break;
		case 2: // Y Axis
			NewScale.Y += Movement;
			break;
		case 3: // Z Axis
			NewScale.Z += Movement;
			break;
		}

		Target->SetActorScale(NewScale);

		break;
	}
	case EGizmoMode::Rotate:
	{
		float RotationSpeed = 0.005f;
		float DeltaAngleX = MouseDeltaX * RotationSpeed;
		float DeltaAngleY = MouseDeltaY * RotationSpeed;

		float Angle = DeltaAngleX + DeltaAngleY;

		// 로컬 모드일 경우 축을 Target 로컬 축으로
		FVector RotationAxis = Axis.GetSafeNormal();

		// = MakeQuatFromAxisAngle(RotationAxis.X, Angle);
		FQuat DeltaQuat{};
		FQuat CurrentRot = Target->GetActorRotation();
		if (CurrentSpace == EGizmoSpace::World)
		{

			switch (GizmoAxis)
			{
			case 1: // X축 회전
			{
				// 마우스 X → 카메라 Up 축 기반
				FQuat RotByX = MakeQuatFromAxisAngle(FVector(-1, 0, 0), DeltaAngleX);
				// 마우스 Y → 카메라 Right 축 기반
				FQuat RotByY = MakeQuatFromAxisAngle(FVector(-1, 0, 0), DeltaAngleY);
				DeltaQuat = RotByX * RotByY;
				break;
			}
			case 2: // Y축 회전
			{
				FQuat RotByX = MakeQuatFromAxisAngle(FVector(0, -1, 0), DeltaAngleX);
				FQuat RotByY = MakeQuatFromAxisAngle(FVector(0, -1, 0), DeltaAngleY);
				DeltaQuat = RotByX * RotByY;
				break;
			}
			case 3: // Z축 회전
			{
				FQuat RotByX = MakeQuatFromAxisAngle(FVector(0, 0, -1), DeltaAngleX);
				FQuat RotByY = MakeQuatFromAxisAngle(FVector(0, 0, -1), DeltaAngleY);
				DeltaQuat = RotByX * RotByY;
				break;
			}
			}
			FQuat NewRot = DeltaQuat * CurrentRot; // 월드 기준 회전
			Target->SetActorRotation(NewRot);
		}
		else
		{
			float RotationSpeed = 0.005f;
			float DeltaAngleX = MouseDeltaX * RotationSpeed;
			float DeltaAngleY = MouseDeltaY * RotationSpeed;

			float Angle = DeltaAngleX + DeltaAngleY;

			// 로컬 모드일 경우 축을 Target 로컬 축으로
			FVector RotationAxis = -Axis.GetSafeNormal();

			//FQuat DeltaQuat = MakeQuatFromAxisAngle(RotationAxis, Angle);

			FQuat CurrentRot = Target->GetActorRotation();

			switch (GizmoAxis)
			{
			case 1: // X축 회전
			{
				// 마우스 X → 카메라 Up 축 기반
				FQuat RotByX = MakeQuatFromAxisAngle(RotationAxis, DeltaAngleX);
				// 마우스 Y → 카메라 Right 축 기반
				FQuat RotByY = MakeQuatFromAxisAngle(RotationAxis, DeltaAngleY);
				DeltaQuat = RotByX * RotByY;
				break;
			}
			case 2: // Y축 회전
			{
				FQuat RotByX = MakeQuatFromAxisAngle(RotationAxis, DeltaAngleX);
				FQuat RotByY = MakeQuatFromAxisAngle(RotationAxis, DeltaAngleY);
				DeltaQuat = RotByX * RotByY;
				break;
			}
			case 3: // Z축 회전
			{
				FQuat RotByX = MakeQuatFromAxisAngle(RotationAxis, DeltaAngleX);
				FQuat RotByY = MakeQuatFromAxisAngle(RotationAxis, DeltaAngleY);
				DeltaQuat = RotByX * RotByY;
				break;
			}
			}

			FQuat NewRot = DeltaQuat * CurrentRot; // 월드 기준 회전

			Target->SetActorRotation(NewRot);


			break;
		}
	}
	}
}



void AGizmoActor::UpdateGizmoPosition()
{
	if (!TargetActor) return;

	SetActorLocation(TargetActor->GetActorLocation());
}

void AGizmoActor::ProcessGizmoInteraction(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY)
{
	if (!TargetActor || !Camera) return;


	UpdateConstantScreenScale(Camera, Viewport);

	ProcessGizmoModeSwitch();

	// 기즈모 드래그
	ProcessGizmoDragging(Camera, Viewport, MousePositionX, MousePositionY);

	ProcessGizmoHovering(Camera, Viewport, MousePositionX, MousePositionY);

}

void AGizmoActor::ProcessGizmoHovering(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY)
{
	if (!Camera) return;

	FVector2D ViewportSize(static_cast<float>(Viewport->GetSizeX()), static_cast<float>(Viewport->GetSizeY()));
	FVector2D ViewportOffset(static_cast<float>(Viewport->GetStartX()), static_cast<float>(Viewport->GetStartY()));
	FVector2D ViewportMousePos(static_cast<float>(MousePositionX) + ViewportOffset.X,
		static_cast<float>(MousePositionY) + ViewportOffset.Y);
	if (!bIsDragging)
		GizmoAxis = CPickingSystem::IsHoveringGizmoForViewport(this, Camera, ViewportMousePos, ViewportSize, ViewportOffset, Viewport);

	if (GizmoAxis > 0)//기즈모 축이 0이상이라면 선택 된것 
	{
		bIsHovering = true;
	}
	else
	{
		bIsHovering = false;
	}

}

void AGizmoActor::ProcessGizmoDragging(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY)
{
	if (!TargetActor || !Camera) return;

	if (InputManager->IsMouseButtonDown(LeftButton))
	{
		FVector2D MouseDelta = InputManager->GetMouseDelta();
		if ((MouseDelta.X * MouseDelta.X + MouseDelta.Y * MouseDelta.Y) > 0.0f)
		{
			OnDrag(TargetActor, GizmoAxis, MouseDelta.X, MouseDelta.Y, Camera, Viewport);
			bIsDragging = true;
			SetActorLocation(TargetActor->GetActorLocation());
		}
	}
	if (InputManager->IsMouseButtonReleased(LeftButton))
	{
		bIsDragging = false;
		GizmoAxis = 0;
		SetSpaceWorldMatrix(CurrentSpace, TargetActor);
	}
}

void AGizmoActor::ProcessGizmoModeSwitch()
{
	// 스페이스 키로 기즈모 모드 전환
	if (InputManager->IsKeyPressed(VK_SPACE))
	{
		int GizmoModeIndex = static_cast<int>(GetMode());
		GizmoModeIndex = (GizmoModeIndex + 1) % 3;  // 3 = enum 개수
		EGizmoMode NewGizmoMode = static_cast<EGizmoMode>(GizmoModeIndex);
		NextMode(NewGizmoMode);
	}
}

void AGizmoActor::UpdateComponentVisibility()
{
	// 선택된 액터가 없으면 모든 기즈모 컴포넌트를 비활성화
	bool bHasSelection = SelectionManager && SelectionManager->HasSelection();

	// Arrow Components (Translate 모드)
	bool bShowArrows = bHasSelection && (CurrentMode == EGizmoMode::Translate);
	if (ArrowX) ArrowX->SetActive(bShowArrows);
	if (ArrowY) ArrowY->SetActive(bShowArrows);
	if (ArrowZ) ArrowZ->SetActive(bShowArrows);

	// Rotate Components (Rotate 모드)
	bool bShowRotates = bHasSelection && (CurrentMode == EGizmoMode::Rotate);
	if (RotateX) RotateX->SetActive(bShowRotates);
	if (RotateY) RotateY->SetActive(bShowRotates);
	if (RotateZ) RotateZ->SetActive(bShowRotates);

	// Scale Components (Scale 모드)
	bool bShowScales = bHasSelection && (CurrentMode == EGizmoMode::Scale);
	if (ScaleX) ScaleX->SetActive(bShowScales);
	if (ScaleY) ScaleY->SetActive(bShowScales);
	if (ScaleZ) ScaleZ->SetActive(bShowScales);
}

void AGizmoActor::UpdateConstantScreenScale(ACameraActor* Camera, FViewport* Viewport)
{
	if (!Camera || !Viewport) return;
	float h = static_cast<float>(Viewport->GetSizeY());
	if (h <= 0.0f) h = 1.0f;
	float w = static_cast<float>(Viewport->GetSizeX());
	float aspect = w / h;
	FMatrix Proj = Camera->GetProjectionMatrix(aspect, Viewport);
	bool bOrtho = std::fabs(Proj.M[3][3] - 1.0f) < KINDA_SMALL_NUMBER;
	float targetPx = 30.0f;
	float scale = 1.0f;
	if (bOrtho)
	{
		float halfH = 1.0f / Proj.M[1][1];
		scale = (targetPx * 2.0f * halfH) / h;
	}
	else
	{
		float yScale = Proj.M[1][1];
		FVector camPos = Camera->GetActorLocation();
		FVector gizPos = GetActorLocation();
		FVector camF = Camera->GetForward();
		float z = FVector::Dot(gizPos - camPos, camF);
		if (z < 1.0f) z = 1.0f;
		scale = (targetPx * 2.0f * z) / (h * yScale);
	}
	if (scale < 0.001f) scale = 0.001f;
	if (scale > 10000.0f) scale = 10000.0f;
	SetActorScale(FVector(scale, scale, scale));
}

void AGizmoActor::OnDrag(AActor* Target, uint32 GizmoAxis, float MouseDeltaX, float MouseDeltaY, const ACameraActor* Camera)
{
	OnDrag(Target, GizmoAxis, MouseDeltaX, MouseDeltaY, Camera, nullptr);
}
