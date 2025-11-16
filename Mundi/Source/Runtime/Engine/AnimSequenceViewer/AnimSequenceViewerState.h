#pragma once

// FRect는 pch.h에 정의되어 있음

// Forward declarations
class UWorld;
class FViewport;
class FViewportClient;
class ASkeletalMeshActor;
class USkeletalMeshComponent;
class UAnimSequence;

// AnimSequence Viewer 탭별 상태
// SkeletalMesh Viewer의 ViewerState 패턴을 따름
class AnimSequenceViewerState
{
public:
    FName Name;
    UWorld* World = nullptr;
    FViewport* Viewport = nullptr;
    FViewportClient* Client = nullptr;

    // Preview Actor
    ASkeletalMeshActor* PreviewActor = nullptr;
    USkeletalMeshComponent* PreviewComponent = nullptr;

    // Animation Data
    UAnimSequence* CurrentAnimation = nullptr;
    FString LoadedAnimPath;
    float AnimationLength = 0.0f;

    // Playback State
    bool bIsPlaying = false;
    bool bLooping = true;
    float PlayRate = 1.0f;

    // Notify Editing State
    int32 SelectedNotifyIndex = -1;
    int32 DraggingNotifyIndex = -1;
    bool bIsDraggingNotify = false;
    float LastEditedTriggerTime = 0.0f;

    // UI Layout State
    FRect ViewportRect;
    FRect TimelineRect;
    FRect NotifyTrackRect;

    // UI Buffers (per-tab)
    char AnimPathBuffer[260] = {0};
    char NotifyNameBuffer[64] = {0};

    // Display Options
    float TopPanelHeight = 150.0f;
    float BottomPanelHeight = 250.0f;
};
