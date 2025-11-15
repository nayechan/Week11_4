#include "pch.h"
#include "SAnimSequenceViewerWindow.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Source/Runtime/Renderer/FSkeletalViewerViewportClient.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Animation/AnimSequence.h"
#include "Source/Runtime/Engine/Animation/AnimSingleNodeInstance.h"
#include "Source/Runtime/Engine/Animation/AnimationTypes.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"
#include "Source/Editor/FBXLoader.h"
#include "Source/Editor/PlatformProcess.h"
#include "USlateManager.h"
#include "ResourceManager.h"
#include "GlobalConsole.h"
#include <fstream>

SAnimSequenceViewerWindow::SAnimSequenceViewerWindow()
{
    ViewportRect = FRect(0, 0, 0, 0);
    TimelineRect = FRect(0, 0, 0, 0);
    NotifyTrackRect = FRect(0, 0, 0, 0);
}

SAnimSequenceViewerWindow::~SAnimSequenceViewerWindow()
{
    // Cleanup viewport and client
    if (ViewportClient)
    {
        delete ViewportClient;
        ViewportClient = nullptr;
    }

    if (Viewport)
    {
        delete Viewport;
        Viewport = nullptr;
    }

    // Cleanup preview actor - just null it, World will clean it up
    PreviewActor = nullptr;
    PreviewComponent = nullptr;
}

bool SAnimSequenceViewerWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice)
{
    World = InWorld;
    Device = InDevice;

    SetRect(StartX, StartY, StartX + Width, StartY + Height);

    // Create viewport and viewport client
    ViewportClient = new FSkeletalViewerViewportClient();
    ViewportClient->SetWorld(World);
    ViewportClient->SetViewportType(EViewportType::Perspective);

    Viewport = new FViewport();
    Viewport->Initialize((float)StartX, (float)StartY, (float)Width, (float)Height, Device);
    Viewport->SetViewportClient(ViewportClient);

    // Create preview actor
    if (World)
    {
        PreviewActor = World->SpawnActor<ASkeletalMeshActor>();
        if (PreviewActor)
        {
            PreviewComponent = PreviewActor->GetSkeletalMeshComponent();

            // Set animation mode to single node
            if (PreviewComponent)
            {
                PreviewComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            }
        }
    }

    bRequestFocus = true;
    return true;
}

void SAnimSequenceViewerWindow::OnRender()
{
    // If window is closed, don't render
    if (!bIsOpen)
    {
        return;
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

    if (!bInitialPlacementDone)
    {
        ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
        ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
        bInitialPlacementDone = true;
    }

    if (bRequestFocus)
    {
        ImGui::SetNextWindowFocus();
    }

    if (ImGui::Begin("Animation Sequence Viewer", &bIsOpen, flags))
    {
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        Rect.Left = pos.x;
        Rect.Top = pos.y;
        Rect.Right = pos.x + size.x;
        Rect.Bottom = pos.y + size.y;
        Rect.UpdateMinMax();

        ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        float totalWidth = contentAvail.x;
        float totalHeight = contentAvail.y;

        // === Top Panel: Asset Browser ===
        RenderAssetBrowser();

        ImGui::Separator();

        // === Center Panel: Viewport ===
        float viewportHeight = totalHeight - TopPanelHeight - BottomPanelHeight - 40.0f; // 40 for spacing
        RenderViewportPanel(viewportHeight);

        ImGui::Separator();

        // === Bottom Panel: Timeline Controls & Notify Track ===
        ImGui::BeginChild("BottomPanel", ImVec2(0, BottomPanelHeight), false);

        RenderTimelineControls();
        ImGui::Spacing();
        RenderTimelineScrubber();
        ImGui::Spacing();
        RenderNotifyTrack();

        if (SelectedNotifyIndex >= 0)
        {
            ImGui::Separator();
            RenderNotifyProperties();
        }

        ImGui::EndChild();
    }
    ImGui::End();

    // If window was closed via X button
    if (!bIsOpen)
    {
        USlateManager::GetInstance().CloseAnimSequenceViewer();
    }

    bRequestFocus = false;
}

void SAnimSequenceViewerWindow::OnUpdate(float DeltaSeconds)
{
    // Update world
    if (World)
    {
        World->Tick(DeltaSeconds);
    }

    // Update viewport client
    if (ViewportClient)
    {
        ViewportClient->Tick(DeltaSeconds);
    }

    // Update animation time from AnimInstance
    if (bIsPlaying && CurrentAnimation && PreviewComponent)
    {
        if (UAnimSingleNodeInstance* AnimInstance = Cast<UAnimSingleNodeInstance>(PreviewComponent->AnimInstance))
        {
            CurrentTime = AnimInstance->GetCurrentTime();

            // Check if animation finished (non-looping)
            if (!AnimInstance->IsPlaying())
            {
                bIsPlaying = false;
            }
        }
    }
}

void SAnimSequenceViewerWindow::OnMouseMove(FVector2D MousePos)
{
    if (!Viewport) return;

    if (ViewportRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(ViewportRect.Left, ViewportRect.Top);
        Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);
    }
}

void SAnimSequenceViewerWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
    if (!Viewport) return;

    if (ViewportRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(ViewportRect.Left, ViewportRect.Top);
        Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
    }
}

void SAnimSequenceViewerWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
    if (!Viewport) return;

    if (ViewportRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(ViewportRect.Left, ViewportRect.Top);
        Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
    }
}

void SAnimSequenceViewerWindow::OnRenderViewport()
{
    if (Viewport && ViewportRect.GetWidth() > 0 && ViewportRect.GetHeight() > 0)
    {
        const uint32 NewStartX = static_cast<uint32>(ViewportRect.Left);
        const uint32 NewStartY = static_cast<uint32>(ViewportRect.Top);
        const uint32 NewWidth = static_cast<uint32>(ViewportRect.Right - ViewportRect.Left);
        const uint32 NewHeight = static_cast<uint32>(ViewportRect.Bottom - ViewportRect.Top);

        Viewport->Resize(NewStartX, NewStartY, NewWidth, NewHeight);
        Viewport->Render();
    }
}

// ========================================
// Asset Browser
// ========================================
void SAnimSequenceViewerWindow::RenderAssetBrowser()
{
    ImGui::BeginChild("AssetBrowser", ImVec2(0, TopPanelHeight), false);

    ImGui::Text("Animation Asset");
    ImGui::Spacing();

    // Animation path input
    ImGui::PushItemWidth(-150.0f);
    ImGui::InputTextWithHint("##AnimPath", "Browse for FBX animation file...", AnimPathBuffer, sizeof(AnimPathBuffer));
    ImGui::PopItemWidth();

    ImGui::SameLine();

    // Browse button
    if (ImGui::Button("Browse...", ImVec2(70, 0)))
    {
        auto widePath = FPlatformProcess::OpenLoadFileDialog(UTF8ToWide(GDataDir), L"fbx", L"FBX Files");
        if (!widePath.empty())
        {
            std::string s = widePath.string();
            strncpy_s(AnimPathBuffer, s.c_str(), sizeof(AnimPathBuffer) - 1);
        }
    }

    ImGui::SameLine();

    // Load button
    if (ImGui::Button("Load", ImVec2(70, 0)))
    {
        FString Path = AnimPathBuffer;
        if (!Path.empty())
        {
            LoadAnimation(Path);
        }
    }

    ImGui::SameLine();

    // Save button
    if (CurrentAnimation)
    {
        if (ImGui::Button("Save", ImVec2(70, 0)))
        {
            SaveAnimation();
        }
    }

    // Display current animation info
    if (CurrentAnimation)
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Loaded: %s", LoadedAnimPath.c_str());
        ImGui::Text("Length: %.2fs", AnimationLength);
        ImGui::Text("Frames: %d", CurrentAnimation->NumberOfFrames);
        ImGui::Text("Frame Rate: %.2f fps", CurrentAnimation->FrameRate.AsDecimal());
        ImGui::Text("Notifies: %d", CurrentAnimation->Notifies.Num());
    }
    else
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No animation loaded.");
    }

    ImGui::EndChild();
}

// ========================================
// Viewport Panel
// ========================================
void SAnimSequenceViewerWindow::RenderViewportPanel(float Height)
{
    ImGui::BeginChild("ViewportPanel", ImVec2(0, Height), true, ImGuiWindowFlags_NoScrollbar);

    ImVec2 childPos = ImGui::GetWindowPos();
    ImVec2 childSize = ImGui::GetWindowSize();
    ImVec2 rectMin = childPos;
    ImVec2 rectMax(childPos.x + childSize.x, childPos.y + childSize.y);

    ViewportRect.Left = rectMin.x;
    ViewportRect.Top = rectMin.y;
    ViewportRect.Right = rectMax.x;
    ViewportRect.Bottom = rectMax.y;
    ViewportRect.UpdateMinMax();

    ImGui::EndChild();
}

