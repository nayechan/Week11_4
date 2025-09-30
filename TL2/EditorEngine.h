#pragma once
#include "pch.h"

class UEditorEngine final {
public:
    UEditorEngine();
    ~UEditorEngine();

    bool Startup(HINSTANCE hInstance);
    void MainLoop();
    void Shutdown();

    HWND GetHWND() const { return HWnd; }

private:
    bool CreateMainWindow(HINSTANCE hInstance);
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static void GetViewportSize(HWND hWnd);

    void Tick(float DeltaSeconds);
    void HandleUVInput(float DeltaSeconds);

private:
    //윈도우 핸들
    HWND HWnd = nullptr;

    //디바이스 리소스 및 렌더러
    D3D11RHI RHIDevice; 
    std::unique_ptr<URenderer> Renderer;
    
    //월드 로직 수정 필요
    UWorld* World = nullptr;

    //에디터 전용?
    USlateManager* SlateManager = nullptr;

    //틱 상태
    bool bRunning = false;
    bool bUVScrollPaused = true;
    float UVScrollTime = 0.0f;
    FVector2D UVScrollSpeed = FVector2D(0.5f, 0.5f);

    // 클라이언트 사이즈
    static float ClientWidth;
    static float ClientHeight;
};
