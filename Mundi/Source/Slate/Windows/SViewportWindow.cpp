#include "pch.h"
#include "SViewportWindow.h"
#include "World.h"
#include "ImGui/imgui.h"
#include "USlateManager.h"

#include "FViewport.h"
#include "FViewportClient.h"
#include "Texture.h"
#include "Gizmo/GizmoActor.h"

#include "CameraComponent.h"
#include "CameraActor.h"

extern float CLIENTWIDTH;
extern float CLIENTHEIGHT;

SViewportWindow::SViewportWindow()
{
	ViewportType = EViewportType::Perspective;
	bIsActive = false;
	bIsMouseDown = false;
}

SViewportWindow::~SViewportWindow()
{
	if (Viewport)
	{
		delete Viewport;
		Viewport = nullptr;
	}

	if (ViewportClient)
	{
		delete ViewportClient;
		ViewportClient = nullptr;
	}

	IconSelect = nullptr;
	IconMove = nullptr;
	IconRotate = nullptr;
	IconScale = nullptr;
	IconWorldSpace = nullptr;
	IconLocalSpace = nullptr;
}

bool SViewportWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* World, ID3D11Device* Device, EViewportType InViewportType)
{
	ViewportType = InViewportType;

	// 이름 설정
	switch (ViewportType)
	{
	case EViewportType::Perspective:		ViewportName = "원근"; break;
	case EViewportType::Orthographic_Front: ViewportName = "정면"; break;
	case EViewportType::Orthographic_Left:  ViewportName = "왼쪽"; break;
	case EViewportType::Orthographic_Top:   ViewportName = "상단"; break;
	case EViewportType::Orthographic_Back:	ViewportName = "후면"; break;
	case EViewportType::Orthographic_Right:  ViewportName = "오른쪽"; break;
	case EViewportType::Orthographic_Bottom:   ViewportName = "하단"; break;
	}

	// FViewport 생성
	Viewport = new FViewport();
	if (!Viewport->Initialize(StartX, StartY, Width, Height, Device))
	{
		delete Viewport;
		Viewport = nullptr;
		return false;
	}

	// FViewportClient 생성
	ViewportClient = new FViewportClient();
	ViewportClient->SetViewportType(ViewportType);
	ViewportClient->SetWorld(World); // 전역 월드 연결 (이미 있다고 가정)

	// 양방향 연결
	Viewport->SetViewportClient(ViewportClient);

	// 툴바 아이콘 로드
	LoadToolbarIcons(Device);

	return true;
}

void SViewportWindow::OnRender()
{
	// Slate(UI)만 처리하고 렌더는 FViewport에 위임
	RenderToolbar();

	if (Viewport)
		Viewport->Render();
}

void SViewportWindow::OnUpdate(float DeltaSeconds)
{
	if (!Viewport)
		return;

	if (!Viewport) return;

	// 툴바 높이만큼 뷰포트 영역 조정

	uint32 NewStartX = static_cast<uint32>(Rect.Left);
	uint32 NewStartY = static_cast<uint32>(Rect.Top);
	uint32 NewWidth = static_cast<uint32>(Rect.Right - Rect.Left);
	uint32 NewHeight = static_cast<uint32>(Rect.Bottom - Rect.Top);

	Viewport->Resize(NewStartX, NewStartY, NewWidth, NewHeight);
	ViewportClient->Tick(DeltaSeconds);
}

void SViewportWindow::OnMouseMove(FVector2D MousePos)
{
	if (!Viewport) return;

	// 툴바 영역 아래에서만 마우스 이벤트 처리
	FVector2D LocalPos = MousePos - FVector2D(Rect.Left, Rect.Top);
	Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);
}

void SViewportWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
	if (!Viewport) return;

	// 툴바 영역 아래에서만 마우스 이벤트 처리s
	bIsMouseDown = true;
	FVector2D LocalPos = MousePos - FVector2D(Rect.Left, Rect.Top);
	Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, Button);

}

void SViewportWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
	if (!Viewport) return;

	bIsMouseDown = false;
	FVector2D LocalPos = MousePos - FVector2D(Rect.Left, Rect.Top);
	Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, Button);
}

void SViewportWindow::SetVClientWorld(UWorld* InWorld)
{
	if (ViewportClient && InWorld)
	{
		ViewportClient->SetWorld(InWorld);
	}
}