// ========================================
// Timeline Controls
// ========================================
void SAnimSequenceViewerWindow::RenderTimelineControls()
{
    ImGui::BeginGroup();

    // Play/Pause button
    if (bIsPlaying)
    {
        if (ImGui::Button("Pause", ImVec2(60, 30)))
        {
            PauseAnimation();
        }
    }
    else
    {
        if (ImGui::Button("Play", ImVec2(60, 30)))
        {
            PlayAnimation();
        }
    }

    ImGui::SameLine();

    // Stop button
    if (ImGui::Button("Stop", ImVec2(60, 30)))
    {
        StopAnimation();
    }

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(20, 0));
    ImGui::SameLine();

    // Step backward
    if (ImGui::Button("<", ImVec2(30, 30)))
    {
        StepBackward();
    }

    ImGui::SameLine();

    // Step forward
    if (ImGui::Button(">", ImVec2(30, 30)))
    {
        StepForward();
    }

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(20, 0));
    ImGui::SameLine();

    // Play rate
    ImGui::Text("Speed:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::DragFloat("##PlayRate", &PlayRate, 0.05f, 0.1f, 5.0f, "%.2fx"))
    {
        if (PreviewComponent && PreviewComponent->AnimInstance)
        {
            Cast<UAnimSingleNodeInstance>(PreviewComponent->AnimInstance)->SetPlayRate(PlayRate);
        }
    }

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(20, 0));
    ImGui::SameLine();

    // Loop checkbox
    if (ImGui::Checkbox("Loop", &bLooping))
    {
        // If already playing, restart with new loop setting
        if (bIsPlaying && PreviewComponent && PreviewComponent->AnimInstance)
        {
            Cast<UAnimSingleNodeInstance>(PreviewComponent->AnimInstance)->Play(bLooping);
        }
    }

    ImGui::EndGroup();

    // Frame/Time info
    if (CurrentAnimation)
    {
        int32 CurrentFrame = CurrentAnimation->FrameRate.AsFrameNumber(CurrentTime);
        int32 TotalFrames = CurrentAnimation->NumberOfFrames;

        ImGui::Text("Frame: %d / %d", CurrentFrame, TotalFrames);
        ImGui::SameLine();
        ImGui::Text("  |  Time: %.2fs / %.2fs", CurrentTime, AnimationLength);
    }
}

// ========================================
// Timeline Scrubber
// ========================================
void SAnimSequenceViewerWindow::RenderTimelineScrubber()
{
    if (!CurrentAnimation)
        return;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0.8f, 0.4f, 1.0f));

    float sliderTime = CurrentTime;
    if (ImGui::SliderFloat("##Timeline", &sliderTime, 0.0f, AnimationLength, "%.3fs"))
    {
        SeekToTime(sliderTime);
    }

    ImGui::PopStyleColor(3);

    // Save timeline rect for notify rendering
    TimelineRect = FRect(
        ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y,
        ImGui::GetItemRectMax().x, ImGui::GetItemRectMax().y
    );
    TimelineRect.UpdateMinMax();
}

