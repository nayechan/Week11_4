#pragma once
#include "SWindow.h"
#include "SSplitterV.h"
#include "SSplitterH.h"
#include "SViewportWindow.h"

class SSceneIOWindow; // 새로 추가할 UI
class SDetailsWindow;
class UMenuBarWidget;
class SMultiViewportWindow : public SWindow
{
public:
    void SaveSplitterConfig();
    void LoadSplitterConfig();
    SMultiViewportWindow();
    virtual ~SMultiViewportWindow();

    void Initialize(ID3D11Device* Device, UWorld* World, const FRect& InRect, SViewportWindow* MainViewport);
    void SwitchLayout(EViewportLayoutMode NewMode);

    void SwitchPanel(SWindow* SwitchPanel);


    virtual void OnRender() override;
    virtual void OnUpdate(float deltaSecond) override;
    virtual void OnMouseMove(FVector2D MousePos) override;
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
    virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

    void SetMainViewPort();


    void OnShutdown();

    static SViewportWindow* ActiveViewport; // 현재 드래그 중인 뷰포트
private:
    UWorld* World = nullptr;
    ID3D11Device* Device = nullptr;

    SSplitterH* RootSplitter = nullptr;

    // 두 가지 레이아웃을 미리 생성해둠
    SSplitter* FourSplitLayout = nullptr;
    SSplitter* SingleLayout = nullptr;

    // 뷰포트
    SViewportWindow* Viewports[4];
    SViewportWindow* MainViewport;
 
    SSplitterH* LeftTop;
    SSplitterH* LeftBottom ;
    // 오른쪽 고정 UI
    SWindow* SceneIOPanel = nullptr;
    // 아래쪽 UI
    SWindow* ControlPanel = nullptr;
    SWindow* DetailPanel = nullptr;

    SSplitterV* TopPanel = nullptr;
    SSplitterV* LeftPanel = nullptr;

    SSplitterV* BottomPanel ;
    
   

    // 현재 모드
    EViewportLayoutMode CurrentMode = EViewportLayoutMode::FourSplit;

    // 메뉴바 관련
    void OnFileMenuAction(const char* action);
    void OnEditMenuAction(const char* action);
    void OnWindowMenuAction(const char* action);
    void OnHelpMenuAction(const char* action);

    UMenuBarWidget* MenuBar;
};