void SViewportWindow::RenderToolbar()
{
	if (!Viewport) return;

	// 툴바 영역 크기
	float ToolbarHeight = 35.0f;
	ImVec2 ToolbarPosition(Rect.Left, Rect.Top);
	ImVec2 ToolbarSize(Rect.Right - Rect.Left, ToolbarHeight);

	// 툴바 위치 지정
	ImGui::SetNextWindowPos(ToolbarPosition);
	ImGui::SetNextWindowSize(ToolbarSize);

	// 뷰포트별 고유한 윈도우 ID
	char WindowId[64];
	sprintf_s(WindowId, "ViewportToolbar_%p", this);

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

	if (ImGui::Begin(WindowId, nullptr, WindowFlags))
	{
		// 기즈모 버튼 스타일 설정
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));      // 간격 좁히기
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);            // 모서리 둥글게
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));        // 배경 투명
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f)); // 호버 배경

		// 기즈모 모드 버튼들 렌더링
		RenderGizmoModeButtons();

		// 구분선
		ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "|");
		ImGui::SameLine();

		// 기즈모 스페이스 버튼 렌더링
		RenderGizmoSpaceButton();

		// 기즈모 버튼 스타일 복원
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(2);

		// 디버그 ShowFlag 토글 버튼들 (ViewMode와 독립적)
		if (ViewportClient && ViewportClient->GetWorld())
		{
			ImGui::SameLine();
			ImGui::Text("|"); // 구분선
			ImGui::SameLine();

			URenderSettings& RenderSettings = ViewportClient->GetWorld()->GetRenderSettings();

			// Tile Culling Debug
			bool bTileCullingDebug = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_TileCullingDebug);
			if (ImGui::Checkbox("TileCull", &bTileCullingDebug))
			{
				RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_TileCullingDebug);
			}

			ImGui::SameLine();

			// BVH Debug
			bool bBVHDebug = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_BVHDebug);
			if (ImGui::Checkbox("BVH", &bBVHDebug))
			{
				RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_BVHDebug);
			}

			ImGui::SameLine();

			// Grid
			bool bGrid = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_Grid);
			if (ImGui::Checkbox("Grid", &bGrid))
			{
				RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_Grid);
			}

			ImGui::SameLine();

			// Bounding Boxes
			bool bBoundingBoxes = RenderSettings.IsShowFlagEnabled(EEngineShowFlags::SF_BoundingBoxes);
			if (ImGui::Checkbox("Bounds", &bBoundingBoxes))
			{
				RenderSettings.ToggleShowFlag(EEngineShowFlags::SF_BoundingBoxes);
			}
		}

		// 카메라 옵션
		ImGui::SameLine();
		RenderCameraOptionDropdownMenu();

		// 뷰모드 드롭다운 메뉴
		ImGui::SameLine(0, 20.0f);
		RenderViewModeDropdownMenu();

		ImGui::SameLine(0, 20.0f);
		const ImVec2 ButtonSize(60, 30);
		if (ImGui::Button("Switch##ToThis", ButtonSize))
		{
			SLATE.SwitchPanel(this);
		}
	}
	ImGui::End();
}

