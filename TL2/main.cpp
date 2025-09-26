#include "pch.h"
#include <fstream>
#include "FViewport.h"
#include "FViewportClient.h"
#include "SSplitterH.h"
#include "SSplitterV.h"
#include "SMultiViewportWindow.h"
// TODO: Delete it, just Test

float CLIENTWIDTH = 1024.0f;
float CLIENTHEIGHT = 1024.0f;


void LoadIniFile()
{
    std::ifstream infile("editor.ini");

    if (!infile.is_open())
    {
        return;
    }

    std::string line;
    while (std::getline(infile, line))
    {
        if (line.empty() || line[0] == ';')
        {
            continue;
        }

        size_t delimiterPos = line.find('=');
        if (delimiterPos != FString::npos)
        {
            FString key = line.substr(0, delimiterPos);
            std::string value = line.substr(delimiterPos + 1);

            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            EditorINI[key] = value;
        }
    }

    infile.close();
    return;
}

void SaveIniFile()
{
    std::ofstream outfile("editor.ini");
    for (const auto& pair : EditorINI)
    {
        outfile << pair.first << " = " << pair.second << std::endl;
    }
    outfile.close();
}

#if defined(_MSC_VER) && defined(_DEBUG)
#   define _CRTDBG_MAP_ALLOC
#   include <cstdlib>
#   include <crtdbg.h>
#endif
#include "UI/Factory/UIWindowFactory.h"

