#include "pch.h"
#include "AnimSequenceViewerBootstrap.h"
#include "Source/Slate/Windows/SWindow.h"  // FRect 정의를 위해 필요
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Renderer/FViewport.h"
#include "Source/Runtime/Renderer/FSkeletalViewerViewportClient.h"

AnimSequenceViewerState* AnimSequenceViewerBootstrap::CreateViewerState(const char* Name, ID3D11Device* InDevice)
{
    if (!InDevice)
        return nullptr;

    AnimSequenceViewerState* State = new AnimSequenceViewerState();
    State->Name = Name ? Name : "AnimViewer";

    // ========================================
    // 1. Preview World 생성 (독립적인 World)
    // ========================================
    State->World = NewObject<UWorld>();
    State->World->SetWorldType(EWorldType::PreviewMinimal);  // 메모리 최적화를 위한 Preview 모드
    State->World->Initialize();
    State->World->GetRenderSettings().DisableShowFlag(EEngineShowFlags::SF_EditorIcon);

    // Gizmo는 Local Space로 설정
    State->World->GetGizmoActor()->SetSpace(EGizmoSpace::Local);

    // ========================================
    // 2. Viewport 및 ViewportClient 생성
    // ========================================
    State->Viewport = new FViewport();
    // 초기 크기는 1x1로 설정 (매 프레임 리사이즈됨)
    State->Viewport->Initialize(0, 0, 1, 1, InDevice);

    auto* Client = new FSkeletalViewerViewportClient();
    Client->SetWorld(State->World);
    Client->SetViewportType(EViewportType::Perspective);
    Client->SetViewMode(EViewMode::VMI_Lit_Phong);
    Client->GetCamera()->SetActorLocation(FVector(3, 0, 2));

    State->Client = Client;
    State->Viewport->SetViewportClient(Client);

    // Editor 카메라 설정
    State->World->SetEditorCameraActor(Client->GetCamera());

    // ========================================
    // 3. Preview Actor 생성
    // ========================================
    if (State->World)
    {
        ASkeletalMeshActor* Preview = State->World->SpawnActor<ASkeletalMeshActor>();
        State->PreviewActor = Preview;
        State->PreviewComponent = Preview ? Preview->GetSkeletalMeshComponent() : nullptr;

        // 애니메이션 모드를 SingleNode로 설정
        if (State->PreviewComponent)
        {
            State->PreviewComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        }
    }

    // ========================================
    // 4. UI Rect 초기화
    // ========================================
    State->ViewportRect = FRect(0, 0, 0, 0);
    State->TimelineRect = FRect(0, 0, 0, 0);
    State->NotifyTrackRect = FRect(0, 0, 0, 0);

    return State;
}

void AnimSequenceViewerBootstrap::DestroyViewerState(AnimSequenceViewerState*& State)
{
    if (!State)
        return;

    // Viewport 정리
    if (State->Viewport)
    {
        delete State->Viewport;
        State->Viewport = nullptr;
    }

    // ViewportClient 정리
    if (State->Client)
    {
        delete State->Client;
        State->Client = nullptr;
    }

    // World 정리 (PreviewActor도 함께 삭제됨)
    if (State->World)
    {
        ObjectFactory::DeleteObject(State->World);
        State->World = nullptr;
    }

    // State 자체 삭제
    delete State;
    State = nullptr;
}