void SViewportWindow::LoadToolbarIcons(ID3D11Device* Device)
{
	if (!Device) return;

	// 기즈모 아이콘 텍스처 생성 및 로드
	IconSelect = NewObject<UTexture>();
	IconSelect->Load("Data/Icon/Viewport_Toolbar_Select.png", Device);

	IconMove = NewObject<UTexture>();
	IconMove->Load("Data/Icon/Viewport_Toolbar_Move.png", Device);

	IconRotate = NewObject<UTexture>();
	IconRotate->Load("Data/Icon/Viewport_Toolbar_Rotate.png", Device);

	IconScale = NewObject<UTexture>();
	IconScale->Load("Data/Icon/Viewport_Toolbar_Scale.png", Device);

	IconWorldSpace = NewObject<UTexture>();
	IconWorldSpace->Load("Data/Icon/Viewport_Toolbar_WorldSpace.png", Device);

	IconLocalSpace = NewObject<UTexture>();
	IconLocalSpace->Load("Data/Icon/Viewport_Toolbar_LocalSpace.png", Device);

	// 뷰포트 모드 아이콘 텍스처 로드
	IconCamera = NewObject<UTexture>();
	IconCamera->Load("Data/Icon/Viewport_Mode_Camera.png", Device);

	IconPerspective = NewObject<UTexture>();
	IconPerspective->Load("Data/Icon/Viewport_Mode_Perspective.png", Device);

	IconTop = NewObject<UTexture>();
	IconTop->Load("Data/Icon/Viewport_Mode_Top.png", Device);

	IconBottom = NewObject<UTexture>();
	IconBottom->Load("Data/Icon/Viewport_Mode_Bottom.png", Device);

	IconLeft = NewObject<UTexture>();
	IconLeft->Load("Data/Icon/Viewport_Mode_Left.png", Device);

	IconRight = NewObject<UTexture>();
	IconRight->Load("Data/Icon/Viewport_Mode_Right.png", Device);

	IconFront = NewObject<UTexture>();
	IconFront->Load("Data/Icon/Viewport_Mode_Front.png", Device);

	IconBack = NewObject<UTexture>();
	IconBack->Load("Data/Icon/Viewport_Mode_Back.png", Device);

	// 뷰포트 설정 아이콘 텍스처 로드
	IconSpeed = NewObject<UTexture>();
	IconSpeed->Load("Data/Icon/Viewport_Mode_Camera.png", Device);

	IconFOV = NewObject<UTexture>();
	IconFOV->Load("Data/Icon/Viewport_Setting_FOV.png", Device);

	IconNearClip = NewObject<UTexture>();
	IconNearClip->Load("Data/Icon/Viewport_Setting_NearClip.png", Device);

	IconFarClip = NewObject<UTexture>();
	IconFarClip->Load("Data/Icon/Viewport_Setting_FarClip.png", Device);

	// 뷰모드 아이콘 텍스처 로드
	IconViewMode_Lit = NewObject<UTexture>();
	IconViewMode_Lit->Load("Data/Icon/Viewport_ViewMode_Lit.png", Device);

	IconViewMode_Unlit = NewObject<UTexture>();
	IconViewMode_Unlit->Load("Data/Icon/Viewport_ViewMode_Unlit.png", Device);

	IconViewMode_Wireframe = NewObject<UTexture>();
	IconViewMode_Wireframe->Load("Data/Icon/Viewport_Toolbar_WorldSpace.png", Device);

	IconViewMode_BufferVis = NewObject<UTexture>();
	IconViewMode_BufferVis->Load("Data/Icon/Viewport_ViewMode_BufferVis.png", Device);
}

void SViewportWindow::RenderGizmoModeButtons()
{
	const ImVec2 IconSize(14, 14);

	// GizmoActor에서 직접 현재 모드 가져오기
	EGizmoMode CurrentGizmoMode = EGizmoMode::Select;
	AGizmoActor* GizmoActor = nullptr;
	if (ViewportClient && ViewportClient->GetWorld())
	{
		GizmoActor = ViewportClient->GetWorld()->GetGizmoActor();
		if (GizmoActor)
		{
			CurrentGizmoMode = GizmoActor->GetMode();
		}
	}

	// Select 버튼
	bool bIsSelectActive = (CurrentGizmoMode == EGizmoMode::Select);
	ImVec4 SelectTintColor = bIsSelectActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconSelect && IconSelect->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##SelectBtn", (void*)IconSelect->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), SelectTintColor))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Select);
			}
		}
	}
	else
	{
		if (ImGui::Button("Select", ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Select);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("오브젝트를 선택합니다. [Q]");
	}
	ImGui::SameLine();

	// Move 버튼
	bool bIsMoveActive = (CurrentGizmoMode == EGizmoMode::Translate);
	ImVec4 MoveTintColor = bIsMoveActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconMove && IconMove->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##MoveBtn", (void*)IconMove->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), MoveTintColor))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Translate);
			}
		}
	}
	else
	{
		if (ImGui::Button("Move", ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Translate);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("오브젝트를 선택하고 이동시킵니다. [W]");
	}
	ImGui::SameLine();

	// Rotate 버튼
	bool bIsRotateActive = (CurrentGizmoMode == EGizmoMode::Rotate);
	ImVec4 RotateTintColor = bIsRotateActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconRotate && IconRotate->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##RotateBtn", (void*)IconRotate->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), RotateTintColor))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Rotate);
			}
		}
	}
	else
	{
		if (ImGui::Button("Rotate", ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Rotate);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("오브젝트를 선택하고 회전시킵니다. [E]");
	}
	ImGui::SameLine();

	// Scale 버튼
	bool bIsScaleActive = (CurrentGizmoMode == EGizmoMode::Scale);
	ImVec4 ScaleTintColor = bIsScaleActive ? ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(1, 1, 1, 1);

	if (IconScale && IconScale->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##ScaleBtn", (void*)IconScale->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), ScaleTintColor))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Scale);
			}
		}
	}
	else
	{
		if (ImGui::Button("Scale", ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				GizmoActor->SetMode(EGizmoMode::Scale);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("오브젝트를 선택하고 스케일을 조절합니다. [R]");
	}

	ImGui::SameLine();
}

void SViewportWindow::RenderGizmoSpaceButton()
{
	const ImVec2 IconSize(14, 14);

	// GizmoActor에서 직접 현재 스페이스 가져오기
	EGizmoSpace CurrentGizmoSpace = EGizmoSpace::World;
	AGizmoActor* GizmoActor = nullptr;
	if (ViewportClient && ViewportClient->GetWorld())
	{
		GizmoActor = ViewportClient->GetWorld()->GetGizmoActor();
		if (GizmoActor)
		{
			CurrentGizmoSpace = GizmoActor->GetSpace();
		}
	}

	// 현재 스페이스에 따라 적절한 아이콘 표시
	bool bIsWorldSpace = (CurrentGizmoSpace == EGizmoSpace::World);
	UTexture* CurrentIcon = bIsWorldSpace ? IconWorldSpace : IconLocalSpace;
	const char* TooltipText = bIsWorldSpace ? "월드 스페이스 좌표 [Tab]" : "로컬 스페이스 좌표 [Tab]";

	// 선택 상태 tint (월드/로컬 모두 동일하게 흰색)
	ImVec4 TintColor = ImVec4(1, 1, 1, 1);

	if (CurrentIcon && CurrentIcon->GetShaderResourceView())
	{
		if (ImGui::ImageButton("##GizmoSpaceBtn", (void*)CurrentIcon->GetShaderResourceView(), IconSize,
			ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), TintColor))
		{
			// 버튼 클릭 시 스페이스 전환
			if (GizmoActor)
			{
				EGizmoSpace NewSpace = bIsWorldSpace ? EGizmoSpace::Local : EGizmoSpace::World;
				GizmoActor->SetSpace(NewSpace);
			}
		}
	}
	else
	{
		// 아이콘이 없는 경우 텍스트 버튼
		const char* ButtonText = bIsWorldSpace ? "World" : "Local";
		if (ImGui::Button(ButtonText, ImVec2(60, 0)))
		{
			if (GizmoActor)
			{
				EGizmoSpace NewSpace = bIsWorldSpace ? EGizmoSpace::Local : EGizmoSpace::World;
				GizmoActor->SetSpace(NewSpace);
			}
		}
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", TooltipText);
	}

	ImGui::SameLine();
}

