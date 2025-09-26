#include "pch.h"
#include "SMultiViewportWindow.h"
#include "SWindow.h"
#include "SSplitterV.h"
#include "ImGui/imgui.h"
#include"SceneIOWindow.h"
#include"SDetailsWindow.h"
#include"SControlPanel.h"
#include"MenuBarWidget.h"
#include"SViewportWindow.h"
extern float CLIENTWIDTH;
extern float CLIENTHEIGHT;
// ✅ 정적 멤버 초기화
SViewportWindow* SMultiViewportWindow::ActiveViewport;
void SMultiViewportWindow::SaveSplitterConfig()
{
	if (!RootSplitter) return;

	EditorINI["RootSplitter"] = std::to_string(RootSplitter->SplitRatio);
	EditorINI["TopPanel"] = std::to_string(TopPanel->SplitRatio);
	EditorINI["LeftTop"] = std::to_string(LeftTop->SplitRatio);
	EditorINI["LeftBottom"] = std::to_string(LeftBottom->SplitRatio);
	EditorINI["LeftPanel"] = std::to_string(LeftPanel->SplitRatio);
	EditorINI["BottomPanel"] = std::to_string(BottomPanel->SplitRatio);
}

// ------------------------------------------------------------
// Splitter 비율 불러오기
// ------------------------------------------------------------
void SMultiViewportWindow::LoadSplitterConfig()
{
	if (!RootSplitter) return;

	if (EditorINI.Contains("RootSplitter"))
		RootSplitter->SplitRatio = std::stof(EditorINI["RootSplitter"]);
	if (EditorINI.Contains("TopPanel"))
		TopPanel->SplitRatio = std::stof(EditorINI["TopPanel"]);
	if (EditorINI.Contains("LeftTop"))
		LeftTop->SplitRatio = std::stof(EditorINI["LeftTop"]);
	if (EditorINI.Contains("LeftBottom"))
		LeftBottom->SplitRatio = std::stof(EditorINI["LeftBottom"]);
	if (EditorINI.Contains("LeftPanel"))
		LeftPanel->SplitRatio = std::stof(EditorINI["LeftPanel"]);
	if (EditorINI.Contains("BottomPanel"))
		BottomPanel->SplitRatio = std::stof(EditorINI["BottomPanel"]);
}

SMultiViewportWindow::SMultiViewportWindow()
{
	for (int i = 0; i < 4; i++)
		Viewports[i] = nullptr;
}

SMultiViewportWindow::~SMultiViewportWindow()
{
	//스플리터 정보 editor.ini 정보 저장
	OnShutdown();
	delete RootSplitter;
	RootSplitter = nullptr;
	delete TopPanel;
	TopPanel = nullptr;
	delete LeftTop;
	LeftTop = nullptr;
	delete LeftBottom;
	LeftBottom = nullptr;
	delete LeftPanel;
	LeftPanel = nullptr;
	delete BottomPanel;
	BottomPanel = nullptr;
	delete SceneIOPanel;
	SceneIOPanel = nullptr;
	// 아래쪽 UI
	delete ControlPanel;
	ControlPanel = nullptr;
	delete DetailPanel;
	DetailPanel = nullptr;


	for (int i = 0; i < 4; i++)
	{
		if (Viewports[i]) {
			delete Viewports[i];
			Viewports[i] = nullptr;
		}
	}



}