// ========================================
// Notify Track Rendering
// ========================================
void SAnimSequenceViewerWindow::RenderNotifyTrack()
{
    if (!CurrentAnimation)
        return;

    ImGui::Text("Anim Notifies:");

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    ImVec2 TrackMin = ImGui::GetCursorScreenPos();
    ImVec2 TrackSize(ImGui::GetContentRegionAvail().x, 60);
    ImVec2 TrackMax(TrackMin.x + TrackSize.x, TrackMin.y + TrackSize.y);

    // Track background
    DrawList->AddRectFilled(TrackMin, TrackMax, IM_COL32(30, 30, 35, 255));
    DrawList->AddRect(TrackMin, TrackMax, IM_COL32(60, 60, 70, 255));

    // Render notify markers
    TArray<FAnimNotifyEvent>& Notifies = CurrentAnimation->Notifies;
    for (int32 i = 0; i < Notifies.Num(); ++i)
    {
        const FAnimNotifyEvent& Notify = Notifies[i];

        // Time to pixel position
        float NormalizedTime = (AnimationLength > 0.0f) ? (Notify.TriggerTime / AnimationLength) : 0.0f;
        float XPos = TrackMin.x + (TrackSize.x * NormalizedTime);

        if (Notify.Duration > 0.0f)
        {
            // Duration notify - render as rect
            float EndTime = Notify.TriggerTime + Notify.Duration;
            float NormalizedEndTime = (AnimationLength > 0.0f) ? (EndTime / AnimationLength) : 0.0f;
            float XEndPos = TrackMin.x + (TrackSize.x * NormalizedEndTime);

            ImVec2 RectMin(XPos, TrackMin.y + 10);
            ImVec2 RectMax(XEndPos, TrackMax.y - 10);

            ImU32 Color = (SelectedNotifyIndex == i)
                ? IM_COL32(100, 200, 255, 200)
                : IM_COL32(80, 150, 200, 150);

            DrawList->AddRectFilled(RectMin, RectMax, Color, 4.0f);
            DrawList->AddRect(RectMin, RectMax, IM_COL32(255, 255, 255, 255), 4.0f, 0, 2.0f);

            // Text
            DrawList->AddText(ImVec2(XPos + 5, TrackMin.y + 15),
                IM_COL32(255, 255, 255, 255),
                Notify.NotifyName.ToString().c_str());
        }
        else
        {
            // Instant notify - render as diamond
            ImVec2 Center(XPos, TrackMin.y + TrackSize.y * 0.5f);
            float Size = 8.0f;

            ImU32 Color = (SelectedNotifyIndex == i)
                ? IM_COL32(255, 180, 50, 255)
                : IM_COL32(200, 140, 40, 255);

            ImVec2 Points[4] = {
                ImVec2(Center.x, Center.y - Size),       // Top
                ImVec2(Center.x + Size, Center.y),       // Right
                ImVec2(Center.x, Center.y + Size),       // Bottom
                ImVec2(Center.x - Size, Center.y)        // Left
            };

            DrawList->AddConvexPolyFilled(Points, 4, Color);
            DrawList->AddPolyline(Points, 4, IM_COL32(255, 255, 255, 255), true, 2.0f);

            // Text
            DrawList->AddText(ImVec2(XPos + 10, Center.y - 8),
                IM_COL32(255, 255, 255, 255),
                Notify.NotifyName.ToString().c_str());
        }

        // Mouse interaction for selection
        ImVec2 MousePos = ImGui::GetIO().MousePos;
        if (ImGui::IsMouseClicked(0))
        {
            float MouseXDist = std::abs(MousePos.x - XPos);
            if (MouseXDist < 10.0f && MousePos.y >= TrackMin.y && MousePos.y <= TrackMax.y)
            {
                SelectedNotifyIndex = i;
            }
        }
    }

    // Save notify track rect
    NotifyTrackRect = FRect(TrackMin.x, TrackMin.y, TrackMax.x, TrackMax.y);
    NotifyTrackRect.UpdateMinMax();

    // Dummy to reserve space
    ImGui::Dummy(ImVec2(0, TrackSize.y));

    // Add/Remove buttons
    if (ImGui::Button("+ Add Notify"))
    {
        AddNotify(CurrentTime, FName("NewNotify"));
    }

    ImGui::SameLine();

    if (SelectedNotifyIndex >= 0 && ImGui::Button("- Remove Notify"))
    {
        RemoveNotify(SelectedNotifyIndex);
        SelectedNotifyIndex = -1;
    }
}

// ========================================
// Notify Properties Panel
// ========================================
void SAnimSequenceViewerWindow::RenderNotifyProperties()
{
    if (SelectedNotifyIndex < 0 || !CurrentAnimation)
        return;

    if (SelectedNotifyIndex >= CurrentAnimation->Notifies.Num())
    {
        SelectedNotifyIndex = -1;
        return;
    }

    FAnimNotifyEvent& Notify = CurrentAnimation->Notifies[SelectedNotifyIndex];

    ImGui::Text("Selected Notify Properties");
    ImGui::Spacing();

    // Name
    strncpy_s(NotifyNameBuffer, Notify.NotifyName.ToString().c_str(), sizeof(NotifyNameBuffer) - 1);
    if (ImGui::InputText("Name", NotifyNameBuffer, sizeof(NotifyNameBuffer)))
    {
        Notify.NotifyName = FName(NotifyNameBuffer);
    }

    // Trigger time
    float TriggerTime = Notify.TriggerTime;
    if (ImGui::DragFloat("Trigger Time", &TriggerTime, 0.01f, 0.0f, AnimationLength))
    {
        Notify.TriggerTime = FMath::Clamp(TriggerTime, 0.0f, AnimationLength);
    }

    // Duration
    float Duration = Notify.Duration;
    if (ImGui::DragFloat("Duration", &Duration, 0.01f, 0.0f, AnimationLength))
    {
        Notify.Duration = FMath::Max(0.0f, Duration);
    }
}

