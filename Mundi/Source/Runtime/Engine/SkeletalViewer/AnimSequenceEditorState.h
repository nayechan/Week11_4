#pragma once

class UWorld; class FViewport; class FViewportClient; class ASkeletalMeshActor; class USkeletalMesh; struct FSkeleton;

class AnimSequenceEditorState
{
public:
    FName Name;
    UWorld* World = nullptr;
    FViewport* Viewport = nullptr;
    FViewportClient* Client = nullptr;

    // Preview actor for animation playback
    ASkeletalMeshActor* PreviewActor = nullptr;
    USkeletalMesh* CurrentMesh = nullptr;
    FString LoadedMeshPath;  // Track loaded mesh path

    // Display options
    bool bShowMesh = true;
    bool bShowBones = true;

    // Bone line rebuild control
    bool bBoneLinesDirty = true;
    int32 LastSelectedBoneIndex = -1;

    // UI path buffers
    char MeshPathBuffer[260] = {0};
    char AnimationPathBuffer[260] = {0};

    // Animation playback
    bool bLoopAnimation = true;  // Default to looping animation

    // Skeleton Swapping (matches PropertyRenderer pattern)
    FSkeleton* TargetSkeleton = nullptr;

    // Bone selection for Notify editing
    int32 SelectedBoneIndex = -1;
    std::set<int32> ExpandedBoneIndices;

    // Notify tracks
    struct NotifyTrack
    {
        int32 ID;
        FString Name;
    };
    TArray<NotifyTrack> NotifyTracks;
    int32 NextTrackID = 1;
    int32 SelectedNotifyTrackID = -1;
};
