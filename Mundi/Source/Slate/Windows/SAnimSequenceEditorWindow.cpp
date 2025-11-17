#include "pch.h"
#include "SAnimSequenceEditorWindow.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Source/Runtime/Engine/SkeletalViewer/AnimSequenceEditorBootstrap.h"
#include "Source/Runtime/Engine/SkeletalViewer/AnimSequenceEditorState.h"
#include "Source/Editor/PlatformProcess.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "SelectionManager.h"
#include "USlateManager.h"
#include "BoneAnchorComponent.h"
#include "Source/Runtime/Engine/Collision/Picking.h"
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "FBXLoader.h"
#include "AnimInstance.h"
#include "AnimSingleNodeInstance.h"
#include "AnimSequence.h"
#include "AnimSequenceBase.h"
#include <filesystem>
#include <fstream>

SAnimSequenceEditorWindow::SAnimSequenceEditorWindow()
{
    CenterRect = FRect(0, 0, 0, 0);
}

SAnimSequenceEditorWindow::~SAnimSequenceEditorWindow()
{
    // Clean up tabs if any
    for (int i = 0; i < Tabs.Num(); ++i)
    {
        AnimSequenceEditorState* State = Tabs[i];
        AnimSequenceEditorBootstrap::DestroyEditorState(State);
    }
    Tabs.Empty();
    ActiveState = nullptr;
}

bool SAnimSequenceEditorWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice)
{
    World = InWorld;
    Device = InDevice;

    IconPause = UResourceManager::GetInstance().Load<UTexture>(GDataDir + "/Icon/Pause.png");
    IconResume = UResourceManager::GetInstance().Load<UTexture>(GDataDir + "/Icon/Resume.png");

    SetRect(StartX, StartY, StartX + Width, StartY + Height);

    // Create first tab/state
    OpenNewTab("AnimEditor 1");
    if (ActiveState && ActiveState->Viewport)
    {
        ActiveState->Viewport->Resize((uint32)StartX, (uint32)StartY, (uint32)Width, (uint32)Height);
    }

    bRequestFocus = true;
    return true;
}

void SAnimSequenceEditorWindow::OpenNewTab(const char* Label)
{
    AnimSequenceEditorState* NewState = AnimSequenceEditorBootstrap::CreateEditorState(Label, World, Device);
    if (NewState)
    {
        Tabs.Add(NewState);
        ActiveState = NewState;
        ActiveTabIndex = Tabs.Num() - 1;
    }
}

void SAnimSequenceEditorWindow::CloseTab(int Index)
{
    if (Index < 0 || Index >= Tabs.Num())
        return;

    AnimSequenceEditorState* State = Tabs[Index];
    AnimSequenceEditorBootstrap::DestroyEditorState(State);
    Tabs.RemoveAt(Index);

    if (Tabs.Num() == 0)
    {
        ActiveState = nullptr;
        ActiveTabIndex = -1;
        bIsOpen = false;
    }
    else
    {
        if (ActiveTabIndex >= Tabs.Num())
            ActiveTabIndex = Tabs.Num() - 1;
        ActiveState = Tabs[ActiveTabIndex];
    }
}

