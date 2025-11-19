#pragma once
#include "SWindow.h"

class AnimSequenceEditorState;
class UWorld;
class UTexture;
struct ID3D11Device;
struct FViewportRenderTarget;

class SAnimSequenceEditorWindow : public SWindow
{
public:
    SAnimSequenceEditorWindow();
    virtual ~SAnimSequenceEditorWindow();

    bool Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice);

    virtual void OnRender() override;
    virtual void OnRenderViewport();
    virtual void OnUpdate(float DeltaSeconds);
    virtual void OnMouseMove(FVector2D MousePos);
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button);
    virtual void OnMouseUp(FVector2D MousePos, uint32 Button);

    // Public file operations (called from USlateManager)
    void LoadAnimationFile(const FString& FilePath, const FString& MeshPath = "");
    void SaveAnimationFile(const FString& FilePath);

private:
    // Tab management
    void OpenNewTab(const char* Label);
    void CloseTab(int Index);

    // Animation rendering
    void RenderAnimationSquenceViewer();

    // UI rendering
    void RenderLeftPanel();
    void RenderCenterViewport();
    void RenderRightPanel();
    void RenderBottomTimeline();

    // Asset browser
    void RenderAssetBrowser();
    void RenderBoneHierarchy();

    // Notify editing
    void RenderNotifyTrackList();
    void RenderNotifyProperties();

    // Timeline and playback
    void RenderTimelineArea(class UAnimSequence* AnimSequence, class UAnimSingleNodeInstance* AnimInstance);

    // Helper functions
    void LoadSkeletalMesh(const FString& Path);
    void UpdateBoneLines();
    void UpdateBoneTransformFromSkeleton(AnimSequenceEditorState* State);
    void ApplyBoneTransformToSkeleton(AnimSequenceEditorState* State);
    void ExpandToSelectedBone(AnimSequenceEditorState* State, int32 BoneIndex);

private:
    UWorld* World = nullptr;
    ID3D11Device* Device = nullptr;

    TArray<AnimSequenceEditorState*> Tabs;
    AnimSequenceEditorState* ActiveState = nullptr;
    int32 ActiveTabIndex = 0;

    FRect CenterRect;

    // Panel sizing
    float LeftPanelRatio = 0.2f;
    float RightPanelRatio = 0.2f;
    float BottomPanelRatio = 0.3f;

    // UI state
    bool bInitialPlacementDone = false;
    bool bRequestFocus = false;
    bool bIsOpen = true;
    bool bShouldRenderViewport = false;  // Set to true only when window is visible and not occluded

    // Icons for playback controls
    UTexture* IconPause = nullptr;
    UTexture* IconResume = nullptr;

    // 뷰포트 렌더 타겟 (ImGui::Image로 표시하기 위해)
    FViewportRenderTarget ViewportRenderTarget;

public:
    bool IsOpen() const { return bIsOpen; }
    void Close() { bIsOpen = false; }
};