void GetViewportSize(HWND hWnd)
{
    RECT clientRect{};
    GetClientRect(hWnd, &clientRect);

    CLIENTWIDTH = static_cast<float>(clientRect.right - clientRect.left);
    CLIENTHEIGHT = static_cast<float>(clientRect.bottom - clientRect.top);

    if (CLIENTWIDTH <= 0) CLIENTWIDTH = 1;
    if (CLIENTHEIGHT <= 0) CLIENTHEIGHT = 1;
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // InputManager에 먼저 메시지 전달 (ImGui보다 먼저)
    UInputManager::GetInstance().ProcessMessage(hWnd, message, wParam, lParam);

    // ImGui 처리
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
    {
        return true;
    }

    switch (message)
    {
    case WM_SIZE:
        {
            WPARAM sizeType = wParam;
            if (sizeType != SIZE_MINIMIZED)
            {
                GetViewportSize(hWnd); // 창 크기 바뀔 때 전역 갱신
             
                // Renderer의 뷰포트 갱신
                if (auto world = UUIManager::GetInstance().GetWorld())
                {
                    if (auto renderer = world->GetRenderer())
                    {
                        UINT newWidth = static_cast<UINT>(CLIENTWIDTH);
                        UINT newHeight = static_cast<UINT>(CLIENTHEIGHT);
                        // Single, consistent resize path (handles RTV/DSV + viewport)
                        static_cast<D3D11RHI*>(renderer->GetRHIDevice())->ResizeSwapChain(newWidth, newHeight);
                        EditorINI["WindowWidth"] = std::to_string(newWidth);
                        EditorINI["WindowHeight"] = std::to_string(newHeight);
                    }
                    // ImGui DisplaySize가 유효할 때만 UI 윈도우 재배치
                    ImGuiIO& io = ImGui::GetIO();
                    if (io.DisplaySize.x > 0 && io.DisplaySize.y > 0)
                    {
                        UUIManager::GetInstance().RepositionImGuiWindows();
                    }
                }
            }
        }
        break;
    case WM_DESTROY:
        // Signal that the app should quit
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

void InitManager(HWND hWnd)
{
    // Renderer Class를 생성합니다.
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
#if defined(_MSC_VER) && defined(_DEBUG)
    // Enable CRT debug heap and automatic leak check on exit
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
    _CrtSetBreakAlloc(0); // Uncomment and set alloc ID to break on specific leak
#endif
    LoadIniFile();

    // 윈도우 클래스 이름
    WCHAR WindowClass[] = L"JungleWindowClass";

    // 윈도우 타이틀바에 표시될 이름
    WCHAR Title[] = L"Game Tech Lab";

    // 각종 메시지를 처리할 함수인 WndProc의 함수 포인터를 WindowClass 구조체에 넣는다.
    WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };

    // 윈도우 클래스 등록
    RegisterClassW(&wndclass);

    // 저장된 창 크기 로드 (없으면 기본값 1024x1024)
    int windowWidth = 1620, windowHeight = 1024;

    if (EditorINI.count("WindowWidth"))
    {
        try
        {
            windowWidth = stoi(EditorINI["WindowWidth"]);
        }
        catch (...)
        {
        }
    }
    if (EditorINI.count("WindowHeight"))
    {
        try
        {
            windowHeight = std::stoi(EditorINI["WindowHeight"]);
        }
        catch (...)
        {
        }
    }

    // 윈도우 생성
    HWND hWnd = CreateWindowExW(0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, windowWidth, windowHeight,
        nullptr, nullptr, hInstance, nullptr);

    //종횡비 계산
    GetViewportSize(hWnd);

    {
        D3D11RHI d3d11RHI;
        d3d11RHI.Initialize(hWnd);
        URenderer renderer(&d3d11RHI); //렌더러 생성이 가장 먼저 되어야 합니다.

    //UResourceManager::GetInstance().Initialize(d3d11RHI.GetDevice(),d3d11RHI.GetDeviceContext()); //리소스매니저 이니셜라이즈
    // UI Manager Initialize
    UUIManager::GetInstance().Initialize(hWnd, d3d11RHI.GetDevice(), d3d11RHI.GetDeviceContext()); //유아이매니저 이니셜라이즈
    UUIWindowFactory::CreateDefaultUILayout();
    
    // InputManager 초기화 (TUUIManager 이후)
    UInputManager::GetInstance().Initialize(hWnd); //인풋 매니저 이니셜라이즈

        //======================================================================================================================
        //월드 생성
        UWorld* World = &UWorld::GetInstance();
        World->SetRenderer(&renderer);//렌더러 설정
        World->Initialize(); 

        //메인 뷰포트 생성
        SViewportWindow* MainViewport = new SViewportWindow();
        MainViewport->Initialize(0, 0, 1000, 1000, World, renderer.GetRHIDevice()->GetDevice(), EViewportType::Perspective);
        // 멀티 뷰포트 생성
        SMultiViewportWindow* MultiViewportWindow = nullptr;
        MultiViewportWindow = new SMultiViewportWindow();
        FRect ScreenRect(0, 0, static_cast<float>(windowWidth), static_cast<float>(windowHeight));
        MultiViewportWindow->Initialize(renderer.GetRHIDevice()->GetDevice(),World, ScreenRect,MainViewport);
        //월드에 뷰포드 설정
        World->SetMultiViewportWindow(MultiViewportWindow);
        World->SetMainViewport(MainViewport);

        //스폰을 위한 월드셋
        UUIManager::GetInstance().SetWorld(World);
        //======================================================================================================================      
        LARGE_INTEGER Frequency;
        QueryPerformanceFrequency(&Frequency);

        LARGE_INTEGER PrevTime, CurrTime;
        QueryPerformanceCounter(&PrevTime);

        FVector CameraLocation{ 0, 0, -10.f };


        bool bUVScrollPaused = true;
        float UVScrollTime = 0.0f;
        FVector2D UVScrollSpeed = FVector2D(0.5f, 0.5f);

        UInputManager& InputMgr = UInputManager::GetInstance();

        bool bIsExit = false;
        while (bIsExit == false)
        {
            MSG msg;

            QueryPerformanceCounter(&CurrTime);

            // 프레임 간 시간 (초 단위)
            float DeltaSeconds = static_cast<float>(
                (CurrTime.QuadPart - PrevTime.QuadPart) / double(Frequency.QuadPart)
                );
            PrevTime = CurrTime;



            if (InputMgr.IsKeyPressed('T'))
            {
                bUVScrollPaused = !bUVScrollPaused;
                if (bUVScrollPaused)
                {
                    // reset when paused
                    UVScrollTime = 0.0f;
                    renderer.UpdateUVScroll(UVScrollSpeed, UVScrollTime);
                }
            }


            if (!bUVScrollPaused)
            {
                UVScrollTime += DeltaSeconds;
            }


            renderer.UpdateUVScroll(UVScrollSpeed, UVScrollTime);

            // 이제 Tick 호출
            World->Tick(DeltaSeconds);
            World->Render();
            //UE_LOG("Hello World %d", 2025);

            // 처리할 메시지가 더 이상 없을때 까지 수행
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);

                if (msg.message == WM_QUIT)
                {
                    bIsExit = true;
                    break;
                }
            }

            if (InputMgr.IsKeyPressed(VK_ESCAPE))
            {
                UE_LOG("ESC Key Pressed - Exiting!\n");
                bIsExit = true;
            }

            // 마우스 위치 실시간 출력 (매 60프레임마다)
            static int frameCount = 0;
            frameCount++;
            if (frameCount % 60 == 0) // 60프레임마다 출력
            {
                FVector2D mousePos = InputMgr.GetMousePosition();
                FVector2D mouseDelta = InputMgr.GetMouseDelta();
                // char debugMsg[128];
            }
        }

        delete MultiViewportWindow;
     
        UUIManager::GetInstance().Release();
        ObjectFactory::DeleteAll(true);
    }
    SaveIniFile();

    return 0;
}