#include "pch.h"
#include "USlateManager.h"
#include "Windows/SWindow.h"
#include "Windows/SSplitterV.h"
#include "Windows/SceneIOWindow.h"
#include "Windows/SDetailsWindow.h"
#include "Windows/SControlPanel.h"
#include "Windows/SViewportWindow.h"
#include "Windows/ConsoleWindow.h"
#include "Widgets/MenuBarWidget.h"
#include "FViewportClient.h"
#include "UIManager.h"
#include "GlobalConsole.h"

IMPLEMENT_CLASS(USlateManager)

USlateManager& USlateManager::GetInstance()
{
    static USlateManager* Instance = nullptr;
    if (Instance == nullptr)
    {
        Instance = NewObject<USlateManager>();
    }
    return *Instance;
}
#include "FViewportClient.h"

extern float CLIENTWIDTH;
extern float CLIENTHEIGHT;

SViewportWindow* USlateManager::ActiveViewport;

void USlateManager::SaveSplitterConfig()
{
    if (!TopPanel) return;

    EditorINI["TopPanel"] = std::to_string(TopPanel->SplitRatio);
    EditorINI["LeftTop"] = std::to_string(LeftTop->SplitRatio);
    EditorINI["LeftBottom"] = std::to_string(LeftBottom->SplitRatio);
    EditorINI["LeftPanel"] = std::to_string(LeftPanel->SplitRatio);
    EditorINI["RightPanel"] = std::to_string(RightPanel->SplitRatio);
}

void USlateManager::LoadSplitterConfig()
{
    if (!TopPanel) return;

    if (EditorINI.Contains("TopPanel"))
        TopPanel->SplitRatio = std::stof(EditorINI["TopPanel"]);
    if (EditorINI.Contains("LeftTop"))
        LeftTop->SplitRatio = std::stof(EditorINI["LeftTop"]);
    if (EditorINI.Contains("LeftBottom"))
        LeftBottom->SplitRatio = std::stof(EditorINI["LeftBottom"]);
    if (EditorINI.Contains("LeftPanel"))
        LeftPanel->SplitRatio = std::stof(EditorINI["LeftPanel"]);
    if (EditorINI.Contains("RightPanel"))
        RightPanel->SplitRatio = std::stof(EditorINI["RightPanel"]);
}

USlateManager::USlateManager()
{
    for (int i = 0; i < 4; i++)
        Viewports[i] = nullptr;
}

USlateManager::~USlateManager()
{
    Shutdown();
}

void USlateManager::Initialize(ID3D11Device* InDevice, UWorld* InWorld, const FRect& InRect)
{
    MenuBar = NewObject<UMenuBarWidget>();
    MenuBar->SetOwner(this);   // 레이아웃 스위칭 등 제어를 위해 주입
    MenuBar->Initialize();

    Device = InDevice;
    World = InWorld;
    Rect = InRect;

    // === 전체 화면: 좌(4뷰포트) + 우(Control + Details) ===
    TopPanel = new SSplitterH();  // 수평 분할 (좌우)
    TopPanel->SetSplitRatio(0.7f);  // 70% 뷰포트, 30% UI
    TopPanel->SetRect(Rect.Min.X, Rect.Min.Y, Rect.Max.X, Rect.Max.Y);

    // 왼쪽: 4분할 뷰포트 영역
    LeftPanel = new SSplitterH();  // 수평 분할 (좌우)
    LeftTop = new SSplitterV();    // 수직 분할 (상하)
    LeftBottom = new SSplitterV(); // 수직 분할 (상하)
    LeftPanel->SideLT = LeftTop;
    LeftPanel->SideRB = LeftBottom;

    // 오른쪽: Control + Details (상하 분할)
    RightPanel = new SSplitterV();  // 수직 분할 (상하)
    RightPanel->SetSplitRatio(0.5f);  // 50-50 분할

    ControlPanel = new SControlPanel();
    DetailPanel = new SDetailsWindow();

    RightPanel->SideLT = ControlPanel;   // 위쪽: ControlPanel
    RightPanel->SideRB = DetailPanel;    // 아래쪽: DetailsWindow

    // TopPanel 좌우 배치
    TopPanel->SideLT = LeftPanel;
    TopPanel->SideRB = RightPanel;

    // === 뷰포트 생성 ===
    Viewports[0] = new SViewportWindow();
    Viewports[1] = new SViewportWindow();
    Viewports[2] = new SViewportWindow();
    Viewports[3] = new SViewportWindow();
    MainViewport = Viewports[0];

    Viewports[0]->Initialize(0, 0,
        Rect.GetWidth() / 2, Rect.GetHeight() / 2,
        World, Device, EViewportType::Perspective);

    Viewports[1]->Initialize(Rect.GetWidth() / 2, 0,
        Rect.GetWidth(), Rect.GetHeight() / 2,
        World, Device, EViewportType::Orthographic_Front);

    Viewports[2]->Initialize(0, Rect.GetHeight() / 2,
        Rect.GetWidth() / 2, Rect.GetHeight(),
        World, Device, EViewportType::Orthographic_Left);

    Viewports[3]->Initialize(Rect.GetWidth() / 2, Rect.GetHeight() / 2,
        Rect.GetWidth(), Rect.GetHeight(),
        World, Device, EViewportType::Orthographic_Top);

    World->SetCameraActor(MainViewport->GetViewportClient()->GetCamera());

    // 뷰포트들을 2x2로 연결
    LeftTop->SideLT = Viewports[0];
    LeftTop->SideRB = Viewports[1];
    LeftBottom->SideLT = Viewports[2];
    LeftBottom->SideRB = Viewports[3];

    SwitchLayout(EViewportLayoutMode::SingleMain);

    LoadSplitterConfig();

    // === Console Overlay 생성 ===
    ConsoleWindow = new UConsoleWindow();
    if (ConsoleWindow)
    {
        UE_LOG("USlateManager: ConsoleWindow created successfully");
        UGlobalConsole::SetConsoleWidget(ConsoleWindow->GetConsoleWidget());
        UE_LOG("USlateManager: GlobalConsole connected to ConsoleWidget");
    }
    else
    {
        UE_LOG("ERROR: Failed to create ConsoleWindow");
    }
}