void SAnimSequenceEditorWindow::OnRender()
{
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
        bRequestFocus = false;
    }

    if (ImGui::Begin("Animation Sequence Editor", &bIsOpen, flags))
    {
        // Render tab bar
        if (ImGui::BeginTabBar("AnimEditorTabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
        {
            for (int i = 0; i < Tabs.Num(); ++i)
            {
                AnimSequenceEditorState* State = Tabs[i];
                bool open = true;
                if (ImGui::BeginTabItem(State->Name.ToString().c_str(), &open))
                {
                    ActiveTabIndex = i;
                    ActiveState = State;
                    ImGui::EndTabItem();
                }
                if (!open)
                {
                    CloseTab(i);
                    break;
                }
            }
            if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
            {
                char label[32];
                sprintf_s(label, "AnimEditor %d", Tabs.Num() + 1);
                OpenNewTab(label);
            }
            ImGui::EndTabBar();
        }

        // Always render the animation sequence viewer UI
        if (ActiveState)
        {
            RenderAnimationSquenceViewer();
        }
        else
        {
            ImGui::Text("No active editor state.");
        }
    }
    ImGui::End();
}

void SAnimSequenceEditorWindow::RenderAnimationSquenceViewer()
{
    // Get AnimInstance if available
    UAnimInstance* AnimInstance = nullptr;
    UAnimSingleNodeInstance* AnimSingleNodeInstance = nullptr;
    UAnimSequence* AnimSequence = nullptr;

    if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkeletalMeshComponent())
    {
        AnimInstance = ActiveState->PreviewActor->GetSkeletalMeshComponent()->AnimInstance;
        if (AnimInstance)
        {
            AnimSingleNodeInstance = static_cast<UAnimSingleNodeInstance*>(AnimInstance);
            AnimSequence = AnimSingleNodeInstance->GetAnimSequence();
        }
    }

    // Keyboard input handling
    if (AnimSingleNodeInstance)
    {
        // Space key: Toggle play/pause
        if (ImGui::IsKeyPressed(ImGuiKey_Space))
        {
            if (AnimSingleNodeInstance->IsPlaying())
            {
                AnimSingleNodeInstance->Pause();
            }
            else
            {
                AnimSingleNodeInstance->Play(ActiveState->bLoopAnimation);
            }
        }
    }

    ImVec2 contentAvail = ImGui::GetContentRegionAvail();
    float totalWidth = contentAvail.x;
    float totalHeight = contentAvail.y;

    float leftWidth = totalWidth * LeftPanelRatio;
    float rightWidth = totalWidth * RightPanelRatio;
    float bottomHeight = totalHeight * BottomPanelRatio;
    float centerWidth = totalWidth - leftWidth - rightWidth;
    float centerHeight = totalHeight - bottomHeight;

    centerWidth = FMath::Max(centerWidth, 1.0f);
    centerHeight = FMath::Max(centerHeight, 1.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    // === LEFT PANEL: Asset Browser ===
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, totalHeight), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    // Asset Browser Header
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.35f, 0.50f, 0.8f));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
    ImGui::Indent(8.0f);
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::Text("Asset Browser");
    ImGui::PopFont();
    ImGui::Unindent(8.0f);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Mesh Path Section
    ImGui::BeginGroup();
    ImGui::Text("Mesh Path:");
    ImGui::PushItemWidth(-1.0f);
    ImGui::InputTextWithHint("##MeshPath", "Browse for FBX file...", ActiveState->MeshPathBuffer, sizeof(ActiveState->MeshPathBuffer));
    ImGui::PopItemWidth();

    ImGui::Spacing();

    // Buttons
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.40f, 0.55f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.50f, 0.70f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.35f, 0.50f, 1.0f));

    float buttonWidth = (leftWidth - 24.0f) * 0.5f - 4.0f;
    if (ImGui::Button("Browse...##MeshBrowse", ImVec2(buttonWidth, 32)))
    {
        auto widePath = FPlatformProcess::OpenLoadFileDialog(UTF8ToWide(GDataDir), L"fbx", L"FBX Files");
        if (!widePath.empty())
        {
            std::string s = widePath.string();
            strncpy_s(ActiveState->MeshPathBuffer, s.c_str(), sizeof(ActiveState->MeshPathBuffer) - 1);
        }
    }

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.60f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.70f, 0.50f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.50f, 0.30f, 1.0f));
    if (ImGui::Button("Load FBX", ImVec2(buttonWidth, 32)))
    {
        FString Path = ActiveState->MeshPathBuffer;
        if (!Path.empty())
        {
            LoadSkeletalMesh(Path);
        }
    }
    ImGui::PopStyleColor(6);
    ImGui::EndGroup();

    ImGui::Spacing();

    // Animation Path Section
    ImGui::BeginGroup();
    ImGui::Text("Animation Path:");
    ImGui::PushItemWidth(-1.0f);
    ImGui::InputTextWithHint("##AnimPath", "Browse for FBX file...", ActiveState->AnimationPathBuffer, sizeof(ActiveState->AnimationPathBuffer));
    ImGui::PopItemWidth();

    ImGui::Spacing();

    // Animation Buttons
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.40f, 0.55f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.50f, 0.70f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.35f, 0.50f, 1.0f));

    if (ImGui::Button("Browse...##AnimBrowse", ImVec2(buttonWidth, 32)))
    {
        auto widePath = FPlatformProcess::OpenLoadFileDialog(UTF8ToWide(GDataDir), L"fbx", L"FBX Files");
        if (!widePath.empty())
        {
            std::string s = widePath.string();
            strncpy_s(ActiveState->AnimationPathBuffer, s.c_str(), sizeof(ActiveState->AnimationPathBuffer) - 1);
        }
    }

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.60f, 0.40f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.50f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.50f, 0.30f, 0.15f, 1.0f));
    if (ImGui::Button("Load Anim", ImVec2(buttonWidth, 32)))
    {
        FString Path = ActiveState->AnimationPathBuffer;
        if (!Path.empty())
        {
            LoadAnimationFile(Path);
        }
    }
    ImGui::PopStyleColor(6);
    ImGui::EndGroup();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Status Info
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.8f, 0.9f, 1.0f));
    if (ActiveState->CurrentMesh)
    {
        ImGui::Text("Mesh: Loaded");
    }
    else
    {
        ImGui::TextDisabled("Mesh: None");
    }

    if (AnimSequence)
    {
        ImGui::Text("Anim: %s", AnimSequence->GetName().c_str());
        ImGui::Text("Length: %.2fs", AnimSequence->GetPlayLength());
        ImGui::Text("Frames: %d", AnimSequence->NumberOfFrames);
    }
    else
    {
        ImGui::TextDisabled("Anim: None");
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Display Options
    ImGui::Text("Display Options");
    ImGui::Spacing();

    // Mesh visibility
    if (ImGui::Checkbox("Show Mesh", &ActiveState->bShowMesh))
    {
        if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkeletalMeshComponent())
        {
            ActiveState->PreviewActor->GetSkeletalMeshComponent()->SetVisibility(ActiveState->bShowMesh);
        }
    }

    // Bones visibility
    if (ImGui::Checkbox("Show Bones", &ActiveState->bShowBones))
    {
        if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetBoneLineComponent())
        {
            ActiveState->PreviewActor->GetBoneLineComponent()->SetLineVisible(ActiveState->bShowBones);
        }
    }

    ImGui::EndChild(); // LeftPanel

    ImGui::SameLine(0, 0);

    // === CENTER & RIGHT & BOTTOM AREA ===
    ImGui::BeginChild("CenterRightBottomArea", ImVec2(centerWidth + rightWidth, totalHeight), false, ImGuiWindowFlags_NoScrollbar);

    // Top: Center Viewport + Right Panel
    ImGui::BeginChild("TopArea", ImVec2(centerWidth + rightWidth, centerHeight), false, ImGuiWindowFlags_NoScrollbar);

    // Center Viewport
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::BeginChild("CenterViewport", ImVec2(centerWidth, centerHeight), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    ImVec2 childPos = ImGui::GetWindowPos();
    ImVec2 childSize = ImGui::GetWindowSize();
    CenterRect.Left = childPos.x;
    CenterRect.Top = childPos.y;
    CenterRect.Right = childPos.x + childSize.x;
    CenterRect.Bottom = childPos.y + childSize.y;
    CenterRect.UpdateMinMax();

    // Viewport will be rendered by OnRenderViewport
    // Just update the CenterRect for viewport positioning

    ImGui::EndChild(); // CenterViewport

    ImGui::SameLine(0, 0);

    // Right Panel
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::BeginChild("RightPanel", ImVec2(rightWidth, centerHeight), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    ImGui::TextDisabled("(Reserved for future properties)");

    ImGui::EndChild(); // RightPanel

    ImGui::EndChild(); // TopArea

    // Bottom: Timeline
    if (!AnimSequence || !AnimSingleNodeInstance)
    {
        // No animation loaded, show simple message
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("BottomPanel", ImVec2(centerWidth + rightWidth, bottomHeight), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();

        ImGui::TextDisabled("Load an animation to see the timeline");

        ImGui::EndChild(); // BottomPanel
        ImGui::EndChild(); // CenterRightBottomArea
        ImGui::PopStyleVar(); // ItemSpacing
        return;
    }

    // Animation timing variables
    float CurrentInternalTime = AnimSingleNodeInstance->GetInteralTime();
    const float AnimationLength = AnimSequence->GetPlayLength();
    int CurrentFrame = static_cast<int>((CurrentInternalTime / AnimationLength) * AnimSequence->NumberOfFrames);

    // Timeline Panel
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::BeginChild("TimelinePanel", ImVec2(centerWidth + rightWidth, bottomHeight), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    float WindowWidth = ImGui::GetWindowWidth();
    float WindowHeight = ImGui::GetWindowHeight();

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    // Layout: Left side = Notify tracks + controls, Right side = Timeline
    const float LeftControlWidth = 200.0f;
    const float RightTimelineWidth = WindowWidth - LeftControlWidth;

    // === LEFT SIDE: Notify Tracks + Playback Controls ===
    ImGui::BeginChild("LeftControlArea", ImVec2(LeftControlWidth, WindowHeight), false, ImGuiWindowFlags_NoScrollbar);

    // Top part: Notify Tracks
    float controlsHeight = 80.0f; // Height for playback controls
    float tracksHeight = WindowHeight - controlsHeight - 10.0f;

    ImGui::BeginChild("NotifyTracks", ImVec2(LeftControlWidth, tracksHeight), true);

    // Initialize default track if empty (use AnimSequence->NotifyTracks)
    TArray<UAnimSequenceBase::FNotifyTrack>& NotifyTracks = AnimSequence->NotifyTracks;
    if (NotifyTracks.Num() == 0)
    {
        UAnimSequenceBase::FNotifyTrack DefaultTrack;
        DefaultTrack.ID = AnimSequence->NextTrackID++;
        DefaultTrack.Name = "Track 1";
        NotifyTracks.Add(DefaultTrack);
    }

    TArray<FAnimNotifyEvent>& Notifies = AnimSequence->Notifies;
    static int SelectedNotifyIndex = -1;

    // Header matching timeline
    const float HeaderHeight = 25.0f;
    const float TrackHeight = 28.0f;

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.85f, 0.9f, 1.0f));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5.0f);
    ImGui::Text("Tracks");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, HeaderHeight - 20.0f)); // Spacer to match timeline header

    // Rename Track state (shared between context menu and popup)
    static int RenameTrackID = -1;
    static bool bOpenRenamePopup = false;

    // Edit Notify state
    static bool bOpenEditNotifyPopup = false;

    // Track list with fixed heights matching timeline lanes
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));

    static int DraggedTrackIndex = -1;

    for (int i = 0; i < NotifyTracks.Num(); i++)
    {
        const UAnimSequenceBase::FNotifyTrack& Track = NotifyTracks[i];
        bool isSelected = (Track.ID == ActiveState->SelectedNotifyTrackID);
        char label[128];
        sprintf_s(label, "%s##Track%d", Track.Name.c_str(), Track.ID);

        // Fixed height selectable to match timeline lane
        ImVec2 selectableSize(LeftControlWidth - 20.0f, TrackHeight);
        if (ImGui::Selectable(label, isSelected, 0, selectableSize))
        {
            ActiveState->SelectedNotifyTrackID = Track.ID;
        }

        // Drag source (allow reordering tracks)
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            ImGui::SetDragDropPayload("TRACK_REORDER", &i, sizeof(int));
            ImGui::Text("Moving: %s", Track.Name.c_str());
            DraggedTrackIndex = i;
            ImGui::EndDragDropSource();
        }

        // Drop target (accept dropped track)
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TRACK_REORDER"))
            {
                int sourceIdx = *(const int*)payload->Data;
                int targetIdx = i;

                if (sourceIdx != targetIdx && sourceIdx >= 0 && sourceIdx < NotifyTracks.Num())
                {
                    // Reorder tracks
                    UAnimSequenceBase::FNotifyTrack temp = NotifyTracks[sourceIdx];
                    NotifyTracks.RemoveAt(sourceIdx);
                    NotifyTracks.Insert(temp, targetIdx);
                    DraggedTrackIndex = -1;
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Context menu for track
        ImGui::PushID(Track.ID);
        if (ImGui::BeginPopupContextItem("TrackContextMenu"))
        {
            if (ImGui::MenuItem("Rename Track"))
            {
                RenameTrackID = Track.ID;
                bOpenRenamePopup = true;
            }
            if (ImGui::MenuItem("Delete Track"))
            {
                // Delete all notifies in this track
                for (int j = Notifies.Num() - 1; j >= 0; j--)
                {
                    if (Notifies[j].TrackIndex == Track.ID)
                    {
                        Notifies.RemoveAt(j);
                    }
                }
                // Remove track from array
                NotifyTracks.RemoveAt(i);
                if (ActiveState->SelectedNotifyTrackID == Track.ID)
                {
                    ActiveState->SelectedNotifyTrackID = NotifyTracks.Num() > 0 ? NotifyTracks[0].ID : -1;
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Open rename popup if flagged
    if (bOpenRenamePopup)
    {
        ImGui::OpenPopup("RenameTrackPopup");
        bOpenRenamePopup = false;
    }

    // Rename Track Popup (outside the loop)
    static char RenameBuffer[64] = "";
    if (ImGui::BeginPopup("RenameTrackPopup"))
    {
        ImGui::Text("Rename Track");
        ImGui::Separator();

        if (ImGui::IsWindowAppearing())
        {
            // Find track name by ID
            for (int i = 0; i < NotifyTracks.Num(); i++)
            {
                if (NotifyTracks[i].ID == RenameTrackID)
                {
                    strcpy_s(RenameBuffer, NotifyTracks[i].Name.c_str());
                    break;
                }
            }
        }

        bool bPressedEnter = ImGui::InputText("Name", RenameBuffer, sizeof(RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Spacing();

        if (ImGui::Button("OK", ImVec2(120, 0)) || bPressedEnter)
        {
            // Find and update track by ID
            for (int i = 0; i < NotifyTracks.Num(); i++)
            {
                if (NotifyTracks[i].ID == RenameTrackID)
                {
                    NotifyTracks[i].Name = RenameBuffer;
                    break;
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Add track button
    if (ImGui::Button("Add Track", ImVec2(LeftControlWidth - 20.0f, 0)))
    {
        UAnimSequenceBase::FNotifyTrack NewTrack;
        NewTrack.ID = AnimSequence->NextTrackID++;
        char newTrackName[64];
        sprintf_s(newTrackName, "Track %d", NewTrack.ID);
        NewTrack.Name = newTrackName;
        NotifyTracks.Add(NewTrack);
    }

    ImGui::Spacing();

    // Save animation sequence button (전체 데이터 - 큰 파일)
    if (ImGui::Button("Save Animation (Full)", ImVec2(LeftControlWidth - 20.0f, 0)))
    {
        // Generate save path: original path + "_modified.json"
        FString SavePath = AnimSequence->GetFilePath();
        if (SavePath.empty())
        {
            SavePath = "Data/Animations/modified_anim.json";
        }
        else
        {
            // Replace extension with _modified.json
            size_t lastDot = SavePath.find_last_of('.');
            if (lastDot != std::string::npos)
            {
                SavePath = SavePath.substr(0, lastDot) + "_modified.json";
            }
            else
            {
                SavePath += "_modified.json";
            }
        }

        if (AnimSequence->SaveToFile(SavePath))
        {
            UE_LOG("Animation saved successfully to: %s", SavePath.c_str());
        }
        else
        {
            UE_LOG("Failed to save animation to: %s", SavePath.c_str());
        }
    }

    ImGui::Spacing();

    // Save Notify Data button (Notify만 - 작은 파일, 추천)
    if (ImGui::Button("Save Notify Data", ImVec2(LeftControlWidth - 20.0f, 0)))
    {
        // Generate default filename from animation path
        FString DefaultFileName;
        FString SourcePath = AnimSequence->GetFilePath();
        if (!SourcePath.empty())
        {
            // Extract filename without extension
            size_t lastSlash = SourcePath.find_last_of("/\\");
            size_t lastDot = SourcePath.find_last_of('.');
            if (lastSlash != std::string::npos && lastDot != std::string::npos && lastDot > lastSlash)
            {
                DefaultFileName = SourcePath.substr(lastSlash + 1, lastDot - lastSlash - 1) + ".anim";
            }
            else if (lastSlash != std::string::npos)
            {
                DefaultFileName = SourcePath.substr(lastSlash + 1) + ".anim";
            }
            else
            {
                DefaultFileName = "animation.anim";
            }
        }
        else
        {
            DefaultFileName = "animation.anim";
        }

        // Show Save File Dialog
        OPENFILENAMEA ofn = {};
        char szFile[260] = {};

        // Copy default filename to buffer
        strncpy_s(szFile, DefaultFileName.c_str(), sizeof(szFile) - 1);

        // Get absolute initial directory
        std::filesystem::path InitialDir = std::filesystem::current_path() / "Data" / "Fbx";
        std::string InitialDirStr = InitialDir.string();

        ofn.lStructSize = sizeof(OPENFILENAMEA);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "Animation Notify Files (*.anim)\0*.anim\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = InitialDirStr.c_str();
        ofn.lpstrTitle = "Save Animation Notify Data";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
        ofn.lpstrDefExt = "anim";

        if (GetSaveFileNameA(&ofn) == TRUE)
        {
            FString SavePath = ofn.lpstrFile;
            if (AnimSequence->SaveNotifyData(SavePath))
            {
                UE_LOG("Notify data saved successfully to: %s", SavePath.c_str());
            }
            else
            {
                UE_LOG("Failed to save notify data to: %s", SavePath.c_str());
            }
        }
    }

    // Load Animation from .anim file (애니메이션 + Notify 통합 로드)
    if (ImGui::Button("Load Animation (.anim)", ImVec2(LeftControlWidth - 20.0f, 0)))
    {
        // Show Open File Dialog
        OPENFILENAMEA ofn = {};
        char szFile[260] = {};

        // Get absolute initial directory
        std::filesystem::path InitialDir = std::filesystem::current_path() / "Data" / "Fbx";
        std::string InitialDirStr = InitialDir.string();

        ofn.lStructSize = sizeof(OPENFILENAMEA);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "Animation Notify Files (*.anim)\0*.anim\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = InitialDirStr.c_str();
        ofn.lpstrTitle = "Load Animation from .anim file";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn) == TRUE)
        {
            FString AnimFilePath = ofn.lpstrFile;
            UE_LOG("=== Loading .anim file: %s ===", AnimFilePath.c_str());

            // .anim 파일을 열어서 SourceFilePath 읽기
            try
            {
                UE_LOG("[1/7] Opening .anim file...");
                std::ifstream InFile(AnimFilePath);
                if (InFile.is_open())
                {
                    UE_LOG("[2/7] Reading JSON string...");
                    FString JsonString((std::istreambuf_iterator<char>(InFile)), std::istreambuf_iterator<char>());
                    InFile.close();
                    UE_LOG("[3/7] JSON string size: %zu bytes", JsonString.size());

                    UE_LOG("[4/7] Parsing JSON...");
                    JSON RootJson = JSON::Load(JsonString);
                    UE_LOG("[5/7] JSON parsed successfully");
                    FString SourceFilePath;
                    FJsonSerializer::ReadString(RootJson, "SourceFilePath", SourceFilePath, "", false);

                    UE_LOG("Read SourceFilePath from JSON: %s", SourceFilePath.c_str());

                    if (!SourceFilePath.empty())
                    {
                        // 상대경로를 절대경로로 변환 (상대경로인 경우)
                        std::filesystem::path SourcePath(SourceFilePath);
                        if (SourcePath.is_relative())
                        {
                            // 먼저 현재 작업 디렉토리에서 찾기
                            std::filesystem::path CurrentPath = std::filesystem::current_path();
                            std::filesystem::path AbsPath = CurrentPath / SourcePath;

                            // 파일이 존재하지 않으면, .anim 파일과 같은 디렉토리에서 찾기
                            if (!std::filesystem::exists(AbsPath))
                            {
                                std::filesystem::path AnimFileDir = std::filesystem::path(AnimFilePath).parent_path();
                                AbsPath = AnimFileDir / SourcePath;
                                UE_LOG("File not found in working directory, trying .anim file directory: %s", AbsPath.string().c_str());
                            }

                            SourcePath = AbsPath;
                            SourceFilePath = SourcePath.string();
                            UE_LOG("Converted to absolute path: %s", SourceFilePath.c_str());
                        }

                        // 파일 존재 여부 확인
                        if (std::filesystem::exists(SourceFilePath))
                        {
                            UE_LOG("[6/7] Source FBX file exists, loading...");
                            UE_LOG("Source file path: %s", SourceFilePath.c_str());

                            // .anim의 source mesh와 현재 로드된 mesh가 같은지 확인
                            bool bNeedLoadMesh = true;
                            if (ActiveState->CurrentMesh && !ActiveState->LoadedMeshPath.empty())
                            {
                                // 경로 정규화 후 비교
                                std::filesystem::path CurrentMeshPath = std::filesystem::path(ActiveState->LoadedMeshPath).lexically_normal();
                                std::filesystem::path NewMeshPath = std::filesystem::path(SourceFilePath).lexically_normal();

                                if (CurrentMeshPath == NewMeshPath)
                                {
                                    UE_LOG("Same mesh already loaded, skipping LoadSkeletalMesh");
                                    bNeedLoadMesh = false;
                                }
                                else
                                {
                                    UE_LOG("WARNING: .anim source mesh (%s) differs from current mesh (%s)",
                                           SourceFilePath.c_str(), ActiveState->LoadedMeshPath.c_str());
                                    UE_LOG("Will load the correct mesh for this animation");
                                }
                            }

                            // 다른 메시거나 처음 로드하는 경우에만 메시 로드
                            if (bNeedLoadMesh)
                            {
                                UE_LOG("Loading new skeletal mesh, cleaning up previous state...");

                                // 이전 AnimInstance 제거
                                if (auto* SkeletalComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
                                {
                                    if (SkeletalComp->AnimInstance)
                                    {
                                        UE_LOG("Clearing previous AnimInstance before mesh change");
                                        SkeletalComp->AnimInstance = nullptr;
                                    }
                                }

                                // 이전 상태 초기화
                                ActiveState->CurrentMesh = nullptr;
                                ActiveState->LoadedMeshPath.clear();
                                ActiveState->bBoneLinesDirty = true;

                                // BoneLines 명시적 클리어 (메시 변경 전 이전 본 인덱스 참조 방지)
                                if (auto* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
                                {
                                    LineComp->ClearLines();
                                }

                                UE_LOG("Loading new skeletal mesh...");
                                LoadSkeletalMesh(SourceFilePath);
                                UE_LOG("LoadSkeletalMesh completed");
                            }
                            else
                            {
                                // 같은 메시를 사용하는 경우에도 본 라인 재구축 필요
                                // (애니메이션이 바뀌면 본 포즈가 달라지므로)
                                UE_LOG("Same mesh, but marking bone lines dirty for animation change");
                                if (ActiveState->bShowBones)
                                {
                                    ActiveState->bBoneLinesDirty = true;
                                }
                            }

                            UE_LOG("Getting SkeletalMeshComponent...");
                            USkeletalMeshComponent* SkeletalMeshComponent = ActiveState->PreviewActor->GetSkeletalMeshComponent();
                            if (SkeletalMeshComponent)
                            {
                                UE_LOG("SkeletalMeshComponent found");
                                if (SkeletalMeshComponent->GetSkeletalMesh())
                                {
                                    UE_LOG("SkeletalMesh found, loading animation...");

                                    // 애니메이션 로드
                                    UE_LOG("Calling UFbxLoader::LoadFbxAnimation...");
                                    UAnimSequence* LoadedAnimSequence = UFbxLoader::GetInstance().LoadFbxAnimation(
                                        SourceFilePath,
                                        SkeletalMeshComponent->GetSkeletalMesh()->GetSkeleton()
                                    );
                                    UE_LOG("UFbxLoader::LoadFbxAnimation returned: %p", LoadedAnimSequence);

                                    if (LoadedAnimSequence)
                                    {
                                        UE_LOG("Animation loaded successfully, applying notify data...");

                                        // Notify 데이터 적용
                                        UE_LOG("Calling LoadNotifyData...");
                                        if (LoadedAnimSequence->LoadNotifyData(AnimFilePath))
                                        {
                                            UE_LOG("Notify data applied successfully from: %s", AnimFilePath.c_str());
                                            UE_LOG("Loaded %d notifies, %d tracks", LoadedAnimSequence->Notifies.Num(), LoadedAnimSequence->NotifyTracks.Num());
                                        }
                                        else
                                        {
                                            UE_LOG("Failed to apply notify data");
                                        }

                                        // AnimInstance 설정 - 기존 AnimInstance가 있으면 재사용, 없으면 새로 생성
                                        UE_LOG("Setting up AnimInstance...");

                                        UAnimSingleNodeInstance* SingleNodeInstance = dynamic_cast<UAnimSingleNodeInstance*>(SkeletalMeshComponent->AnimInstance);
                                        if (SingleNodeInstance)
                                        {
                                            // 기존 AnimInstance의 AnimationAsset만 교체
                                            UE_LOG("Reusing existing AnimInstance");
                                            SingleNodeInstance->SetAnimationAsset(LoadedAnimSequence);
                                        }
                                        else
                                        {
                                            // 새로운 AnimInstance 생성
                                            UE_LOG("Creating new AnimInstance");
                                            UAnimSingleNodeInstance* AnimInstance = NewObject<UAnimSingleNodeInstance>();
                                            AnimInstance->SetAnimationAsset(LoadedAnimSequence);
                                            SkeletalMeshComponent->SetAnimInstance(AnimInstance);
                                        }

                                        UE_LOG("[7/7] Animation loaded successfully from .anim file");
                                        UE_LOG("=== Load complete ===");

                                        // Animation Path 업데이트 (Mesh Path 아님!)
                                        strncpy_s(ActiveState->AnimationPathBuffer, SourceFilePath.c_str(), sizeof(ActiveState->AnimationPathBuffer) - 1);
                                    }
                                    else
                                    {
                                        UE_LOG("Failed to load animation from source: %s", SourceFilePath.c_str());
                                    }
                                }
                                else
                                {
                                    UE_LOG("Failed to get SkeletalMesh from component");
                                }
                            }
                            else
                            {
                                UE_LOG("Failed to get SkeletalMeshComponent");
                            }
                        }
                        else
                        {
                            UE_LOG("ERROR: Source FBX file not found: %s", SourceFilePath.c_str());
                            UE_LOG("Please check that the FBX file exists and the path in .anim file is correct");
                        }
                    }
                    else
                    {
                        UE_LOG("No SourceFilePath found in .anim file");
                    }
                }
                else
                {
                    UE_LOG("Failed to open .anim file: %s", AnimFilePath.c_str());
                }
            }
            catch (const std::exception& e)
            {
                UE_LOG("Exception loading .anim file: %s", e.what());
            }
        }
    }

    // Delete key: Remove selected notify
    if (ImGui::IsKeyPressed(ImGuiKey_Delete))
    {
        if (SelectedNotifyIndex >= 0 && SelectedNotifyIndex < Notifies.Num())
        {
            Notifies.RemoveAt(SelectedNotifyIndex);
            SelectedNotifyIndex = -1;
        }
    }

    ImGui::EndChild();

    ImGui::Dummy(ImVec2(0.0f, 5.0f));

    // Bottom part: Playback Controls
    ImGui::BeginChild("PlaybackControls", ImVec2(LeftControlWidth, controlsHeight), false);

    // Playback Controls - Compact style
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.22f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.33f, 0.38f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.17f, 0.20f, 1.0f));

    float buttonSize = 24.0f;
    float spacing = 2.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));

    // Align to left with small margin
    ImGui::SetCursorPosX(5.0f);
    ImGui::SetCursorPosY(10.0f);

    // First Frame (|◀)
    if (ImGui::Button("|\xE2\x97\x80##First", ImVec2(buttonSize, buttonSize)))
    {
        AnimSingleNodeInstance->Pause();
        AnimSingleNodeInstance->SetInteralTime(0.0f);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("First Frame");

    ImGui::SameLine();

    // Previous Frame (◀)
    if (ImGui::Button("\xE2\x97\x80##Prev", ImVec2(buttonSize, buttonSize)))
    {
        AnimSingleNodeInstance->Pause();
        float frameTime = AnimationLength / AnimSequence->NumberOfFrames;
        float newTime = std::max(0.0f, CurrentInternalTime - frameTime);
        AnimSingleNodeInstance->SetInteralTime(newTime);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Previous Frame");

    ImGui::SameLine();

    // Stop (■)
    if (ImGui::Button("\xE2\x96\xA0##Stop", ImVec2(buttonSize, buttonSize)))
    {
        AnimSingleNodeInstance->Pause();
        AnimSingleNodeInstance->SetInteralTime(0.0f);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Stop");

    ImGui::SameLine();

    // Play/Pause
    if (AnimSingleNodeInstance->IsPlaying())
    {
        // Pause (||)
        if (ImGui::Button("||##Pause", ImVec2(buttonSize, buttonSize)))
        {
            AnimSingleNodeInstance->Pause();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Pause");
    }
    else
    {
        // Play (▶)
        if (ImGui::Button("\xE2\x96\xB6##Play", ImVec2(buttonSize, buttonSize)))
        {
            AnimSingleNodeInstance->Play(ActiveState->bLoopAnimation);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Play");
    }

    ImGui::SameLine();

    // Next Frame (▶)
    if (ImGui::Button("\xE2\x96\xB6##Next", ImVec2(buttonSize, buttonSize)))
    {
        AnimSingleNodeInstance->Pause();
        float frameTime = AnimationLength / AnimSequence->NumberOfFrames;
        float newTime = std::min(AnimationLength, CurrentInternalTime + frameTime);
        AnimSingleNodeInstance->SetInteralTime(newTime);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Next Frame");

    ImGui::SameLine();

    // Last Frame (▶|)
    if (ImGui::Button("\xE2\x96\xB6|##Last", ImVec2(buttonSize, buttonSize)))
    {
        AnimSingleNodeInstance->Pause();
        AnimSingleNodeInstance->SetInteralTime(AnimationLength);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Last Frame");

    ImGui::SameLine();

    // Loop Toggle
    ImGui::PushStyleColor(ImGuiCol_Button, ActiveState->bLoopAnimation ?
        ImVec4(0.15f, 0.50f, 0.35f, 1.0f) : ImVec4(0.20f, 0.22f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ActiveState->bLoopAnimation ?
        ImVec4(0.20f, 0.60f, 0.45f, 1.0f) : ImVec4(0.30f, 0.33f, 0.38f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ActiveState->bLoopAnimation ?
        ImVec4(0.10f, 0.40f, 0.25f, 1.0f) : ImVec4(0.15f, 0.17f, 0.20f, 1.0f));
    if (ImGui::Button("Loop##Loop", ImVec2(buttonSize * 1.5f, buttonSize)))
    {
        ActiveState->bLoopAnimation = !ActiveState->bLoopAnimation;
        if (AnimSingleNodeInstance->IsPlaying())
        {
            AnimSingleNodeInstance->Play(ActiveState->bLoopAnimation);
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(ActiveState->bLoopAnimation ? "Loop: ON" : "Loop: OFF");
    ImGui::PopStyleColor(6);

    // Speed control (same line)
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(5.0f, 0.0f)); // Small spacer
    ImGui::SameLine();

    float playRate = AnimSingleNodeInstance->GetPlayRate();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.85f, 0.9f, 1.0f));

    ImGui::PushItemWidth(60.0f);

    const char* speedOptions[] = { "x0.1", "x0.25", "x0.5", "x1.0", "x1.5", "x2.0", "x3.0" };
    const float speedValues[] = { 0.1f, 0.25f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f };
    int currentSpeedIndex = 3; // Default to 1.0x

    // Find current speed index
    for (int i = 0; i < 7; ++i)
    {
        if (fabs(playRate - speedValues[i]) < 0.01f)
        {
            currentSpeedIndex = i;
            break;
        }
    }

    char speedLabel[16];
    sprintf_s(speedLabel, "x%.2f", playRate);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.18f, 0.20f, 0.23f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.20f, 0.23f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.28f, 0.32f, 1.0f));

    if (ImGui::BeginCombo("##Speed", speedLabel, ImGuiComboFlags_NoArrowButton))
    {
        for (int i = 0; i < 7; ++i)
        {
            bool isSelected = (currentSpeedIndex == i);
            if (ImGui::Selectable(speedOptions[i], isSelected))
            {
                AnimSingleNodeInstance->SetPlayRate(speedValues[i]);
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::PopStyleColor(4);
    ImGui::PopItemWidth();
    ImGui::PopStyleVar(); // ItemSpacing

    ImGui::EndChild(); // PlaybackControls
    ImGui::EndChild(); // LeftControlArea

    // === RIGHT SIDE: Timeline + Notify Display ===
    ImGui::SameLine(0, 0);
    ImGui::BeginChild("TimelineArea", ImVec2(RightTimelineWidth, WindowHeight), false, ImGuiWindowFlags_NoScrollbar);

    // Timeline layout
    ImVec2 TimeLineStartPos = ImGui::GetCursorScreenPos();
    float TimeLineWidth = RightTimelineWidth - 20.0f;
    TimeLineStartPos.x += 10.0f;
    TimeLineStartPos.y += 10.0f;

    // Use same heights as left panel
    float TotalTracksHeight = NotifyTracks.Num() * TrackHeight;
    float TimeLineHeight = HeaderHeight + TotalTracksHeight;

    ImVec2 TimeLineEndPos = ImVec2(TimeLineStartPos.x + TimeLineWidth, TimeLineStartPos.y + TimeLineHeight);

    // Background
    DrawList->AddRectFilled(TimeLineStartPos, TimeLineEndPos, IM_COL32(25, 25, 28, 255));
    DrawList->AddRect(TimeLineStartPos, TimeLineEndPos, IM_COL32(60, 60, 65, 255), 0.0f, 0, 1.0f);

    // Draw frame markers and labels in header
    int FrameStep = 5;
    if (AnimSequence->NumberOfFrames > 100) FrameStep = 10;
    if (AnimSequence->NumberOfFrames > 200) FrameStep = 20;

    for (int Frame = 0; Frame <= AnimSequence->NumberOfFrames; Frame += FrameStep)
    {
        float Xpos = TimeLineStartPos.x + (Frame / (float)AnimSequence->NumberOfFrames) * TimeLineWidth;

        // Vertical grid line
        DrawList->AddLine(
            ImVec2(Xpos, TimeLineStartPos.y + HeaderHeight),
            ImVec2(Xpos, TimeLineEndPos.y),
            IM_COL32(40, 40, 45, 255), 1.0f
        );

        // Frame number
        char frameLabel[16];
        sprintf_s(frameLabel, "%d", Frame);
        ImVec2 labelSize = ImGui::CalcTextSize(frameLabel);
        DrawList->AddText(ImVec2(Xpos - labelSize.x * 0.5f, TimeLineStartPos.y + 5.0f), IM_COL32(150, 150, 155, 255), frameLabel);
    }

    // Dragging state
    static int DraggingNotifyIndex = -1;
    static float DragStartTime = 0.0f;
    static int DragStartTrack = 0;
    static float DragOffsetX = 0.0f;  // Offset from notify start position to click position

    // Track if any notify was right-clicked (to prevent timeline right-click conflict)
    static bool bNotifyRightClicked = false;
    bNotifyRightClicked = false;

    // Draw track lanes
    for (int trackIdx = 0; trackIdx < NotifyTracks.Num(); trackIdx++)
    {
        const UAnimSequenceBase::FNotifyTrack& Track = NotifyTracks[trackIdx];
        float LaneY = TimeLineStartPos.y + HeaderHeight + trackIdx * TrackHeight;

        // Lane background (alternating colors)
        ImU32 LaneColor = (trackIdx % 2 == 0) ? IM_COL32(30, 30, 33, 255) : IM_COL32(25, 25, 28, 255);
        DrawList->AddRectFilled(
            ImVec2(TimeLineStartPos.x, LaneY),
            ImVec2(TimeLineEndPos.x, LaneY + TrackHeight),
            LaneColor
        );

        // Lane separator
        DrawList->AddLine(
            ImVec2(TimeLineStartPos.x, LaneY),
            ImVec2(TimeLineEndPos.x, LaneY),
            IM_COL32(50, 50, 55, 255), 1.0f
        );

        // Draw notifies in this track (skip dragging notify)
        for (int i = 0; i < Notifies.Num(); i++)
        {
            // Skip notify being dragged - will be drawn separately at the end
            if (DraggingNotifyIndex == i) continue;

            FAnimNotifyEvent& Notify = Notifies[i];
            if (Notify.TrackIndex != Track.ID) continue;

            float NotifyPosX = TimeLineStartPos.x + (Notify.TriggerTime / AnimationLength) * TimeLineWidth;
            float NotifyY = LaneY + 8.0f;

            // Notify marker (triangle)
            ImVec2 p1(NotifyPosX, NotifyY);
            ImVec2 p2(NotifyPosX - 6.0f, NotifyY + 12.0f);
            ImVec2 p3(NotifyPosX + 6.0f, NotifyY + 12.0f);

            ImU32 NotifyColor = (i == SelectedNotifyIndex)
                ? IM_COL32(255, 200, 80, 255)   // Selected
                : IM_COL32(200, 120, 255, 255); // Normal

            DrawList->AddTriangleFilled(p1, p2, p3, NotifyColor);
            DrawList->AddTriangle(p1, p2, p3, IM_COL32(255, 255, 255, 180), 1.5f);

            // Duration bar
            float DurationEndPosX = NotifyPosX;  // Default to marker position
            if (Notify.Duration > 0.0f)
            {
                DurationEndPosX = TimeLineStartPos.x + ((Notify.TriggerTime + Notify.Duration) / AnimationLength) * TimeLineWidth;
                DrawList->AddRectFilled(
                    ImVec2(NotifyPosX, NotifyY + 12.0f),
                    ImVec2(DurationEndPosX, NotifyY + 20.0f),
                    IM_COL32(200, 120, 255, 120)
                );
                DrawList->AddRect(
                    ImVec2(NotifyPosX, NotifyY + 12.0f),
                    ImVec2(DurationEndPosX, NotifyY + 20.0f),
                    IM_COL32(200, 120, 255, 255), 0.0f, 0, 1.5f
                );
            }

            // Clickable/draggable area for notify (text will be drawn later on top)
            // IMPORTANT: Store FString first to avoid dangling pointer from temporary object
            FString notifyNameStr = Notify.NotifyName.ToString();
            const char* notifyName = notifyNameStr.c_str();
            ImVec2 textSize = ImGui::CalcTextSize(notifyName);

            // Calculate button width to include duration bar and text label
            float textEndX = NotifyPosX + 10.0f + textSize.x + 2.0f;  // Text label end position
            float buttonRightEdge = FMath::Max(textEndX, DurationEndPosX);  // Whichever is further right
            float buttonWidth = buttonRightEdge - (NotifyPosX - 10.0f);

            ImGui::SetCursorScreenPos(ImVec2(NotifyPosX - 10.0f, NotifyY - 5.0f));
            ImGui::PushID(i);
            ImGui::InvisibleButton("##Notify", ImVec2(buttonWidth, 25.0f));

            bool bItemHovered = ImGui::IsItemHovered();
            bool bItemActive = ImGui::IsItemActive();

            // Check if this notify was right-clicked
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            {
                bNotifyRightClicked = true;
            }

            // Right click context menu for notify
            if (ImGui::BeginPopupContextItem("NotifyContextMenu"))
            {
                SelectedNotifyIndex = i;

                if (ImGui::MenuItem("Edit Notify"))
                {
                    bOpenEditNotifyPopup = true;
                }
                if (ImGui::MenuItem("Delete Notify"))
                {
                    Notifies.RemoveAt(i);
                    SelectedNotifyIndex = -1;
                }
                ImGui::EndPopup();
            }

            // Double click to edit (check first to prevent single click action)
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && bItemHovered)
            {
                SelectedNotifyIndex = i;
                bOpenEditNotifyPopup = true;
            }
            // Start dragging
            else if (bItemActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f))
            {
                DraggingNotifyIndex = i;
                DragStartTime = Notify.TriggerTime;
                DragStartTrack = Notify.TrackIndex;
                // Calculate offset: where did the user click relative to the notify start position?
                DragOffsetX = ImGui::GetMousePos().x - NotifyPosX;
            }
            // Single click (selection only, no timeline seek)
            else if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
            {
                SelectedNotifyIndex = i;
            }

            ImGui::PopID();
        }
    }

    // Pass 2: Draw all notify text labels on top using ForegroundDrawList (renders above all widgets)
    ImDrawList* FgDrawList = ImGui::GetForegroundDrawList();
    for (int trackIdx = 0; trackIdx < NotifyTracks.Num(); trackIdx++)
    {
        const UAnimSequenceBase::FNotifyTrack& Track = NotifyTracks[trackIdx];
        float LaneY = TimeLineStartPos.y + HeaderHeight + trackIdx * TrackHeight;

        for (int i = 0; i < Notifies.Num(); i++)
        {
            // Skip notify being dragged
            if (DraggingNotifyIndex == i) continue;

            FAnimNotifyEvent& Notify = Notifies[i];
            if (Notify.TrackIndex != Track.ID) continue;

            float NotifyPosX = TimeLineStartPos.x + (Notify.TriggerTime / AnimationLength) * TimeLineWidth;
            float NotifyY = LaneY + 8.0f;

            // Draw notify label with background box on top
            // IMPORTANT: Store FString first to avoid dangling pointer from temporary object
            FString notifyNameStr = Notify.NotifyName.ToString();
            const char* notifyName = notifyNameStr.c_str();
            ImVec2 textSize = ImGui::CalcTextSize(notifyName);

            ImVec2 boxMin(NotifyPosX + 8.0f, NotifyY);
            ImVec2 boxMax(NotifyPosX + 12.0f + textSize.x, NotifyY + textSize.y + 4.0f);

            ImU32 BoxColor = (i == SelectedNotifyIndex)
                ? IM_COL32(60, 50, 80, 220)   // Selected
                : IM_COL32(45, 45, 55, 200);  // Normal

            FgDrawList->AddRectFilled(boxMin, boxMax, BoxColor, 3.0f);
            FgDrawList->AddRect(boxMin, boxMax, IM_COL32(150, 150, 160, 150), 3.0f, 0, 1.0f);

            FgDrawList->AddText(
                ImVec2(NotifyPosX + 10.0f, NotifyY + 2.0f),
                IM_COL32(220, 230, 240, 255),
                notifyName
            );
        }
    }

    // Draw dragging notify on top of everything
    if (DraggingNotifyIndex >= 0 && DraggingNotifyIndex < Notifies.Num())
    {
        FAnimNotifyEvent& DragNotify = Notifies[DraggingNotifyIndex];

        // Calculate position based on mouse, maintaining click offset
        ImVec2 MousePos = ImGui::GetMousePos();
        float NotifyPosX = MousePos.x - DragOffsetX;  // Apply offset to maintain click position
        float MouseY = MousePos.y - (TimeLineStartPos.y + HeaderHeight);
        int DragTrackArrayIdx = FMath::Clamp((int)(MouseY / TrackHeight), 0, NotifyTracks.Num() - 1);
        float NotifyY = TimeLineStartPos.y + HeaderHeight + DragTrackArrayIdx * TrackHeight + 8.0f;

        // Notify marker (triangle) - bright yellow for dragging
        ImVec2 p1(NotifyPosX, NotifyY);
        ImVec2 p2(NotifyPosX - 6.0f, NotifyY + 12.0f);
        ImVec2 p3(NotifyPosX + 6.0f, NotifyY + 12.0f);

        DrawList->AddTriangleFilled(p1, p2, p3, IM_COL32(255, 200, 80, 255));
        DrawList->AddTriangle(p1, p2, p3, IM_COL32(255, 255, 255, 220), 2.0f);

        // Duration bar
        if (DragNotify.Duration > 0.0f)
        {
            float EndPosX = NotifyPosX + (DragNotify.Duration / AnimationLength) * TimeLineWidth;
            DrawList->AddRectFilled(
                ImVec2(NotifyPosX, NotifyY + 12.0f),
                ImVec2(EndPosX, NotifyY + 20.0f),
                IM_COL32(255, 200, 80, 150)
            );
            DrawList->AddRect(
                ImVec2(NotifyPosX, NotifyY + 12.0f),
                ImVec2(EndPosX, NotifyY + 20.0f),
                IM_COL32(255, 200, 80, 255), 0.0f, 0, 2.0f
            );
        }

        // Notify label with background box (use ForegroundDrawList for topmost rendering)
        // IMPORTANT: Store FString first to avoid dangling pointer from temporary object
        FString notifyNameStr = DragNotify.NotifyName.ToString();
        const char* notifyName = notifyNameStr.c_str();
        ImVec2 textSize = ImGui::CalcTextSize(notifyName);

        // Background box for label (dragging)
        ImVec2 boxMin(NotifyPosX + 8.0f, NotifyY);
        ImVec2 boxMax(NotifyPosX + 12.0f + textSize.x, NotifyY + textSize.y + 4.0f);

        FgDrawList->AddRectFilled(boxMin, boxMax, IM_COL32(80, 60, 30, 230), 3.0f);
        FgDrawList->AddRect(boxMin, boxMax, IM_COL32(255, 200, 80, 200), 3.0f, 0, 1.5f);

        FgDrawList->AddText(
            ImVec2(NotifyPosX + 10.0f, NotifyY + 2.0f),
            IM_COL32(255, 255, 255, 255),
            notifyName
        );

        // End dragging on mouse release
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            // Apply offset when calculating final position
            float Ratio = ((MousePos.x - DragOffsetX) - TimeLineStartPos.x) / TimeLineWidth;
            Ratio = std::clamp(Ratio, 0.0f, 1.0f);
            DragNotify.TriggerTime = Ratio * AnimationLength;
            // Convert array index to Track.ID
            DragNotify.TrackIndex = NotifyTracks[DragTrackArrayIdx].ID;

            SelectedNotifyIndex = DraggingNotifyIndex;
            DraggingNotifyIndex = -1;
        }
    }

    // Draw playhead
    float HeadPosX = TimeLineStartPos.x + (CurrentInternalTime / AnimationLength) * TimeLineWidth;
    DrawList->AddLine(
        ImVec2(HeadPosX, TimeLineStartPos.y + HeaderHeight),
        ImVec2(HeadPosX, TimeLineEndPos.y),
        IM_COL32(80, 200, 120, 255), 2.0f);

    // Timeline interaction (InvisibleButton for dragging playhead and right-click)
    static ImVec2 RightClickPos;
    static int RightClickTrackID = -1;

    ImGui::SetCursorScreenPos(ImVec2(TimeLineStartPos.x, TimeLineStartPos.y + HeaderHeight));
    ImGui::InvisibleButton("##Timeline", ImVec2(TimeLineWidth, TotalTracksHeight));

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        float MouseX = ImGui::GetMousePos().x;
        float Ratio = (MouseX - TimeLineStartPos.x) / TimeLineWidth;
        Ratio = std::clamp(Ratio, 0.0f, 1.0f);
        CurrentInternalTime = Ratio * AnimationLength;
        AnimSingleNodeInstance->Pause();
        AnimSingleNodeInstance->SetInteralTime(CurrentInternalTime);
    }

    // Only open AddNotify popup if no notify was right-clicked and no other popup is open
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !bNotifyRightClicked && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
    {
        RightClickPos = ImGui::GetMousePos();
        // Determine which track was clicked
        float MouseY = RightClickPos.y - (TimeLineStartPos.y + HeaderHeight);
        int RightClickArrayIdx = FMath::Clamp((int)(MouseY / TrackHeight), 0, NotifyTracks.Num() - 1);
        RightClickTrackID = NotifyTracks[RightClickArrayIdx].ID;
        ImGui::OpenPopup("AddNotifyPopup");
    }

    // Add Notify Popup
    static char NotifyNameInput[64] = "NewNotify";
    static float NotifyDuration = 0.0f;

    if (ImGui::BeginPopup("AddNotifyPopup"))
    {
        ImGui::Text("Add Notify");
        ImGui::Separator();
        ImGui::InputText("Name", NotifyNameInput, sizeof(NotifyNameInput));
        ImGui::InputFloat("Duration", &NotifyDuration, 0.01f, 0.1f, "%.2f");

        if (ImGui::Button("Add", ImVec2(80, 0)))
        {
            float Ratio = (RightClickPos.x - TimeLineStartPos.x) / TimeLineWidth;
            Ratio = std::clamp(Ratio, 0.0f, 1.0f);
            float TriggerTime = Ratio * AnimationLength;

            FAnimNotifyEvent NewNotify;
            NewNotify.TriggerTime = TriggerTime;
            NewNotify.Duration = NotifyDuration;
            NewNotify.NotifyName = FName(NotifyNameInput);
            NewNotify.TrackIndex = RightClickTrackID;

            AnimSequence->AddNotify(NewNotify);

            // Reset inputs
            strcpy_s(NotifyNameInput, "NewNotify");
            NotifyDuration = 0.0f;

            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Open edit notify popup if flagged
    if (bOpenEditNotifyPopup)
    {
        ImGui::OpenPopup("EditNotifyPopup");
        bOpenEditNotifyPopup = false;
    }

    // Edit Notify Popup
    static char EditNameBuffer[64] = "";
    static float EditTime = 0.0f;
    static float EditDuration = 0.0f;
    static int EditTrackID = -1;

    if (ImGui::BeginPopup("EditNotifyPopup"))
    {
        if (SelectedNotifyIndex >= 0 && SelectedNotifyIndex < Notifies.Num())
        {
            FAnimNotifyEvent& Notify = Notifies[SelectedNotifyIndex];

            if (ImGui::IsWindowAppearing())
            {
                strcpy_s(EditNameBuffer, Notify.NotifyName.ToString().c_str());
                EditTime = Notify.TriggerTime;
                EditDuration = Notify.Duration;
                EditTrackID = Notify.TrackIndex;
            }

            ImGui::Text("Edit Notify");
            ImGui::Separator();

            // Edit name
            bool bPressedEnter = ImGui::InputText("Name", EditNameBuffer, sizeof(EditNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

            // Edit time
            ImGui::InputFloat("Time", &EditTime, 0.01f, 0.1f, "%.2f");

            // Edit duration
            ImGui::InputFloat("Duration", &EditDuration, 0.01f, 0.1f, "%.2f");

            // Edit track - find current track name
            FString CurrentTrackName = "Unknown";
            for (int i = 0; i < NotifyTracks.Num(); i++)
            {
                if (NotifyTracks[i].ID == EditTrackID)
                {
                    CurrentTrackName = NotifyTracks[i].Name;
                    break;
                }
            }

            if (ImGui::BeginCombo("Track", CurrentTrackName.c_str()))
            {
                for (int i = 0; i < NotifyTracks.Num(); i++)
                {
                    const UAnimSequenceBase::FNotifyTrack& Track = NotifyTracks[i];
                    bool isSelected = (Track.ID == EditTrackID);
                    if (ImGui::Selectable(Track.Name.c_str(), isSelected))
                    {
                        EditTrackID = Track.ID;
                    }
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Spacing();

            if (ImGui::Button("Save", ImVec2(80, 0)) || bPressedEnter)
            {
                Notify.NotifyName = FName(EditNameBuffer);
                Notify.TriggerTime = FMath::Clamp(EditTime, 0.0f, AnimationLength);
                Notify.Duration = FMath::Max(0.0f, EditDuration);
                Notify.TrackIndex = EditTrackID;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Delete", ImVec2(80, 0)))
            {
                Notifies.RemoveAt(SelectedNotifyIndex);
                SelectedNotifyIndex = -1;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }

    // Bottom info bar - Current Frame (Time in seconds) (Progress %)
    ImGui::SetCursorScreenPos(ImVec2(TimeLineStartPos.x, TimeLineEndPos.y + 10.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.85f, 0.9f, 1.0f));

    float ProgressPercent = (AnimationLength > 0.0f) ? (CurrentInternalTime / AnimationLength) * 100.0f : 0.0f;

    char frameInfo[128];
    sprintf_s(frameInfo, "Frame: %d / %d  (%.2fs / %.2fs)  (%.1f%%)",
              CurrentFrame,
              AnimSequence->NumberOfFrames,
              CurrentInternalTime,
              AnimationLength,
              ProgressPercent);
    ImGui::Text("%s", frameInfo);

    ImGui::PopStyleColor();

    ImGui::EndChild(); // TimelineArea

    ImGui::EndChild(); // TimelinePanel

    ImGui::EndChild(); // CenterRightBottomArea

    ImGui::PopStyleVar(); // ItemSpacing
}

void SAnimSequenceEditorWindow::LoadAnimationFile(const FString& FilePath)
{
    UE_LOG("SAnimSequenceEditorWindow::LoadAnimationFile called with: %s", FilePath.c_str());

    if (!ActiveState || FilePath.empty())
    {
        UE_LOG("LoadAnimationFile: Invalid ActiveState or FilePath");
        return;
    }

    if (!ActiveState->PreviewActor)
    {
        UE_LOG("LoadAnimationFile: No PreviewActor");
        return;
    }

    // FBX 파일 경로를 직접 사용
    FString FbxPath = FilePath;

    // FBX 파일 존재 확인
    if (!std::filesystem::exists(FbxPath))
    {
        UE_LOG("LoadAnimationFile: FBX file not found: %s", FbxPath.c_str());
        return;
    }

    // 메시가 아직 로드되지 않았으면 먼저 로드
    if (!ActiveState->CurrentMesh || ActiveState->LoadedMeshPath != FbxPath)
    {
        UE_LOG("Loading skeletal mesh from FBX: %s", FbxPath.c_str());
        LoadSkeletalMesh(FbxPath);
    }

    // SkeletalMeshComponent 가져오기
    USkeletalMeshComponent* SkeletalComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
    if (!SkeletalComp)
    {
        UE_LOG("LoadAnimationFile: No SkeletalMeshComponent");
        return;
    }

    if (!SkeletalComp->GetSkeletalMesh() || !SkeletalComp->GetSkeletalMesh()->GetSkeleton())
    {
        UE_LOG("LoadAnimationFile: SkeletalMesh or Skeleton is null");
        return;
    }

    // UFbxLoader를 사용해서 AnimSequence 직접 로드
    UAnimSequence* AnimSequence = UFbxLoader::GetInstance().LoadFbxAnimation(
        FbxPath,
        SkeletalComp->GetSkeletalMesh()->GetSkeleton()
    );

    if (!AnimSequence)
    {
        UE_LOG("LoadAnimationFile: Failed to load AnimSequence from FBX: %s", FbxPath.c_str());
        return;
    }

    UE_LOG("AnimSequence loaded: %s, frames: %d, length: %.2f",
           AnimSequence->GetName().c_str(),
           AnimSequence->NumberOfFrames,
           AnimSequence->GetPlayLength());

    // AnimSingleNodeInstance 생성 (원본 방식: Play 호출 안 함)
    UAnimSingleNodeInstance* AnimSingleNode = NewObject<UAnimSingleNodeInstance>();
    AnimSingleNode->SetAnimationAsset(AnimSequence);
    SkeletalComp->SetAnimInstance(AnimSingleNode);

    UE_LOG("AnimSingleNode created with animation: %s", AnimSequence->GetName().c_str());

    // Set to frame 0 and update to show initial pose instead of T-pose
    AnimSingleNode->SetInteralTime(0.0f);
    SkeletalComp->TickAnimation(0.0f);  // Force update to apply frame 0 pose

    // NOTE: .anim 파일 자동 로드 제거
    // FBX만 로드하면 notify 없이 깨끗한 상태로 시작
    // Notify가 필요하면 "Load Animation (.anim)" 버튼을 사용

    // Animation path buffer에 FBX 경로 저장 (UI용)
    strncpy_s(ActiveState->AnimationPathBuffer, FbxPath.c_str(), sizeof(ActiveState->AnimationPathBuffer) - 1);

    // 본 라인 업데이트
    ActiveState->bBoneLinesDirty = true;

    UE_LOG("Successfully loaded animation: %s", FbxPath.c_str());
}

void SAnimSequenceEditorWindow::SaveAnimationFile(const FString& FilePath)
{
    UE_LOG("SAnimSequenceEditorWindow::SaveAnimationFile called with: %s", FilePath.c_str());
    // This will be called from USlateManager
}

void SAnimSequenceEditorWindow::LoadSkeletalMesh(const FString& Path)
{
    UE_LOG(">>> LoadSkeletalMesh ENTER: %s", Path.c_str());

    if (!ActiveState || Path.empty())
    {
        UE_LOG(">>> LoadSkeletalMesh EXIT: ActiveState or Path is invalid");
        return;
    }

    // Load the skeletal mesh using the resource manager
    UE_LOG(">>> Calling UResourceManager::Load<USkeletalMesh>...");
    USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
    UE_LOG(">>> UResourceManager::Load returned: %p", Mesh);

    if (Mesh && ActiveState->PreviewActor)
    {
        // Set the mesh on the preview actor
        ActiveState->PreviewActor->SetSkeletalMesh(Path);
        ActiveState->CurrentMesh = Mesh;
        ActiveState->LoadedMeshPath = Path;  // Track for resource unloading

        // Sync mesh visibility with checkbox state
        if (auto* Skeletal = ActiveState->PreviewActor->GetSkeletalMeshComponent())
        {
            Skeletal->SetVisibility(ActiveState->bShowMesh);
        }

        // Mark bone lines as dirty to rebuild on next frame
        ActiveState->bBoneLinesDirty = true;

        // Clear and sync bone line visibility
        if (auto* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
        {
            LineComp->ClearLines();
            LineComp->SetLineVisible(ActiveState->bShowBones);
        }

        UE_LOG("SAnimSequenceEditorWindow: Loaded skeletal mesh from %s", Path.c_str());
    }
    else
    {
        UE_LOG("SAnimSequenceEditorWindow: Failed to load skeletal mesh from %s", Path.c_str());
    }
}

void SAnimSequenceEditorWindow::UpdateBoneLines()
{
    // TODO: Implement bone line rendering if needed for animation editor
}

void SAnimSequenceEditorWindow::UpdateBoneTransformFromSkeleton(AnimSequenceEditorState* State)
{
    // TODO: Implement if needed
}

void SAnimSequenceEditorWindow::ApplyBoneTransformToSkeleton(AnimSequenceEditorState* State)
{
    // TODO: Implement if needed
}

void SAnimSequenceEditorWindow::OnUpdate(float DeltaSeconds)
{
    if (!ActiveState || !ActiveState->Viewport)
        return;

    // Tick the preview world
    if (ActiveState->World)
    {
        ActiveState->World->Tick(DeltaSeconds);

        // Tick animation - critical for animation playback!
        if (ActiveState->PreviewActor)
        {
            if (USkeletalMeshComponent* SkeletalComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
            {
                SkeletalComp->TickAnimation(DeltaSeconds);

                // Always mark bone lines as dirty to update for all interactions
                // (playing, paused, scrubbing timeline, etc.)
                if (SkeletalComp->AnimInstance)
                {
                    ActiveState->bBoneLinesDirty = true;
                }
            }
        }
    }

    if (ActiveState && ActiveState->Client)
    {
        ActiveState->Client->Tick(DeltaSeconds);
    }
}

void SAnimSequenceEditorWindow::OnRenderViewport()
{
    if (ActiveState && ActiveState->Viewport && CenterRect.GetWidth() > 0 && CenterRect.GetHeight() > 0)
    {
        const uint32 NewStartX = static_cast<uint32>(CenterRect.Left);
        const uint32 NewStartY = static_cast<uint32>(CenterRect.Top);
        const uint32 NewWidth = static_cast<uint32>(CenterRect.GetWidth());
        const uint32 NewHeight = static_cast<uint32>(CenterRect.GetHeight());

        ActiveState->Viewport->Resize(NewStartX, NewStartY, NewWidth, NewHeight);

        // Rebuild bone lines if needed
        if (ActiveState->bShowBones && ActiveState->PreviewActor && ActiveState->CurrentMesh && ActiveState->bBoneLinesDirty)
        {
            if (ULineComponent* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
            {
                LineComp->SetLineVisible(true);
            }
            ActiveState->PreviewActor->RebuildBoneLines();
            ActiveState->bBoneLinesDirty = false;
        }

        // Render viewport
        ActiveState->Viewport->Render();
    }
}

void SAnimSequenceEditorWindow::OnMouseMove(FVector2D MousePos)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        ActiveState->Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);
    }
}

void SAnimSequenceEditorWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        ActiveState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
    }
}

void SAnimSequenceEditorWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        ActiveState->Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
    }
}
