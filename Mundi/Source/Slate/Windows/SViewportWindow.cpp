#include "pch.h"
#include "SViewportWindow.h"
#include "World.h"
#include "ImGui/imgui.h"
#include"USlateManager.h"

#include "FViewport.h"
#include "FViewportClient.h"
#include "Texture.h"
#include "Gizmo/GizmoActor.h"

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
}

bool SViewportWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* World, ID3D11Device* Device, EViewportType InViewportType)
{
	ViewportType = InViewportType;

	// 이름 설정
	switch (ViewportType)
	{
	case EViewportType::Perspective:       ViewportName = "Perspective"; break;
	case EViewportType::Orthographic_Front: ViewportName = "Front"; break;
	case EViewportType::Orthographic_Left:  ViewportName = "Left"; break;
	case EViewportType::Orthographic_Top:   ViewportName = "Top"; break;
	case EViewportType::Orthographic_Back: ViewportName = "Back"; break;
	case EViewportType::Orthographic_Right:  ViewportName = "Right"; break;
	case EViewportType::Orthographic_Bottom:   ViewportName = "Bottom"; break;
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
	float ToolbarHeight = 30.0f;
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
		// 기즈모 모드 버튼들 렌더링
		RenderGizmoModeButtons();

		// 1단계: 메인 ViewMode 선택 (Lit, Unlit, Buffer Visualization, Wireframe)
		const char* MainViewModes[] = { "Lit", "Unlit", "Buffer Visualization", "Wireframe" };

		// 현재 ViewMode에서 메인 모드 인덱스 계산
		int CurrentMainMode = 0; // 기본값: Lit
		EViewModeIndex CurrentViewMode = ViewportClient->GetViewModeIndex();
		if (CurrentViewMode == EViewModeIndex::VMI_Unlit)
		{
			CurrentMainMode = 1;
		}
		else if (CurrentViewMode == EViewModeIndex::VMI_WorldNormal || CurrentViewMode == EViewModeIndex::VMI_SceneDepth)
		{
			CurrentMainMode = 2; // Buffer Visualization
			// 현재 BufferVis 서브모드도 동기화
			if (CurrentViewMode == EViewModeIndex::VMI_SceneDepth)
				CurrentBufferVisSubMode = 0;
			else if (CurrentViewMode == EViewModeIndex::VMI_WorldNormal)
				CurrentBufferVisSubMode = 1;
		}
		else if (CurrentViewMode == EViewModeIndex::VMI_Wireframe)
		{
			CurrentMainMode = 3;
		}
		else if (CurrentViewMode == EViewModeIndex::VMI_SceneDepth)
		{
			CurrentMainMode = 4;
		}
		else // Lit 계열 (Gouraud, Lambert, Phong)
		{
			CurrentMainMode = 0;
			// 현재 Lit 서브모드도 동기화
			if (CurrentViewMode == EViewModeIndex::VMI_Lit)
				CurrentLitSubMode = 0;
			else if (CurrentViewMode == EViewModeIndex::VMI_Lit_Gouraud)
				CurrentLitSubMode = 1;
			else if (CurrentViewMode == EViewModeIndex::VMI_Lit_Lambert)
				CurrentLitSubMode = 2;
			else if (CurrentViewMode == EViewModeIndex::VMI_Lit_Phong)
				CurrentLitSubMode = 3;
		}

		ImGui::SameLine();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 0));
		ImGui::SetNextItemWidth(80.0f);
		bool MainModeChanged = ImGui::Combo("##MainViewMode", &CurrentMainMode, MainViewModes, IM_ARRAYSIZE(MainViewModes));

		// 2단계: Lit 서브모드 선택 (Lit 선택 시에만 표시)
		if (CurrentMainMode == 0) // Lit 선택됨
		{
			ImGui::SameLine();
			const char* LitSubModes[] = { "Default(Phong)", "Gouraud", "Lambert", "Phong" };
			ImGui::SetNextItemWidth(80.0f);
			bool SubModeChanged = ImGui::Combo("##LitSubMode", &CurrentLitSubMode, LitSubModes, IM_ARRAYSIZE(LitSubModes));

			if (SubModeChanged && ViewportClient)
			{
				switch (CurrentLitSubMode)
				{
				case 0: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit); break;
				case 1: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Gouraud); break;
				case 2: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Lambert); break;
				case 3: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Phong); break;
				}
			}
		}

		// 2단계: Buffer Visualization 서브모드 선택 (Buffer Visualization 선택 시에만 표시)
		if (CurrentMainMode == 2) // Buffer Visualization 선택됨
		{
			ImGui::SameLine();
			const char* bufferVisSubModes[] = { "SceneDepth", "WorldNormal" };
			ImGui::SetNextItemWidth(100.0f);
			bool subModeChanged = ImGui::Combo("##BufferVisSubMode", &CurrentBufferVisSubMode, bufferVisSubModes, IM_ARRAYSIZE(bufferVisSubModes));

			if (subModeChanged && ViewportClient)
			{
				switch (CurrentBufferVisSubMode)
				{
				case 0: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_SceneDepth); break;
				case 1: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_WorldNormal); break;
				}
			}
		}

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

		// 메인 모드 변경 시 처리
		if (MainModeChanged && ViewportClient)
		{
			switch (CurrentMainMode)
			{
			case 0: // Lit - 현재 선택된 서브모드 적용
				switch (CurrentLitSubMode)
				{
				case 0: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit); break;
				case 1: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Gouraud); break;
				case 2: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Lambert); break;
				case 3: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Phong); break;
				}
				break;
			case 1: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Unlit); break;
			case 2: // Buffer Visualization - 현재 선택된 서브모드 적용
				switch (CurrentBufferVisSubMode)
				{
				case 0: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_SceneDepth); break;
				case 1: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_WorldNormal); break;
				}
				break;
			case 3: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Wireframe); break;
			}
		}
		// 🔘 여기 '한 번 클릭' 버튼 추가
		const float btnW = 60.0f;
		const ImVec2 btnSize(btnW, 0.0f);

		ImGui::SameLine();
		float avail = ImGui::GetContentRegionAvail().x;      // 현재 라인에서 남은 가로폭
		// 뷰포트 모드 선택 콤보박스 너비도 고려 (100px)
		const float comboW = 100.0f;
		if (avail > (btnW + comboW + 10.0f)) // 10은 여백
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - btnW - comboW - 10.0f));
		}

		// 뷰포트 모드 선택 콤보박스
		static const char* const viewportModes[] = {
			"Perspective",
			"Top",
			"Bottom",
			"Front",
			"Left",
			"Right",
			"Back"
		};

		int currentMode = static_cast<int>(ViewportType);
		ImGui::SetNextItemWidth(comboW);
		if (ImGui::Combo("##ViewportMode", &currentMode, viewportModes, (int)IM_ARRAYSIZE(viewportModes)))
		{
			EViewportType newType = static_cast<EViewportType>(currentMode);
			if (newType != ViewportType)
			{
				ViewportType = newType;

				// ViewportClient 업데이트
				if (ViewportClient)
				{
					ViewportClient->SetViewportType(ViewportType);
					ViewportClient->SetupCameraMode();
				}

				// 뷰포트 이름 업데이트
				switch (ViewportType)
				{
				case EViewportType::Perspective:       ViewportName = "Perspective"; break;
				case EViewportType::Orthographic_Front: ViewportName = "Front"; break;
				case EViewportType::Orthographic_Left:  ViewportName = "Left"; break;
				case EViewportType::Orthographic_Top:   ViewportName = "Top"; break;
				case EViewportType::Orthographic_Back: ViewportName = "Back"; break;
				case EViewportType::Orthographic_Right:  ViewportName = "Right"; break;
				case EViewportType::Orthographic_Bottom:   ViewportName = "Bottom"; break;
				}
			}
		}

		ImGui::SameLine();

		if (ImGui::Button("Switch##ToThis", btnSize))
		{
			SLATE.SwitchPanel(this);
		}

		//ImGui::PopStyleVar();

	}
	ImGui::End();
}

void SViewportWindow::LoadToolbarIcons(ID3D11Device* Device)
{
	if (!Device) return;

	// 아이콘 텍스처 생성 및 로드
	IconSelect = NewObject<UTexture>();
	IconSelect->Load("Data/Icon/Viewport_Toolbar_Select.png", Device);

	IconMove = NewObject<UTexture>();
	IconMove->Load("Data/Icon/Viewport_Toolbar_Move.png", Device);

	IconRotate = NewObject<UTexture>();
	IconRotate->Load("Data/Icon/Viewport_Toolbar_Rotate.png", Device);

	IconScale = NewObject<UTexture>();
	IconScale->Load("Data/Icon/Viewport_Toolbar_Scale.png", Device);
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

	// 버튼 간격 좁히기
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));

	// 모서리 둥글게
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

	// 배경 투명하게
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));

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

	// 배경 스타일, 버튼 간격 및 모서리 둥글기 스타일 복원
	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar(2);
	ImGui::SameLine();
}