// ========================================
// Animation Loading
// ========================================
void SAnimSequenceViewerWindow::LoadAnimation(const FString& Path)
{
    if (Path.empty() || !PreviewActor || !PreviewComponent)
        return;

    // Try to load mesh first if not already loaded
    if (!PreviewComponent->GetSkeletalMesh())
    {
        UE_LOG("No skeletal mesh loaded. Attempting to load mesh from same FBX file...");
        USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
        if (Mesh && PreviewActor)
        {
            PreviewActor->SetSkeletalMesh(Path);
            UE_LOG("Auto-loaded skeletal mesh from: %s", Path.c_str());
        }
    }

    // Get target skeleton
    const FSkeleton* TargetSkeleton = nullptr;
    if (PreviewComponent->GetSkeletalMesh())
    {
        TargetSkeleton = PreviewComponent->GetSkeletalMesh()->GetSkeleton();
    }

    // Load animation
    UAnimSequence* Anim = UFbxLoader::GetInstance().LoadFbxAnimation(Path, TargetSkeleton);

    if (Anim)
    {
        CurrentAnimation = Anim;
        LoadedAnimPath = Path;
        AnimationLength = Anim->GetPlayLength();

        // Set animation on component
        if (PreviewComponent)
        {
            PreviewComponent->PlayAnimation(Anim, bLooping);

            // Update play rate
            if (UAnimSingleNodeInstance* AnimInstance = Cast<UAnimSingleNodeInstance>(PreviewComponent->AnimInstance))
            {
                AnimInstance->SetPlayRate(PlayRate);
            }
        }

        // Reset time
        CurrentTime = 0.0f;
        bIsPlaying = false;

        UE_LOG("SAnimSequenceViewerWindow: Loaded animation from %s", Path.c_str());
        UE_LOG("Animation: %.2fs, %d frames, %d notifies", AnimationLength, Anim->NumberOfFrames, Anim->Notifies.Num());
    }
    else
    {
        UE_LOG("SAnimSequenceViewerWindow: Failed to load animation from %s", Path.c_str());
    }
}

// ========================================
// Playback Control Functions
// ========================================
void SAnimSequenceViewerWindow::PlayAnimation()
{
    if (!PreviewComponent || !CurrentAnimation)
        return;

    if (UAnimSingleNodeInstance* AnimInstance = Cast<UAnimSingleNodeInstance>(PreviewComponent->AnimInstance))
    {
        AnimInstance->Play(bLooping);
        bIsPlaying = true;
    }
}

void SAnimSequenceViewerWindow::PauseAnimation()
{
    if (!PreviewComponent)
        return;

    if (UAnimSingleNodeInstance* AnimInstance = Cast<UAnimSingleNodeInstance>(PreviewComponent->AnimInstance))
    {
        AnimInstance->Pause();
        bIsPlaying = false;
    }
}

void SAnimSequenceViewerWindow::StopAnimation()
{
    if (!PreviewComponent)
        return;

    if (UAnimSingleNodeInstance* AnimInstance = Cast<UAnimSingleNodeInstance>(PreviewComponent->AnimInstance))
    {
        AnimInstance->Stop();
        bIsPlaying = false;
        CurrentTime = 0.0f;
    }
}

void SAnimSequenceViewerWindow::SeekToTime(float Time)
{
    if (!PreviewComponent || !CurrentAnimation)
        return;

    CurrentTime = FMath::Clamp(Time, 0.0f, AnimationLength);

    if (UAnimSingleNodeInstance* AnimInstance = Cast<UAnimSingleNodeInstance>(PreviewComponent->AnimInstance))
    {
        AnimInstance->SetCurrentTime(CurrentTime);

        // Force pose update
        PreviewComponent->ForceRecomputePose();
    }
}