void USlateManager::SwitchLayout(EViewportLayoutMode NewMode)
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

void USlateManager::SwitchPanel(SWindow* SwitchPanel)
{
    if (TopPanel->SideLT != SwitchPanel) {
        TopPanel->SideLT = SwitchPanel;
        CurrentMode = EViewportLayoutMode::SingleMain;
    }
    else {
        TopPanel->SideLT = LeftPanel;
        CurrentMode = EViewportLayoutMode::FourSplit;
    }
}

void USlateManager::Render()
{
    // 메뉴바 렌더링 (항상 최상단에)
    MenuBar->RenderWidget();
    if (TopPanel)
    {
        TopPanel->OnRender();
    }

    // Console overlay rendering (on top of everything)
    if (ConsoleWindow && ConsoleAnimationProgress > 0.0f)
    {
        extern float CLIENTWIDTH;
        extern float CLIENTHEIGHT;

        // Apply ease-out curve for smooth deceleration
        float EasedProgress = 1.0f - (1.0f - ConsoleAnimationProgress) * (1.0f - ConsoleAnimationProgress);

        // Calculate console dimensions
        float ConsoleHeight = CLIENTHEIGHT * ConsoleHeightRatio;
        float ConsoleWidth = CLIENTWIDTH;

        // Calculate Y position (slide up from bottom)
        float YPosWhenHidden = CLIENTHEIGHT; // Off-screen at bottom
        float YPosWhenVisible = CLIENTHEIGHT - ConsoleHeight; // Visible at bottom
        float CurrentYPos = YPosWhenHidden + (YPosWhenVisible - YPosWhenHidden) * EasedProgress;

        // Set window position and size
        ImGui::SetNextWindowPos(ImVec2(0, CurrentYPos));
        ImGui::SetNextWindowSize(ImVec2(ConsoleWidth, ConsoleHeight));

        // Window flags - Remove NoBringToFrontOnFocus to ensure it appears on top
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoScrollWithMouse;

        // Push window to front
        ImGui::SetNextWindowFocus();

        // Render console
        if (ImGui::Begin("ConsoleOverlay", nullptr, flags))
        {
            // Add semi-transparent background
            ImDrawList* DrawList = ImGui::GetWindowDrawList();
            ImVec2 WindowPos = ImGui::GetWindowPos();
            ImVec2 WindowSize = ImGui::GetWindowSize();
            DrawList->AddRectFilled(
                WindowPos,
                ImVec2(WindowPos.x + WindowSize.x, WindowPos.y + WindowSize.y),
                IM_COL32(20, 20, 20, 240) // Dark background with high opacity
            );

            // Render console widget
            ConsoleWindow->RenderWidget();
        }
        ImGui::End();
    }
}

void USlateManager::Update(float DeltaSeconds)
{
    ProcessInput();
    if (TopPanel)
    {
        // 메뉴바 높이만큼 아래로 이동
        float menuBarHeight = ImGui::GetFrameHeight();
        TopPanel->Rect = FRect(0, menuBarHeight, CLIENTWIDTH, CLIENTHEIGHT);
        TopPanel->OnUpdate(DeltaSeconds);
    }

    // Console animation update
    if (bIsConsoleAnimating)
    {
        if (bIsConsoleVisible)
        {
            // Animating in (showing)
            ConsoleAnimationProgress += DeltaSeconds / ConsoleAnimationDuration;
            if (ConsoleAnimationProgress >= 1.0f)
            {
                ConsoleAnimationProgress = 1.0f;
                bIsConsoleAnimating = false;
            }
        }
        else
        {
            // Animating out (hiding)
            ConsoleAnimationProgress -= DeltaSeconds / ConsoleAnimationDuration;
            if (ConsoleAnimationProgress <= 0.0f)
            {
                ConsoleAnimationProgress = 0.0f;
                bIsConsoleAnimating = false;
            }
        }
    }

    // Update ConsoleWindow
    if (ConsoleWindow && ConsoleAnimationProgress > 0.0f)
    {
        ConsoleWindow->Update();
    }
}

