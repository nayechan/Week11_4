#include "pch.h"
#include "SViewportWindow.h"
#include "World.h"
#include "ImGui/imgui.h"
#include"USlateManager.h"

#include "FViewport.h"
#include "FViewportClient.h"

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
	uint32 NewStartY = static_cast<uint32>(Rect.Top );
	uint32 NewWidth = static_cast<uint32>(Rect.Right - Rect.Left);
	uint32 NewHeight = static_cast<uint32>(Rect.Bottom - Rect.Top );

	Viewport->Resize(NewStartX, NewStartY, NewWidth, NewHeight);
	ViewportClient->Tick(DeltaSeconds);
}

void SViewportWindow::OnMouseMove(FVector2D MousePos)
{
	if (!Viewport) return;

	// 툴바 영역 아래에서만 마우스 이벤트 처리
	FVector2D LocalPos = MousePos - FVector2D(Rect.Left, Rect.Top );
	Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);
}

void SViewportWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
	if (!Viewport) return;

	// 툴바 영역 아래에서만 마우스 이벤트 처리s
		bIsMouseDown = true;
		FVector2D LocalPos = MousePos - FVector2D(Rect.Left, Rect.Top );
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
	float toolbarHeight = 30.0f;
	ImVec2 toolbarPos(Rect.Left, Rect.Top);
	ImVec2 toolbarSize(Rect.Right - Rect.Left, toolbarHeight);

	// 툴바 위치 지정
	ImGui::SetNextWindowPos(toolbarPos);
	ImGui::SetNextWindowSize(toolbarSize);

	// 뷰포트별 고유한 윈도우 ID
	char windowId[64];
	sprintf_s(windowId, "ViewportToolbar_%p", this);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

	if (ImGui::Begin(windowId, nullptr, flags))
	{
		// 뷰포트 모드 선택 콤보박스
		const char* viewportModes[] = {
			"Perspective",
			"Top",
			"Bottom",
			"Front",
			"Left",
			"Right",
			"Back"
		};

		int currentMode = static_cast<int>(ViewportType);
		ImGui::SetNextItemWidth(100);
		if (ImGui::Combo("##ViewportMode", &currentMode, viewportModes, IM_ARRAYSIZE(viewportModes)))
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

		// 뷰포트 이름 표시
		ImGui::Text("%s", ViewportName.ToString().c_str());
		ImGui::SameLine();

		// 버튼들
		if (ImGui::Button("Move")) { /* TODO: 이동 모드 전환 */ }
		ImGui::SameLine();

		if (ImGui::Button("Rotate")) { /* TODO: 회전 모드 전환 */ }
		ImGui::SameLine();

		if (ImGui::Button("Scale")) { /* TODO: 스케일 모드 전환 */ }
		ImGui::SameLine();

		if (ImGui::Button("Reset")) { /* TODO: 카메라 Reset */ }

		// 1단계: 메인 ViewMode 선택 (Lit, Unlit, Wireframe, SceneDepth)
		const char* mainViewModes[] = { "Lit", "Unlit", "Wireframe", "SceneDepth" };

		// 현재 ViewMode에서 메인 모드 인덱스 계산
		int currentMainMode = 0; // 기본값: Lit
		EViewModeIndex currentViewMode = ViewportClient->GetViewModeIndex();
		if (currentViewMode == EViewModeIndex::VMI_Unlit)
			currentMainMode = 1;
		else if (currentViewMode == EViewModeIndex::VMI_Wireframe)
			currentMainMode = 2;
		else if (currentViewMode == EViewModeIndex::VMI_SceneDepth)
			currentMainMode = 3;
		else // Lit 계열 (Gouraud, Lambert, Phong)
		{
			currentMainMode = 0;
			// 현재 Lit 서브모드도 동기화
			if (currentViewMode == EViewModeIndex::VMI_Lit_Gouraud)
				CurrentLitSubMode = 0;
			else if (currentViewMode == EViewModeIndex::VMI_Lit_Lambert)
				CurrentLitSubMode = 1;
			else if (currentViewMode == EViewModeIndex::VMI_Lit_Phong)
				CurrentLitSubMode = 2;
		}

		ImGui::SameLine();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 0));
		ImGui::SetNextItemWidth(80.0f);
		bool mainModeChanged = ImGui::Combo("##MainViewMode", &currentMainMode, mainViewModes, IM_ARRAYSIZE(mainViewModes));

		// 2단계: Lit 서브모드 선택 (Lit 선택 시에만 표시)
		if (currentMainMode == 0) // Lit 선택됨
		{
			ImGui::SameLine();
			const char* litSubModes[] = { "Gouraud", "Lambert", "Phong" };
			ImGui::SetNextItemWidth(80.0f);
			bool subModeChanged = ImGui::Combo("##LitSubMode", &CurrentLitSubMode, litSubModes, IM_ARRAYSIZE(litSubModes));

			if (subModeChanged && ViewportClient)
			{
				switch (CurrentLitSubMode)
				{
				case 0: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Gouraud); break;
				case 1: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Lambert); break;
				case 2: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Phong); break;
				}
			}
		}

		ImGui::PopStyleVar(2);

		// 메인 모드 변경 시 처리
		if (mainModeChanged && ViewportClient)
		{
			switch (currentMainMode)
			{
			case 0: // Lit - 현재 선택된 서브모드 적용
				switch (CurrentLitSubMode)
				{
				case 0: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Gouraud); break;
				case 1: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Lambert); break;
				case 2: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Lit_Phong); break;
				}
				break;
			case 1: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Unlit); break;
			case 2: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_Wireframe); break;
			case 3: ViewportClient->SetViewModeIndex(EViewModeIndex::VMI_SceneDepth); break;
			}
		}
		// 🔘 여기 ‘한 번 클릭’ 버튼 추가
		const float btnW = 60.0f;
		const ImVec2 btnSize(btnW, 0.0f);

		ImGui::SameLine();
		float avail = ImGui::GetContentRegionAvail().x;      // 현재 라인에서 남은 가로폭
		if (avail > btnW) {
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - btnW));
		}

		if (ImGui::Button("Switch##ToThis", btnSize))
		{
			SLATE.SwitchPanel(this);
		}

		//ImGui::PopStyleVar();

	}
	ImGui::End();
}