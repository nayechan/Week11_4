#pragma once
#include "SWindow.h"

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
class SAnimSequenceViewerWindow : public SWindow
{
public:
    SAnimSequenceViewerWindow();
    virtual ~SAnimSequenceViewerWindow();

    bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice);

    // SWindow overrides
    virtual void OnRender() override;
    virtual void OnUpdate(float DeltaSeconds) override;
    virtual void OnMouseMove(FVector2D MousePos) override;
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
    virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

    void OnRenderViewport();

    // 애니메이션 로드
    void LoadAnimation(const FString& Path);

    // 애니메이션 저장
    void SaveAnimation();

    // 윈도우 상태
    bool IsOpen() const { return bIsOpen; }
    void Close() { bIsOpen = false; }

private:
    // === UI 렌더링 함수 ===
    void RenderAssetBrowser();
    void RenderViewportPanel(float Height);
    void RenderTimelineControls();
    void RenderTimelineScrubber();
    void RenderNotifyTrack();
    void RenderNotifyProperties();

    // === 재생 제어 함수 ===
    void PlayAnimation();
    void PauseAnimation();
    void StopAnimation();
    void StepForward();   // 다음 프레임
    void StepBackward();  // 이전 프레임
    void SeekToTime(float Time);
    void SeekToFrame(int32 Frame);

    // === Notify 편집 함수 ===
    void AddNotify(float Time, const FName& NotifyName);
    void RemoveNotify(int32 Index);
    void UpdateNotifyTime(int32 Index, float NewTime);

    // === 헬퍼 함수 ===
    float TimeToPixel(float Time) const;
    float PixelToTime(float PixelX) const;

private:
    // === 뷰포트 & 월드 ===
    UWorld* World = nullptr;
    ID3D11Device* Device = nullptr;
    FViewport* Viewport = nullptr;
    FViewportClient* ViewportClient = nullptr;

    // === 프리뷰 액터 ===
    ASkeletalMeshActor* PreviewActor = nullptr;
    USkeletalMeshComponent* PreviewComponent = nullptr;

    // === 애니메이션 데이터 ===
    UAnimSequence* CurrentAnimation = nullptr;
    FString LoadedAnimPath;
    char AnimPathBuffer[260] = {0};

    // === 재생 제어 상태 ===
    bool bIsPlaying = false;
    bool bLooping = true;
    float PlayRate = 1.0f;
    float CurrentTime = 0.0f;
    float AnimationLength = 0.0f;

    // === Notify 편집 상태 ===
    int32 SelectedNotifyIndex = -1;
    bool bEditingNotify = false;
    char NotifyNameBuffer[64] = {0};
    float NotifyTriggerTime = 0.0f;
    float NotifyDuration = 0.0f;

    // === UI 레이아웃 ===
    FRect ViewportRect;      // 뷰포트 영역
    FRect TimelineRect;      // 타임라인 슬라이더 영역
    FRect NotifyTrackRect;   // Notify 트랙 영역

    float TopPanelHeight = 150.0f;      // 상단 패널 (Asset Browser)
    float BottomPanelHeight = 250.0f;   // 하단 패널 (Timeline + Notify)

    // === 윈도우 상태 ===
    bool bIsOpen = true;
    bool bInitialPlacementDone = false;
    bool bRequestFocus = false;
};