void SViewportWindow::RenderCameraOptionDropdownMenu()
{
	ImVec2 cursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(cursorPos.y - 2.0f);

	// 오른쪽 정렬을 위한 여유 공간 계산
	const float RightMargin = 200.0f; // 오른쪽에서 얼마나 떨어진 위치에 배치할지 (뷰모드 + Switch 버튼 공간)
	float AvailableWidth = ImGui::GetContentRegionAvail().x; // 현재 남은 가로 공간

	const ImVec2 IconSize(17, 17);

	// 드롭다운 버튼 텍스트 준비
	char ButtonText[64];
	sprintf_s(ButtonText, "%s %s", ViewportName.ToString().c_str(), "▼");

	// 버튼 너비 계산 (아이콘 크기 + 간격 + 텍스트 크기 + 좌우 패딩)
	ImVec2 TextSize = ImGui::CalcTextSize(ButtonText);
	const float HorizontalPadding = 8.0f;
	const float CameraDropdownWidth = IconSize.x + 4.0f + TextSize.x + HorizontalPadding * 2.0f;

	// 오른쪽 정렬: 남은 공간이 (버튼 너비 + 오른쪽 여백)보다 크면 X 위치 조정
	if (AvailableWidth > (CameraDropdownWidth + RightMargin))
	{
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (AvailableWidth - CameraDropdownWidth - RightMargin));
	}

	// 드롭다운 버튼 스타일 적용
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 0.6f));

	// 드롭다운 버튼 생성 (카메라 아이콘 + 현재 모드명 + 화살표)
	ImVec2 ButtonSize(CameraDropdownWidth, ImGui::GetFrameHeight());
	ImVec2 ButtonCursorPos = ImGui::GetCursorPos();

	// 버튼 클릭 영역
	if (ImGui::Button("##ViewportModeBtn", ButtonSize))
	{
		ImGui::OpenPopup("ViewportModePopup");
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("카메라 옵션");
	}

	// 버튼 위에 내용 렌더링 (아이콘 + 텍스트, 가운데 정렬)
	float ButtonContentWidth = IconSize.x + 4.0f + TextSize.x;
	float ButtonContentStartX = ButtonCursorPos.x + (ButtonSize.x - ButtonContentWidth) * 0.5f;
	ImVec2 ButtonContentCursorPos = ImVec2(ButtonContentStartX, ButtonCursorPos.y + (ButtonSize.y - IconSize.y) * 0.5f);
	ImGui::SetCursorPos(ButtonContentCursorPos);

	// 현재 뷰포트 모드에 따라 아이콘 선택
	UTexture* CurrentModeIcon = nullptr;
	switch (ViewportType)
	{
	case EViewportType::Perspective:
		CurrentModeIcon = IconCamera;
		break;
	case EViewportType::Orthographic_Top:
		CurrentModeIcon = IconTop;
		break;
	case EViewportType::Orthographic_Bottom:
		CurrentModeIcon = IconBottom;
		break;
	case EViewportType::Orthographic_Left:
		CurrentModeIcon = IconLeft;
		break;
	case EViewportType::Orthographic_Right:
		CurrentModeIcon = IconRight;
		break;
	case EViewportType::Orthographic_Front:
		CurrentModeIcon = IconFront;
		break;
	case EViewportType::Orthographic_Back:
		CurrentModeIcon = IconBack;
		break;
	default:
		CurrentModeIcon = IconCamera;
		break;
	}

	if (CurrentModeIcon && CurrentModeIcon->GetShaderResourceView())
	{
		ImGui::Image((void*)CurrentModeIcon->GetShaderResourceView(), IconSize);
		ImGui::SameLine(0, 4);
	}

	ImGui::Text("%s", ButtonText);

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(1);

	// ===== 뷰포트 모드 드롭다운 팝업 =====
	if (ImGui::BeginPopup("ViewportModePopup", ImGuiWindowFlags_NoMove))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

		// 선택된 항목의 파란 배경 제거
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 0.6f));

		// --- 섹션 1: 원근 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "원근");
		ImGui::Separator();

		bool bIsPerspective = (ViewportType == EViewportType::Perspective);
		const char* RadioIcon = bIsPerspective ? "●" : "○";

		// 원근 모드 선택 항목 (라디오 버튼 + 아이콘 + 텍스트 통합)
		ImVec2 SelectableSize(180, 20);
		ImVec2 SelectableCursorPos = ImGui::GetCursorPos();

		if (ImGui::Selectable("##Perspective", bIsPerspective, 0, SelectableSize))
		{
			ViewportType = EViewportType::Perspective;
			ViewportName = "원근";
			if (ViewportClient)
			{
				ViewportClient->SetViewportType(ViewportType);
				ViewportClient->SetupCameraMode();
			}
			ImGui::CloseCurrentPopup();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("뷰포트를 원근 보기로 전환합니다.");
		}

		// Selectable 위에 내용 렌더링
		ImVec2 ContentPos = ImVec2(SelectableCursorPos.x + 4, SelectableCursorPos.y + (SelectableSize.y - IconSize.y) * 0.5f);
		ImGui::SetCursorPos(ContentPos);

		ImGui::Text("%s", RadioIcon);
		ImGui::SameLine(0, 4);

		if (IconPerspective && IconPerspective->GetShaderResourceView())
		{
			ImGui::Image((void*)IconPerspective->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		ImGui::Text("원근");

		// --- 섹션 2: 직교 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "직교");
		ImGui::Separator();

		// 직교 모드 목록
		struct ViewportModeEntry {
			EViewportType type;
			const char* koreanName;
			UTexture** icon;
			const char* tooltip;
		};

		ViewportModeEntry orthographicModes[] = {
			{ EViewportType::Orthographic_Top, "상단", &IconTop, "뷰포트를 상단 보기로 전환합니다." },
			{ EViewportType::Orthographic_Bottom, "하단", &IconBottom, "뷰포트를 하단 보기로 전환합니다." },
			{ EViewportType::Orthographic_Left, "왼쪽", &IconLeft, "뷰포트를 왼쪽 보기로 전환합니다." },
			{ EViewportType::Orthographic_Right, "오른쪽", &IconRight, "뷰포트를 오른쪽 보기로 전환합니다." },
			{ EViewportType::Orthographic_Front, "정면", &IconFront, "뷰포트를 정면 보기로 전환합니다." },
			{ EViewportType::Orthographic_Back, "후면", &IconBack, "뷰포트를 후면 보기로 전환합니다." }
		};

		for (int i = 0; i < 6; i++)
		{
			const auto& mode = orthographicModes[i];
			bool bIsSelected = (ViewportType == mode.type);
			const char* RadioIcon = bIsSelected ? "●" : "○";

			// 직교 모드 선택 항목 (라디오 버튼 + 아이콘 + 텍스트 통합)
			char SelectableID[32];
			sprintf_s(SelectableID, "##Ortho%d", i);

			ImVec2 OrthoSelectableCursorPos = ImGui::GetCursorPos();

			if (ImGui::Selectable(SelectableID, bIsSelected, 0, SelectableSize))
			{
				ViewportType = mode.type;
				ViewportName = mode.koreanName;
				if (ViewportClient)
				{
					ViewportClient->SetViewportType(ViewportType);
					ViewportClient->SetupCameraMode();
				}
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", mode.tooltip);
			}

			// Selectable 위에 내용 렌더링
			ImVec2 OrthoContentPos = ImVec2(OrthoSelectableCursorPos.x + 4, OrthoSelectableCursorPos.y + (SelectableSize.y - IconSize.y) * 0.5f);
			ImGui::SetCursorPos(OrthoContentPos);

			ImGui::Text("%s", RadioIcon);
			ImGui::SameLine(0, 4);

			if (*mode.icon && (*mode.icon)->GetShaderResourceView())
			{
				ImGui::Image((void*)(*mode.icon)->GetShaderResourceView(), IconSize);
				ImGui::SameLine(0, 4);
			}

			ImGui::Text("%s", mode.koreanName);
		}

		// --- 섹션 3: 이동 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "이동");
		ImGui::Separator();

		ACameraActor* Camera = ViewportClient ? ViewportClient->GetCamera() : nullptr;
		if (Camera)
		{
			if (IconSpeed && IconSpeed->GetShaderResourceView())
			{
				ImGui::Image((void*)IconSpeed->GetShaderResourceView(), IconSize);
				ImGui::SameLine();
			}
			ImGui::Text("카메라 이동 속도");

			float speed = Camera->GetCameraSpeed();
			ImGui::SetNextItemWidth(180);
			if (ImGui::SliderFloat("##CameraSpeed", &speed, 1.0f, 100.0f, "%.1f"))
			{
				Camera->SetCameraSpeed(speed);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("WASD 키로 카메라를 이동할 때의 속도 (1-100)");
			}
		}

		// --- 섹션 4: 뷰 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "뷰");
		ImGui::Separator();

		if (Camera && Camera->GetCameraComponent())
		{
			UCameraComponent* camComp = Camera->GetCameraComponent();

			// FOV
			if (IconFOV && IconFOV->GetShaderResourceView())
			{
				ImGui::Image((void*)IconFOV->GetShaderResourceView(), IconSize);
				ImGui::SameLine();
			}
			ImGui::Text("필드 오브 뷰");

			float fov = camComp->GetFOV();
			ImGui::SetNextItemWidth(180);
			if (ImGui::SliderFloat("##FOV", &fov, 30.0f, 120.0f, "%.1f"))
			{
				camComp->SetFOV(fov);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("카메라 시야각 (30-120도)\n값이 클수록 넓은 범위가 보입니다");
			}

			// 근평면
			if (IconNearClip && IconNearClip->GetShaderResourceView())
			{
				ImGui::Image((void*)IconNearClip->GetShaderResourceView(), IconSize);
				ImGui::SameLine();
			}
			ImGui::Text("근평면");

			float nearClip = camComp->GetNearClip();
			ImGui::SetNextItemWidth(180);
			if (ImGui::SliderFloat("##NearClip", &nearClip, 0.01f, 10.0f, "%.2f"))
			{
				camComp->SetClipPlanes(nearClip, camComp->GetFarClip());
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("카메라에서 가장 가까운 렌더링 거리 (0.01-10)\n이 값보다 가까운 오브젝트는 보이지 않습니다");
			}

			// 원평면
			if (IconFarClip && IconFarClip->GetShaderResourceView())
			{
				ImGui::Image((void*)IconFarClip->GetShaderResourceView(), IconSize);
				ImGui::SameLine();
			}
			ImGui::Text("원평면");

			float farClip = camComp->GetFarClip();
			ImGui::SetNextItemWidth(180);
			if (ImGui::SliderFloat("##FarClip", &farClip, 100.0f, 10000.0f, "%.0f"))
			{
				camComp->SetClipPlanes(camComp->GetNearClip(), farClip);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("카메라에서 가장 먼 렌더링 거리 (100-10000)\n이 값보다 먼 오브젝트는 보이지 않습니다");
			}
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
		ImGui::EndPopup();
	}
}

void SViewportWindow::RenderViewModeDropdownMenu()
{
	if (!ViewportClient) return;

	ImVec2 cursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPosY(cursorPos.y - 3.5f);

	const ImVec2 IconSize(17, 17);

	// 현재 뷰모드 이름 및 아이콘 가져오기
	EViewModeIndex CurrentViewMode = ViewportClient->GetViewModeIndex();
	const char* CurrentViewModeName = "뷰모드";
	UTexture* CurrentViewModeIcon = nullptr;

	switch (CurrentViewMode)
	{
	case EViewModeIndex::VMI_Lit:
	case EViewModeIndex::VMI_Lit_Gouraud:
	case EViewModeIndex::VMI_Lit_Lambert:
	case EViewModeIndex::VMI_Lit_Phong:
		CurrentViewModeName = "라이팅 포함";
		CurrentViewModeIcon = IconViewMode_Lit;
		break;
	case EViewModeIndex::VMI_Unlit:
		CurrentViewModeName = "언릿";
		CurrentViewModeIcon = IconViewMode_Unlit;
		break;
	case EViewModeIndex::VMI_Wireframe:
		CurrentViewModeName = "와이어프레임";
		CurrentViewModeIcon = IconViewMode_Wireframe;
		break;
	case EViewModeIndex::VMI_WorldNormal:
		CurrentViewModeName = "월드 노멀";
		CurrentViewModeIcon = IconViewMode_BufferVis;
		break;
	case EViewModeIndex::VMI_SceneDepth:
		CurrentViewModeName = "씬 뎁스";
		CurrentViewModeIcon = IconViewMode_BufferVis;
		break;
	}

	// 드롭다운 버튼 텍스트 준비
	char ButtonText[64];
	sprintf_s(ButtonText, "%s %s", CurrentViewModeName, "▼");

	// 버튼 너비 계산 (아이콘 크기 + 간격 + 텍스트 크기 + 좌우 패딩)
	ImVec2 TextSize = ImGui::CalcTextSize(ButtonText);
	const float Padding = 8.0f;
	const float DropdownWidth = IconSize.x + 4.0f + TextSize.x + Padding * 2.0f;

	// 스타일 적용
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 0.6f));

	// 드롭다운 버튼 생성 (아이콘 + 텍스트)
	ImVec2 ButtonSize(DropdownWidth, ImGui::GetFrameHeight());
	ImVec2 ButtonCursorPos = ImGui::GetCursorPos();

	// 버튼 클릭 영역
	if (ImGui::Button("##ViewModeBtn", ButtonSize))
	{
		ImGui::OpenPopup("ViewModePopup");
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("뷰모드 선택");
	}

	// 버튼 위에 내용 렌더링 (아이콘 + 텍스트, 가운데 정렬)
	float ButtonContentWidth = IconSize.x + 4.0f + TextSize.x;
	float ButtonContentStartX = ButtonCursorPos.x + (ButtonSize.x - ButtonContentWidth) * 0.5f;
	ImVec2 ButtonContentCursorPos = ImVec2(ButtonContentStartX, ButtonCursorPos.y + (ButtonSize.y - IconSize.y) * 0.5f);
	ImGui::SetCursorPos(ButtonContentCursorPos);

	// 현재 뷰모드 아이콘 표시
	if (CurrentViewModeIcon && CurrentViewModeIcon->GetShaderResourceView())
	{
		ImGui::Image((void*)CurrentViewModeIcon->GetShaderResourceView(), IconSize);
		ImGui::SameLine(0, 4);
	}

	ImGui::Text("%s", ButtonText);

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(1);

	// ===== 뷰모드 드롭다운 팝업 =====
	if (ImGui::BeginPopup("ViewModePopup", ImGuiWindowFlags_NoMove))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

		// 선택된 항목의 파란 배경 제거
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.3f, 0.6f));

		// --- 섹션: 뷰모드 ---
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "뷰모드");
		ImGui::Separator();

		// ===== Lit 메뉴 (서브메뉴 포함) =====
		bool bIsLitMode = (CurrentViewMode == EViewModeIndex::VMI_Lit ||
			CurrentViewMode == EViewModeIndex::VMI_Lit_Gouraud ||
			CurrentViewMode == EViewModeIndex::VMI_Lit_Lambert ||
			CurrentViewMode == EViewModeIndex::VMI_Lit_Phong);

		const char* LitRadioIcon = bIsLitMode ? "●" : "○";

		// Lit 아이콘 + 텍스트 + 화살표
		if (IconViewMode_Lit && IconViewMode_Lit->GetShaderResourceView())
		{
			ImGui::Image((void*)IconViewMode_Lit->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		ImGui::Text("%s", LitRadioIcon);
		ImGui::SameLine(0, 4);

		if (ImGui::BeginMenu("Lit (릿)"))
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "셰이더 모델");
			ImGui::Separator();

			// PHONG
			bool bIsPhong = (CurrentViewMode == EViewModeIndex::VMI_Lit || CurrentViewMode == EViewModeIndex::VMI_Lit_Phong);
			const char* PhongIcon = bIsPhong ? "●" : "○";
			char PhongLabel[32];
			sprintf_s(PhongLabel, "%s PHONG", PhongIcon);
			if (ImGui::MenuItem(PhongLabel))
			{
				ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Phong);
				CurrentLitSubMode = 3;
				ImGui::CloseCurrentPopup();
			}

			// GOURAUD
			bool bIsGouraud = (CurrentViewMode == EViewModeIndex::VMI_Lit_Gouraud);
			const char* GouraudIcon = bIsGouraud ? "●" : "○";
			char GouraudLabel[32];
			sprintf_s(GouraudLabel, "%s GOURAUD", GouraudIcon);
			if (ImGui::MenuItem(GouraudLabel))
			{
				ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Gouraud);
				CurrentLitSubMode = 1;
				ImGui::CloseCurrentPopup();
			}

			// LAMBERT
			bool bIsLambert = (CurrentViewMode == EViewModeIndex::VMI_Lit_Lambert);
			const char* LambertIcon = bIsLambert ? "●" : "○";
			char LambertLabel[32];
			sprintf_s(LambertLabel, "%s LAMBERT", LambertIcon);
			if (ImGui::MenuItem(LambertLabel))
			{
				ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Lambert);
				CurrentLitSubMode = 2;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndMenu();
		}

		// ===== Unlit 메뉴 =====
		bool bIsUnlit = (CurrentViewMode == EViewModeIndex::VMI_Unlit);
		const char* UnlitRadioIcon = bIsUnlit ? "●" : "○";

		if (IconViewMode_Unlit && IconViewMode_Unlit->GetShaderResourceView())
		{
			ImGui::Image((void*)IconViewMode_Unlit->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		ImGui::Text("%s", UnlitRadioIcon);
		ImGui::SameLine(0, 4);

		if (ImGui::MenuItem("Unlit (언릿)"))
		{
			ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Unlit);
			ImGui::CloseCurrentPopup();
		}

		// ===== Wireframe 메뉴 =====
		bool bIsWireframe = (CurrentViewMode == EViewModeIndex::VMI_Wireframe);
		const char* WireframeRadioIcon = bIsWireframe ? "●" : "○";

		if (IconViewMode_Wireframe && IconViewMode_Wireframe->GetShaderResourceView())
		{
			ImGui::Image((void*)IconViewMode_Wireframe->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		ImGui::Text("%s", WireframeRadioIcon);
		ImGui::SameLine(0, 4);

		if (ImGui::MenuItem("Wireframe (와이어프레임)"))
		{
			ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Wireframe);
			ImGui::CloseCurrentPopup();
		}

		// ===== Buffer Visualization 메뉴 (서브메뉴 포함) =====
		bool bIsBufferVis = (CurrentViewMode == EViewModeIndex::VMI_WorldNormal ||
			CurrentViewMode == EViewModeIndex::VMI_SceneDepth);

		const char* BufferVisRadioIcon = bIsBufferVis ? "●" : "○";

		if (IconViewMode_BufferVis && IconViewMode_BufferVis->GetShaderResourceView())
		{
			ImGui::Image((void*)IconViewMode_BufferVis->GetShaderResourceView(), IconSize);
			ImGui::SameLine(0, 4);
		}

		ImGui::Text("%s", BufferVisRadioIcon);
		ImGui::SameLine(0, 4);

		if (ImGui::BeginMenu("Buffer Visualization (버퍼 시각화)"))
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "버퍼 시각화");
			ImGui::Separator();

			// Scene Depth
			bool bIsSceneDepth = (CurrentViewMode == EViewModeIndex::VMI_SceneDepth);
			const char* SceneDepthIcon = bIsSceneDepth ? "●" : "○";
			char SceneDepthLabel[32];
			sprintf_s(SceneDepthLabel, "%s Scene Depth", SceneDepthIcon);
			if (ImGui::MenuItem(SceneDepthLabel))
			{
				ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_SceneDepth);
				CurrentBufferVisSubMode = 0;
				ImGui::CloseCurrentPopup();
			}

			// World Normal
			bool bIsWorldNormal = (CurrentViewMode == EViewModeIndex::VMI_WorldNormal);
			const char* WorldNormalIcon = bIsWorldNormal ? "●" : "○";
			char WorldNormalLabel[32];
			sprintf_s(WorldNormalLabel, "%s World Normal", WorldNormalIcon);
			if (ImGui::MenuItem(WorldNormalLabel))
			{
				ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_WorldNormal);
				CurrentBufferVisSubMode = 1;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndMenu();
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
		ImGui::EndPopup();
	}
}