void SMultiViewportWindow::Initialize(ID3D11Device* InDevice, UWorld* InWorld, const FRect& InRect, SViewportWindow* InMainViewport)
{
	MenuBar = NewObject<UMenuBarWidget>();
	MenuBar->SetOwner(this);   // 레이아웃 스위칭 등 제어를 위해 주입
	MenuBar->Initialize();

	MainViewport = InMainViewport;

	Device = InDevice;
	World = InWorld;
	Rect = InRect;

	// 최상위: 수평 스플리터 (위: 뷰포트+오른쪽UI, 아래: Console/Property)
	RootSplitter = new SSplitterH();
	//RootSplitter->SetSplitRatio(0.8);


	RootSplitter->SetRect(Rect.Min.X, Rect.Min.Y, Rect.Max.X, Rect.Max.Y);

	// === 위쪽: 좌(4뷰포트) + 우(SceneIO) ===

	TopPanel = new SSplitterV();
	TopPanel->SetSplitRatio(0.7f);


	// 왼쪽: 4분할 뷰포트
	LeftPanel = new SSplitterV();
	LeftTop = new SSplitterH();
	LeftBottom = new SSplitterH();
	LeftPanel->SideLT = LeftTop;
	LeftPanel->SideRB = LeftBottom;

	// 오른쪽: SceneIO UI
	SceneIOPanel = new SSceneIOWindow();
	SceneIOPanel->SetRect(Rect.Max.X - 300, Rect.Min.Y, Rect.Max.X, Rect.Max.Y);

	// TopPanel 좌우 배치
	TopPanel->SideLT = LeftPanel;
	TopPanel->SideRB = SceneIOPanel;

	// === 아래쪽: Console + Property ===
	BottomPanel = new SSplitterV();
	ControlPanel = new SControlPanel();   // 직접 만든 ConsoleWindow 클래스
	DetailPanel = new SDetailsWindow();  // 직접 만든 PropertyWindow 클래스
	BottomPanel->SideLT = ControlPanel;
	BottomPanel->SideRB = DetailPanel;

	// 최상위 스플리터에 연결
	RootSplitter->SideLT = TopPanel;
	RootSplitter->SideRB = BottomPanel;

	// === 뷰포트 생성 ===
	InMainViewport->SetMainViewPort();
	Viewports[0] = InMainViewport;
	Viewports[1] = new SViewportWindow();
	Viewports[2] = new SViewportWindow();
	Viewports[3] = new SViewportWindow();

	//Viewports[0]->Initialize(0, 0,
	//	Rect.GetWidth() / 2, Rect.GetHeight() / 2,
	//	World, Device, EViewportType::Perspective);

	Viewports[1]->Initialize(Rect.GetWidth() / 2, 0,
		Rect.GetWidth(), Rect.GetHeight() / 2,
		World, Device, EViewportType::Orthographic_Front);

	Viewports[2]->Initialize(0, Rect.GetHeight() / 2,
		Rect.GetWidth() / 2, Rect.GetHeight(),
		World, Device, EViewportType::Orthographic_Left);

	Viewports[3]->Initialize(Rect.GetWidth() / 2, Rect.GetHeight() / 2,
		Rect.GetWidth(), Rect.GetHeight(),
		World, Device, EViewportType::Orthographic_Top);

	// 뷰포트들을 2x2로 연결
	LeftTop->SideLT = Viewports[0];
	LeftTop->SideRB = Viewports[1];
	LeftBottom->SideLT = Viewports[2];
	LeftBottom->SideRB = Viewports[3];

	SwitchLayout(EViewportLayoutMode::SingleMain);

	LoadSplitterConfig();
}
void SMultiViewportWindow::SwitchLayout(EViewportLayoutMode NewMode)
{
	if (NewMode == CurrentMode) return;

	if (NewMode == EViewportLayoutMode::FourSplit)
	{
		TopPanel->SideLT = LeftPanel;

	}
	else if (NewMode == EViewportLayoutMode::SingleMain)
	{
		TopPanel->SideLT = MainViewport;
	}

	CurrentMode = NewMode;
}

void SMultiViewportWindow::SwitchPanel(SWindow* SwitchPanel)
{

	if (TopPanel->SideLT != SwitchPanel) {
		TopPanel->SideLT = SwitchPanel;
	}
	else {
		TopPanel->SideLT = LeftPanel;
	}
}

void SMultiViewportWindow::OnRender()
{
	// 메뉴바 렌더링 (항상 최상단에)

	MenuBar->RenderWidget();
	if (RootSplitter)
	{
		RootSplitter->OnRender();
	}
}

void SMultiViewportWindow::OnUpdate(float DeltaSeconds)
{
	if (RootSplitter) {
		// 메뉴바 높이만큼 아래로 이동
		float menuBarHeight = ImGui::GetFrameHeight();
		RootSplitter->Rect = FRect(0, menuBarHeight, CLIENTWIDTH, CLIENTHEIGHT);
		RootSplitter->OnUpdate(DeltaSeconds);
	}

}

void SMultiViewportWindow::OnMouseMove(FVector2D MousePos)
{
	if (ActiveViewport)
	{
		ActiveViewport->OnMouseMove(MousePos);
	}
	else if (RootSplitter)
	{
		RootSplitter->OnMouseMove(MousePos);
	}
}

void SMultiViewportWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
	if (RootSplitter)
	{
		RootSplitter->OnMouseDown(MousePos, Button);

		// 어떤 뷰포트 안에서 눌렸는지 확인
		for (auto* VP : Viewports)
		{
			if (VP && VP->Rect.Contains(MousePos))
			{
				ActiveViewport = VP; // 고정
				break;
			}
		}
	}
}


void SMultiViewportWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
	if (ActiveViewport)
	{
		ActiveViewport->OnMouseUp(MousePos, Button);
		ActiveViewport = nullptr; // 드래그 끝나면 해제
	}
	else if (RootSplitter)
	{
		RootSplitter->OnMouseUp(MousePos, Button);
	}
}

void SMultiViewportWindow::OnShutdown()
{
	SaveSplitterConfig();

}
