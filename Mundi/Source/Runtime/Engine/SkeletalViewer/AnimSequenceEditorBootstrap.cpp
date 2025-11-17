#include "pch.h"
#include "AnimSequenceEditorBootstrap.h"
#include "CameraActor.h"
#include "AnimSequenceEditorState.h"
#include "FViewport.h"
#include "FSkeletalViewerViewportClient.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "AnimInstance.h"

AnimSequenceEditorState* AnimSequenceEditorBootstrap::CreateEditorState(const char* Name, UWorld* InWorld, ID3D11Device* InDevice)
{
    if (!InDevice) return nullptr;

    AnimSequenceEditorState* State = new AnimSequenceEditorState();
    State->Name = Name ? Name : "AnimEditor";

    // Create preview world
    State->World = NewObject<UWorld>();
    State->World->SetWorldType(EWorldType::PreviewMinimal);  // Set as preview world for memory optimization
    State->World->Initialize();
    State->World->GetRenderSettings().DisableShowFlag(EEngineShowFlags::SF_EditorIcon);

    State->World->GetGizmoActor()->SetSpace(EGizmoSpace::Local);

    // Viewport + client per tab
    State->Viewport = new FViewport();
    State->Viewport->Initialize(0, 0, 1, 1, InDevice);

    auto* Client = new FSkeletalViewerViewportClient();
    Client->SetWorld(State->World);
    Client->SetViewportType(EViewportType::Perspective);
    Client->SetViewMode(EViewMode::VMI_Lit_Phong);
    Client->GetCamera()->SetActorLocation(FVector(3, 0, 2));

    State->Client = Client;
    State->Viewport->SetViewportClient(Client);

    State->World->SetEditorCameraActor(Client->GetCamera());

    // Spawn a persistent preview actor for animation playback
    if (State->World)
    {
        ASkeletalMeshActor* Preview = State->World->SpawnActor<ASkeletalMeshActor>();
        State->PreviewActor = Preview;
    }

    return State;
}

void AnimSequenceEditorBootstrap::DestroyEditorState(AnimSequenceEditorState*& State)
{
    if (!State) return;
    if (State->PreviewActor && State->PreviewActor->GetSkeletalMeshComponent() && State->PreviewActor->GetSkeletalMeshComponent()->AnimInstance)
    {
        ObjectFactory::DeleteObject(State->PreviewActor->GetSkeletalMeshComponent()->AnimInstance);
    }
    if (State->Viewport) { delete State->Viewport; State->Viewport = nullptr; }
    if (State->Client) { delete State->Client; State->Client = nullptr; }
    if (State->World) { ObjectFactory::DeleteObject(State->World); State->World = nullptr; }
    delete State; State = nullptr;
}