void USlateManager::ProcessInput()
{
    const FVector2D MousePosition = INPUT.GetMousePosition();

    if (INPUT.IsMouseButtonPressed(LeftButton))
    {
        const FVector2D MousePosition = INPUT.GetMousePosition();
        {
            OnMouseDown(MousePosition, 0);
        }
    }
    if (INPUT.IsMouseButtonPressed(RightButton))
    {
        const FVector2D MousePosition = INPUT.GetMousePosition();
        {
            OnMouseDown(MousePosition, 1);
        }
    }
    if (INPUT.IsMouseButtonReleased(LeftButton))
    {
        const FVector2D MousePosition = INPUT.GetMousePosition();
        {
            OnMouseUp(MousePosition, 0);
        }
    }
    if (INPUT.IsMouseButtonReleased(RightButton))
    {
        const FVector2D MousePosition = INPUT.GetMousePosition();
        {
            OnMouseUp(MousePosition, 1);
        }
    }
    OnMouseMove(MousePosition);

    // Console toggle with Alt + ` (Grave Accent)
    if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent) && ImGui::GetIO().KeyAlt)
    {
        ToggleConsole();
    }

    // 단축키로 기즈모 모드 변경
    if (World->GetGizmoActor())
        World->GetGizmoActor()->ProcessGizmoModeSwitch();
}

void USlateManager::OnMouseMove(FVector2D MousePos)
{
    if (ActiveViewport)
    {
        ActiveViewport->OnMouseMove(MousePos);
    }
    else if (TopPanel)
    {
        TopPanel->OnMouseMove(MousePos);
    }
}

void USlateManager::OnMouseDown(FVector2D MousePos, uint32 Button)
{
    if (TopPanel)
    {
        TopPanel->OnMouseDown(MousePos, Button);

        // 어떤 뷰포트 안에서 눌렸는지 확인
        for (auto* VP : Viewports)
        {
            if (VP && VP->Rect.Contains(MousePos))
            {
                ActiveViewport = VP; // 고정

                // 우클릭인 경우 커서 숨김 및 잠금
                if (Button == 1)
                {
                    INPUT.SetCursorVisible(false);
                    INPUT.LockCursor();
                }
                break;
            }
        }
    }
}

void USlateManager::OnMouseUp(FVector2D MousePos, uint32 Button)
{
    // 우클릭 해제 시 커서 복원 (ActiveViewport와 무관하게 처리)
    if (Button == 1 && INPUT.IsCursorLocked())
    {
        INPUT.SetCursorVisible(true);
        INPUT.ReleaseCursor();
    }

    if (ActiveViewport)
    {
        ActiveViewport->OnMouseUp(MousePos, Button);
        ActiveViewport = nullptr; // 드래그 끝나면 해제
    }
    else if (TopPanel)
    {
        TopPanel->OnMouseUp(MousePos, Button);
    }
}

void USlateManager::OnShutdown()
{
    SaveSplitterConfig();
}

void USlateManager::Shutdown()
{
    // Save layout/config
    SaveSplitterConfig();

    // Delete console window
    if (ConsoleWindow)
    {
        delete ConsoleWindow;
        ConsoleWindow = nullptr;
        UE_LOG("USlateManager: ConsoleWindow destroyed");
    }

    // Delete UI panels and viewports explicitly to release any held D3D contexts
    if (TopPanel) { delete TopPanel; TopPanel = nullptr; }
    if (LeftTop) { delete LeftTop; LeftTop = nullptr; }
    if (LeftBottom) { delete LeftBottom; LeftBottom = nullptr; }
    if (LeftPanel) { delete LeftPanel; LeftPanel = nullptr; }
    if (RightPanel) { delete RightPanel; RightPanel = nullptr; }

    if (ControlPanel) { delete ControlPanel; ControlPanel = nullptr; }
    if (DetailPanel) { delete DetailPanel; DetailPanel = nullptr; }

    for (int i = 0; i < 4; ++i)
    {
        if (Viewports[i]) { delete Viewports[i]; Viewports[i] = nullptr; }
    }
    MainViewport = nullptr;
    ActiveViewport = nullptr;
}

void USlateManager::SetPIEWorld(UWorld* InWorld)
{
    MainViewport->SetVClientWorld(InWorld);
}

void USlateManager::ToggleConsole()
{
    bIsConsoleVisible = !bIsConsoleVisible;
    bIsConsoleAnimating = true;

    UE_LOG("USlateManager: Console toggled - %s", bIsConsoleVisible ? "SHOWING" : "HIDING");
}