void SAnimSequenceViewerWindow::SeekToFrame(int32 Frame)
{
    if (!CurrentAnimation)
        return;

    float Time = CurrentAnimation->FrameRate.AsSeconds(Frame);
    SeekToTime(Time);
}

void SAnimSequenceViewerWindow::StepForward()
{
    if (!CurrentAnimation)
        return;

    int32 CurrentFrame = CurrentAnimation->FrameRate.AsFrameNumber(CurrentTime);
    int32 NextFrame = FMath::Min(CurrentFrame + 1, CurrentAnimation->NumberOfFrames - 1);
    SeekToFrame(NextFrame);
}

void SAnimSequenceViewerWindow::StepBackward()
{
    if (!CurrentAnimation)
        return;

    int32 CurrentFrame = CurrentAnimation->FrameRate.AsFrameNumber(CurrentTime);
    int32 PrevFrame = FMath::Max(0, CurrentFrame - 1);
    SeekToFrame(PrevFrame);
}

// ========================================
// Notify Editing Functions
// ========================================
void SAnimSequenceViewerWindow::AddNotify(float Time, const FName& NotifyName)
{
    if (!CurrentAnimation)
        return;

    FAnimNotifyEvent NewNotify;
    NewNotify.TriggerTime = FMath::Clamp(Time, 0.0f, AnimationLength);
    NewNotify.Duration = 0.0f;
    NewNotify.NotifyName = NotifyName;

    CurrentAnimation->Notifies.Add(NewNotify);
    SelectedNotifyIndex = CurrentAnimation->Notifies.Num() - 1;

    UE_LOG("Added notify '%s' at time %.2f", NotifyName.ToString().c_str(), Time);
}

void SAnimSequenceViewerWindow::RemoveNotify(int32 Index)
{
    if (!CurrentAnimation)
        return;

    if (Index >= 0 && Index < CurrentAnimation->Notifies.Num())
    {
        CurrentAnimation->Notifies.RemoveAt(Index);
        UE_LOG("Removed notify at index %d", Index);
    }
}

void SAnimSequenceViewerWindow::UpdateNotifyTime(int32 Index, float NewTime)
{
    if (!CurrentAnimation)
        return;

    if (Index >= 0 && Index < CurrentAnimation->Notifies.Num())
    {
        CurrentAnimation->Notifies[Index].TriggerTime = FMath::Clamp(NewTime, 0.0f, AnimationLength);
    }
}

// ========================================
// Helper Functions
// ========================================
float SAnimSequenceViewerWindow::TimeToPixel(float Time) const
{
    if (AnimationLength <= 0.0f)
        return TimelineRect.Left;

    float NormalizedTime = Time / AnimationLength;
    return TimelineRect.Left + (TimelineRect.GetWidth() * NormalizedTime);
}

float SAnimSequenceViewerWindow::PixelToTime(float PixelX) const
{
    if (TimelineRect.GetWidth() <= 0.0f)
        return 0.0f;

    float NormalizedX = (PixelX - TimelineRect.Left) / TimelineRect.GetWidth();
    return FMath::Clamp(NormalizedX * AnimationLength, 0.0f, AnimationLength);
}

// ========================================
// Save Animation
// ========================================
void SAnimSequenceViewerWindow::SaveAnimation()
{
    if (!CurrentAnimation || LoadedAnimPath.empty())
    {
        UE_LOG("Cannot save animation: No animation loaded");
        return;
    }

    // Save as JSON
    FString JsonPath = LoadedAnimPath;
    // Replace .fbx with .anim.json
    size_t dotPos = JsonPath.rfind('.');
    if (dotPos != std::string::npos)
    {
        JsonPath = JsonPath.substr(0, dotPos) + ".anim.json";
    }
    else
    {
        JsonPath += ".anim.json";
    }

    // Serialize to JSON
    JSON JsonData;
    CurrentAnimation->Serialize(false, JsonData);

    // Write to file
    std::ofstream OutFile(JsonPath);
    if (OutFile.is_open())
    {
        OutFile << JsonData.dump(4); // Pretty print with 4 spaces
        OutFile.close();
        UE_LOG("Animation saved to: %s", JsonPath.c_str());
    }
    else
    {
        UE_LOG("Failed to save animation to: %s", JsonPath.c_str());
    }
}
