#pragma once
#include "SWindow.h"
#include "Source/Runtime/Engine/AnimSequenceViewer/AnimSequenceViewerState.h"

class FViewport;
class FViewportClient;
class UWorld;
class UAnimSequence;
class ASkeletalMeshActor;
class USkeletalMeshComponent;
struct ID3D11Device;
struct FAnimNotifyEvent;

// Animation Sequence Viewer Window
// 애니메이션 시퀀스를 로드하고 재생하며, 타임라인과 Notify를 편집할 수 있는 에디터 윈도우
// SkeletalMeshViewer의 ViewerState 패턴을 따름
class SAnimSequenceViewerWindow : public SWindow
{
public:
    SAnimSequenceViewerWindow();
    virtual ~SAnimSequenceViewerWindow();

    bool Initialize(float StartX, float StartY, float Width, float Height, ID3D11Device* InDevice);

    // SWindow overrides
    virtual void OnRender() override;
    virtual void OnUpdate(float DeltaSeconds) override;
    virtual void OnMouseMove(FVector2D MousePos) override;
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
    virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

    void OnRenderViewport();

    // Accessors (active tab)
    FViewport* GetViewport() const { return ActiveState ? ActiveState->Viewport : nullptr; }
    FViewportClient* GetViewportClient() const { return ActiveState ? ActiveState->Client : nullptr; }

    // 애니메이션 로드 (외부에서 호출 가능)
    void LoadAnimation(const FString& Path);

    // 윈도우 상태
    bool IsOpen() const { return bIsOpen; }
    void Close() { bIsOpen = false; }

private:
    // === UI 렌더링 함수 ===
    void RenderAssetBrowser(AnimSequenceViewerState* State);
    void RenderViewportPanel(AnimSequenceViewerState* State, float Height);
    void RenderTimelineControls(AnimSequenceViewerState* State);
    void RenderTimelineScrubber(AnimSequenceViewerState* State);
    void RenderNotifyTrack(AnimSequenceViewerState* State);
    void RenderNotifyProperties(AnimSequenceViewerState* State);

    // === 재생 제어 함수 ===
    void PlayAnimation(AnimSequenceViewerState* State);
    void PauseAnimation(AnimSequenceViewerState* State);
    void StopAnimation(AnimSequenceViewerState* State);
    void StepForward(AnimSequenceViewerState* State);
    void StepBackward(AnimSequenceViewerState* State);
    void SeekToTime(AnimSequenceViewerState* State, float Time);
    void SeekToFrame(AnimSequenceViewerState* State, int32 Frame);

    // === Notify 편집 함수 ===
    void AddNotify(AnimSequenceViewerState* State, float Time, const FName& NotifyName);
    void RemoveNotify(AnimSequenceViewerState* State, int32 Index);
    void UpdateNotifyTime(AnimSequenceViewerState* State, int32 Index, float NewTime);
    void SortNotifies(AnimSequenceViewerState* State);

    // === 애니메이션 저장 ===
    void SaveAnimation(AnimSequenceViewerState* State);

    // === 헬퍼 함수 ===
    float TimeToPixel(AnimSequenceViewerState* InState, float Time) const;
    float PixelToTime(AnimSequenceViewerState* InState, float PixelX) const;
    float GetCurrentAnimTime(AnimSequenceViewerState* InState) const;

    // === 탭 관리 ===
    void OpenNewTab(const char* Name = "AnimViewer");
    void CloseTab(int Index);

private:
    // === 탭별 상태 (ViewerState 패턴) ===
    AnimSequenceViewerState* ActiveState = nullptr;
    TArray<AnimSequenceViewerState*> Tabs;
    int ActiveTabIndex = -1;

    // === 초기화용 (Bootstrap에 전달) ===
    ID3D11Device* Device = nullptr;

    // === 윈도우 상태 ===
    bool bIsOpen = true;
    bool bInitialPlacementDone = false;
    bool bRequestFocus = false;
};
