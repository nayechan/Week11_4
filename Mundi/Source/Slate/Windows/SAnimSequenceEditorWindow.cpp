#include "pch.h"
#include "SAnimSequenceEditorWindow.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "D3D11RHI.h"
#include "RHIDevice.h"
#include "RenderManager.h"
#include "Renderer.h"
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
#include "FbxManager.h"
#include "AnimInstance.h"
#include "AnimSingleNodeInstance.h"
#include "AnimSequence.h"
#include "AnimSequenceBase.h"
#include "ResourceManager.h"
#include "SkeletalMesh.h"
#include "SkeletalMeshComponent.h"
#include "WindowsBinWriter.h"
#include "WindowsBinReader.h"
#include "ObjectIterator.h"
#include <filesystem>
#include <fstream>

SAnimSequenceEditorWindow::SAnimSequenceEditorWindow()
{
    CenterRect = FRect(0, 0, 0, 0);
}

SAnimSequenceEditorWindow::~SAnimSequenceEditorWindow()
{
    // Release viewport render target (메모리 누수 방지)
    ViewportRenderTarget.Release();

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
        bShouldRenderViewport = false;
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

    bool windowAppeared = ImGui::Begin("Animation Sequence Editor", &bIsOpen, flags);

    // Check if window is visible, not collapsed, and not fully occluded
    bool isCollapsed = ImGui::IsWindowCollapsed();

    // Only render viewport if window is visible and not collapsed
    bShouldRenderViewport = windowAppeared && !isCollapsed;

    if (windowAppeared)
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
    else
    {
        bShouldRenderViewport = false;
    }

    ImGui::End();

    // Render the viewport to its render target
    OnRenderViewport();
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

    // === Skeleton Swapping Section ===
    // IMPORTANT: 이 섹션은 화면과 완전히 독립적이며, 오직 MeshPathBuffer, AnimationPathBuffer, TargetSkeleton만 사용합니다.
    // 화면 적용 함수(LoadSkeletalMesh, SetAnimationAsset 등)는 절대 호출하지 않습니다.

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.35f, 0.50f, 0.8f));
    ImGui::Text("Skeleton Swapping");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // TargetSkeleton 초기화: MeshPathBuffer 또는 AnimationPathBuffer 기반
    ActiveState->TargetSkeleton = nullptr;
    if (ActiveState->MeshPathBuffer[0] != '\0')
    {
        // MeshPathBuffer에 경로가 있으면 해당 메시의 Skeleton 사용
        FString MeshPath = ActiveState->MeshPathBuffer;
        USkeletalMesh* Mesh = UResourceManager::GetInstance().Get<USkeletalMesh>(MeshPath);
        if (Mesh)
        {
            ActiveState->TargetSkeleton = const_cast<FSkeleton*>(Mesh->GetSkeleton());
        }
    }
    else if (ActiveState->AnimationPathBuffer[0] != '\0')
    {
        // AnimationPathBuffer에 경로가 있으면 해당 애니메이션의 Skeleton 사용
        FString AnimPath = ActiveState->AnimationPathBuffer;
        UAnimSequence* Anim = UResourceManager::GetInstance().Get<UAnimSequence>(AnimPath);
        if (Anim)
        {
            ActiveState->TargetSkeleton = Anim->Skeleton;
        }
    }

    // 1. Skeletal Mesh ComboBox
    ImGui::Text("Skeletal Mesh");
    {
        TArray<USkeletalMesh*> AllMeshes = UResourceManager::GetInstance().GetAll<USkeletalMesh>();
        TArray<USkeletalMesh*> UniqueMeshes;
        TArray<const char*> MeshItems;
        UniqueMeshes.Add(nullptr);
        MeshItems.Add("None");

        for (USkeletalMesh* Mesh : AllMeshes)
        {
            if (Mesh && !Mesh->GetFilePath().empty())
            {
                bool bAlreadyAdded = false;
                for (USkeletalMesh* ExistingMesh : UniqueMeshes)
                {
                    if (ExistingMesh && ExistingMesh->GetFilePath() == Mesh->GetFilePath())
                    {
                        bAlreadyAdded = true;
                        break;
                    }
                }
                if (!bAlreadyAdded)
                {
                    UniqueMeshes.Add(Mesh);
                    FString DisplayName = Mesh->GetFilePath();
                    size_t lastSlash = DisplayName.find_last_of("/\\");
                    if (lastSlash != std::string::npos)
                    {
                        DisplayName = DisplayName.substr(lastSlash + 1);
                    }
                    MeshItems.Add(_strdup(DisplayName.c_str()));
                }
            }
        }

        // 현재 선택: MeshPathBuffer 기반
        int CurrentMeshIdx = 0;
        if (ActiveState->MeshPathBuffer[0] != '\0')
        {
            FString BufferPath = ActiveState->MeshPathBuffer;
            for (int i = 0; i < UniqueMeshes.Num(); ++i)
            {
                if (UniqueMeshes[i] && UniqueMeshes[i]->GetFilePath() == BufferPath)
                {
                    CurrentMeshIdx = i;
                    break;
                }
            }
        }

        ImGui::SetNextItemWidth(240);
        if (ImGui::Combo("##SkeletalMesh", &CurrentMeshIdx, MeshItems.data(), static_cast<int>(MeshItems.size())))
        {
            // Swapping: MeshPathBuffer 업데이트만, 화면 적용 금지
            if (CurrentMeshIdx > 0 && CurrentMeshIdx < UniqueMeshes.Num())
            {
                USkeletalMesh* SelectedMesh = UniqueMeshes[CurrentMeshIdx];
                if (SelectedMesh)
                {
                    FString MeshPath = SelectedMesh->GetFilePath();
                    strncpy_s(ActiveState->MeshPathBuffer, sizeof(ActiveState->MeshPathBuffer),
                              MeshPath.c_str(), _TRUNCATE);

                    // 자동으로 TargetSkeleton 설정
                    ActiveState->TargetSkeleton = const_cast<FSkeleton*>(SelectedMesh->GetSkeleton());

                    UE_LOG("[Swapping] SkeletalMesh buffer set to: %s (NOT applied to screen)", MeshPath.c_str());
                }
            }
            else if (CurrentMeshIdx == 0)
            {
                // Clear
                ActiveState->MeshPathBuffer[0] = '\0';
                // TargetSkeleton은 AnimationPathBuffer 기반으로 재설정될 것임
                UE_LOG("[Swapping] SkeletalMesh buffer cleared");
            }
        }

        for (int i = 1; i < MeshItems.Num(); ++i)
        {
            free(const_cast<char*>(MeshItems[i]));
        }
    }

    ImGui::Spacing();

    // 2. Skeleton ComboBox
    ImGui::Text("Skeleton");
    {
        TArray<FString> AllSkeletonNames = FFbxManager::GetAllSkeletonNames();
        TArray<const char*> SkeletonItems;
        SkeletonItems.Add("None");

        // 현재 선택: TargetSkeleton 기반
        int CurrentSkeletonIdx = 0;
        for (int i = 0; i < AllSkeletonNames.Num(); ++i)
        {
            SkeletonItems.Add(AllSkeletonNames[i].c_str());
            if (ActiveState->TargetSkeleton && AllSkeletonNames[i] == ActiveState->TargetSkeleton->Name)
            {
                CurrentSkeletonIdx = i + 1;
            }
        }

        ImGui::SetNextItemWidth(240);
        if (ImGui::Combo("##Skeleton", &CurrentSkeletonIdx, SkeletonItems.data(), static_cast<int>(SkeletonItems.size())))
        {
            // Swapping: MeshPathBuffer나 AnimationPathBuffer의 리소스 Skeleton만 변경
            FSkeleton* NewSkeleton = nullptr;
            if (CurrentSkeletonIdx > 0 && CurrentSkeletonIdx <= AllSkeletonNames.Num())
            {
                FString SelectedName = AllSkeletonNames[CurrentSkeletonIdx - 1];
                NewSkeleton = FFbxManager::GetSkeletonByName(SelectedName);
            }

            // MeshPathBuffer에 있는 메시의 Skeleton 변경
            if (ActiveState->MeshPathBuffer[0] != '\0')
            {
                FString MeshPath = ActiveState->MeshPathBuffer;
                USkeletalMesh* Mesh = UResourceManager::GetInstance().Get<USkeletalMesh>(MeshPath);
                if (Mesh)
                {
                    Mesh->SetSkeleton(NewSkeleton);
                    Mesh->SkeletonID = NewSkeleton ? NewSkeleton->Name : "";

                    // Save .fbx.skeleton file with new Skeleton reference
                    FString SkeletonOverridePath = MeshPath + ".skeleton";
                    Mesh->SaveOverrideData(SkeletonOverridePath);

                    UE_LOG("[Swapping] Changed SkeletalMesh Skeleton to: %s",
                           NewSkeleton ? NewSkeleton->Name.c_str() : "None");
                }
            }

            // AnimationPathBuffer에 있는 애니메이션의 Skeleton 변경
            if (ActiveState->AnimationPathBuffer[0] != '\0')
            {
                FString AnimPath = ActiveState->AnimationPathBuffer;
                UAnimSequence* Anim = UResourceManager::GetInstance().Get<UAnimSequence>(AnimPath);
                if (Anim)
                {
                    Anim->Skeleton = NewSkeleton;
                    Anim->SkeletonID = NewSkeleton ? NewSkeleton->Name : "";

                    // FBX 애니메이션의 경우 .fbx.skeleton의 "Animation" 섹션에 저장
                    // .anim 파일의 경우 SourceAnimationPath를 통해 원본 애니메이션의 Skeleton을 따라감
                    if (AnimPath.find(".fbx") != FString::npos)
                    {
                        FString SkeletonOverridePath = AnimPath + ".skeleton";
                        Anim->SaveOverrideData(SkeletonOverridePath);

                        // 이 FBX 애니메이션을 SourceAnimationPath로 참조하는 모든 .anim 파일도 업데이트
                        int32 UpdatedCount = 0;
                        for (TObjectIterator<UAnimSequence> It; It; ++It)
                        {
                            UAnimSequence* DerivedAnim = *It;
                            if (DerivedAnim && DerivedAnim->SourceAnimationPath == AnimPath)
                            {
                                DerivedAnim->Skeleton = NewSkeleton;
                                DerivedAnim->SkeletonID = NewSkeleton ? NewSkeleton->Name : "";
                                ++UpdatedCount;
                            }
                        }

                        if (UpdatedCount > 0)
                        {
                            UE_LOG("[Swapping] Updated %d derived .anim file(s) referencing this animation", UpdatedCount);
                        }
                    }

                    UE_LOG("[Swapping] Changed Animation Skeleton to: %s",
                           NewSkeleton ? NewSkeleton->Name.c_str() : "None");
                }
            }

            ActiveState->TargetSkeleton = NewSkeleton;
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted("Change Skeleton reference for selected SkeletalMesh or Animation");
            ImGui::EndTooltip();
        }
    }

    ImGui::Spacing();

    // 3. Animation ComboBox
    ImGui::Text("Animation");
    {
        TArray<FString> AllAnimPaths = UResourceManager::GetInstance().GetAllFilePaths<UAnimSequence>();
        TArray<const char*> AnimItems;
        TArray<FString> FilteredAnimPaths;
        AnimItems.Add("None");
        FilteredAnimPaths.Add("");

        // 필터링: MeshPathBuffer의 메시가 참조하는 Skeleton과 동일한 Skeleton을 참조하는 Animation만
        FSkeleton* FilterSkeleton = nullptr;
        if (ActiveState->MeshPathBuffer[0] != '\0')
        {
            FString MeshPath = ActiveState->MeshPathBuffer;
            USkeletalMesh* Mesh = UResourceManager::GetInstance().Get<USkeletalMesh>(MeshPath);
            if (Mesh)
            {
                FilterSkeleton = const_cast<FSkeleton*>(Mesh->GetSkeleton());
            }
        }

        for (const FString& AnimPath : AllAnimPaths)
        {
            if (!AnimPath.empty())
            {
                bool bShouldInclude = true;

                // MeshPathBuffer에 메시가 선택되어 있으면 Skeleton 필터링
                if (FilterSkeleton)
                {
                    UAnimSequence* Anim = UResourceManager::GetInstance().Get<UAnimSequence>(AnimPath);
                    if (Anim)
                    {
                        bShouldInclude = (Anim->Skeleton == FilterSkeleton);
                    }
                    else
                    {
                        bShouldInclude = false;
                    }
                }

                if (bShouldInclude)
                {
                    FilteredAnimPaths.Add(AnimPath);

                    std::filesystem::path fsPath(AnimPath);
                    FString DisplayName = fsPath.filename().string();
                    AnimItems.Add(_strdup(DisplayName.c_str()));
                }
            }
        }

        // 현재 선택: AnimationPathBuffer 기반
        int CurrentAnimIdx = 0;
        if (ActiveState->AnimationPathBuffer[0] != '\0')
        {
            FString BufferPath = ActiveState->AnimationPathBuffer;
            for (int i = 0; i < FilteredAnimPaths.Num(); ++i)
            {
                if (FilteredAnimPaths[i] == BufferPath)
                {
                    CurrentAnimIdx = i;
                    break;
                }
            }
        }

        ImGui::SetNextItemWidth(240);
        if (ImGui::Combo("##Animation", &CurrentAnimIdx, AnimItems.data(), static_cast<int>(AnimItems.size())))
        {
            // Swapping: AnimationPathBuffer 업데이트만, 화면 적용 금지
            if (CurrentAnimIdx > 0 && CurrentAnimIdx < FilteredAnimPaths.Num())
            {
                const FString& SelectedPath = FilteredAnimPaths[CurrentAnimIdx];
                UAnimSequence* SelectedAnim = UResourceManager::GetInstance().Get<UAnimSequence>(SelectedPath);

                if (SelectedAnim)
                {
                    strncpy_s(ActiveState->AnimationPathBuffer, sizeof(ActiveState->AnimationPathBuffer),
                              SelectedPath.c_str(), _TRUNCATE);

                    // 자동으로 TargetSkeleton 설정
                    ActiveState->TargetSkeleton = SelectedAnim->Skeleton;

                    UE_LOG("[Swapping] Animation buffer set to: %s (NOT applied to screen)", SelectedPath.c_str());
                }
            }
            else if (CurrentAnimIdx == 0)
            {
                // Clear
                ActiveState->AnimationPathBuffer[0] = '\0';
                UE_LOG("[Swapping] Animation buffer cleared");
            }
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            if (FilterSkeleton)
            {
                ImGui::Text("Showing animations compatible with Skeleton: %s", FilterSkeleton->Name.c_str());
            }
            else
            {
                ImGui::TextUnformatted("Showing all animations (no SkeletalMesh selected)");
            }
            ImGui::EndTooltip();
        }

        for (int i = 1; i < AnimItems.Num(); ++i)
        {
            free(const_cast<char*>(AnimItems[i]));
        }
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // === Skeleton-Based Animation Browser ===
    static int32 SelectedMeshIndex = -1;

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.35f, 0.50f, 0.8f));
    ImGui::Text("Compatible Animations");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Get all loaded skeletal meshes
    TArray<USkeletalMesh*> AllLoadedMeshes = UResourceManager::GetInstance().GetAll<USkeletalMesh>();

    // Remove duplicates based on normalized file path and sort for stable order
    TMap<FString, USkeletalMesh*> UniqueMeshes;
    for (USkeletalMesh* Mesh : AllLoadedMeshes)
    {
        if (Mesh)
        {
            FString FilePath = Mesh->GetFilePath();
            if (!FilePath.empty())
            {
                // Normalize path to handle relative/absolute path differences
                // (e.g., "Data/Fbx/mesh.fbx" vs "Data/Fbx/../Fbx/mesh.fbx")
                try
                {
                    std::filesystem::path NormalizedPath = std::filesystem::path(FilePath).lexically_normal();
                    FString NormalizedPathStr = NormalizedPath.string();
                    std::replace(NormalizedPathStr.begin(), NormalizedPathStr.end(), '\\', '/');

                    if (UniqueMeshes.find(NormalizedPathStr) == UniqueMeshes.end())
                    {
                        UniqueMeshes[NormalizedPathStr] = Mesh;
                    }
                }
                catch (...)
                {
                    // Fallback: use original path if normalization fails
                    if (UniqueMeshes.find(FilePath) == UniqueMeshes.end())
                    {
                        UniqueMeshes[FilePath] = Mesh;
                    }
                }
            }
        }
    }

    // Convert to sorted array
    TArray<USkeletalMesh*> LoadedMeshes;
    TArray<FString> SortedPaths;
    for (auto& Pair : UniqueMeshes)
    {
        SortedPaths.Add(Pair.first);
    }
    std::sort(SortedPaths.begin(), SortedPaths.end());

    for (const FString& Path : SortedPaths)
    {
        LoadedMeshes.Add(UniqueMeshes[Path]);
    }

    if (LoadedMeshes.Num() > 0)
    {
        // Skeletal Mesh selector
        ImGui::Text("Select Skeletal Mesh:");
        ImGui::PushItemWidth(-1.0f);

        // Prepare combo items
        TArray<FString> MeshNames;
        for (int32 i = 0; i < LoadedMeshes.Num(); ++i)
        {
            USkeletalMesh* Mesh = LoadedMeshes[i];
            FString DisplayName = Mesh->GetFilePath();
            if (!DisplayName.empty())
            {
                // Extract filename from path
                size_t lastSlash = DisplayName.find_last_of("/\\");
                if (lastSlash != std::string::npos)
                {
                    DisplayName = DisplayName.substr(lastSlash + 1);
                }
            }
            else
            {
                DisplayName = "Unnamed Mesh";
            }
            MeshNames.Add(DisplayName);
        }

        // Current selection display
        const char* CurrentSelectionLabel = (SelectedMeshIndex >= 0 && SelectedMeshIndex < MeshNames.Num())
            ? MeshNames[SelectedMeshIndex].c_str()
            : "Select a mesh...";

        if (ImGui::BeginCombo("##MeshSelector", CurrentSelectionLabel))
        {
            for (int32 i = 0; i < LoadedMeshes.Num(); ++i)
            {
                bool isSelected = (SelectedMeshIndex == i);
                if (ImGui::Selectable(MeshNames[i].c_str(), isSelected))
                {
                    SelectedMeshIndex = i;
                }
                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        ImGui::Spacing();

        // If a mesh is selected, show compatible animations
        if (SelectedMeshIndex >= 0 && SelectedMeshIndex < LoadedMeshes.Num())
        {
            USkeletalMesh* SelectedMesh = LoadedMeshes[SelectedMeshIndex];
            const FSkeleton* MeshSkeleton = SelectedMesh->GetSkeleton();

            if (MeshSkeleton)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 0.6f, 1.0f));
                ImGui::Text("Skeleton: %s", MeshSkeleton->Name.c_str());
                ImGui::Text("Bones: %d", MeshSkeleton->Bones.Num());
                ImGui::PopStyleColor();
                ImGui::Spacing();

                // Get all loaded animations
                TArray<UAnimSequence*> AllAnimations = UResourceManager::GetInstance().GetAll<UAnimSequence>();

                // Filter animations that match this skeleton (pointer equality only)
                TArray<UAnimSequence*> CompatibleAnimations;
                for (UAnimSequence* Anim : AllAnimations)
                {
                    if (Anim && Anim->Skeleton)
                    {
                        // Only pointer equality check - must be the exact same Skeleton
                        if (Anim->Skeleton == MeshSkeleton)
                        {
                            CompatibleAnimations.Add(Anim);
                        }
                    }
                }

                // Display compatible animations list
                ImGui::Text("Compatible Animations: %d", CompatibleAnimations.Num());
                ImGui::Spacing();

                if (CompatibleAnimations.Num() > 0)
                {
                    // Scrollable animation list
                    ImGui::BeginChild("AnimListScroll", ImVec2(0, 150), true);

                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.30f, 0.40f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.45f, 0.60f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.25f, 0.35f, 1.0f));

                    for (int32 i = 0; i < CompatibleAnimations.Num(); ++i)
                    {
                        UAnimSequence* Anim = CompatibleAnimations[i];
                        // Always use filename from FilePath instead of GetName()
                        FString AnimName = Anim->GetFilePath();
                        if (!AnimName.empty())
                        {
                            size_t lastSlash = AnimName.find_last_of("/\\");
                            if (lastSlash != std::string::npos)
                            {
                                AnimName = AnimName.substr(lastSlash + 1);
                            }
                        }
                        else
                        {
                            AnimName = "Unnamed Animation";
                        }

                        char ButtonLabel[256];
                        sprintf_s(ButtonLabel, "%s##Anim%d", AnimName.c_str(), i);

                        if (ImGui::Button(ButtonLabel, ImVec2(-1.0f, 0.0f)))
                        {
                            // Load this animation into the editor
                            if (ActiveState && ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkeletalMeshComponent())
                            {
                                USkeletalMeshComponent* SkeletalComp = ActiveState->PreviewActor->GetSkeletalMeshComponent();

                                // If current mesh doesn't match selected mesh, load the selected mesh first
                                if (ActiveState->CurrentMesh != SelectedMesh)
                                {
                                    FString MeshPath = SelectedMesh->GetFilePath();
                                    if (!MeshPath.empty())
                                    {
                                        LoadSkeletalMesh(MeshPath);
                                    }
                                }

                                // Set animation
                                UAnimSingleNodeInstance* AnimSingleNode = Cast<UAnimSingleNodeInstance>(SkeletalComp->AnimInstance);
                                if (!AnimSingleNode)
                                {
                                    AnimSingleNode = NewObject<UAnimSingleNodeInstance>();
                                    SkeletalComp->SetAnimInstance(AnimSingleNode);
                                }

                                AnimSingleNode->SetAnimationAsset(Anim);
                                AnimSingleNode->SetInteralTime(0.0f);
                                SkeletalComp->TickAnimation(0.0f);

                                // Update animation path buffer
                                FString AnimPath = Anim->GetFilePath();
                                if (!AnimPath.empty())
                                {
                                    strncpy_s(ActiveState->AnimationPathBuffer, AnimPath.c_str(), sizeof(ActiveState->AnimationPathBuffer) - 1);
                                }

                                // Mark bone lines as dirty
                                ActiveState->bBoneLinesDirty = true;
                            }
                        }

                        // Show animation info on hover
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::Text("Name: %s", AnimName.c_str());
                            ImGui::Text("Length: %.2fs", Anim->GetPlayLength());
                            ImGui::Text("Frames: %d", Anim->NumberOfFrames);
                            ImGui::EndTooltip();
                        }
                    }

                    ImGui::PopStyleColor(3);
                    ImGui::EndChild();
                }
                else
                {
                    ImGui::TextDisabled("No compatible animations found");
                }
            }
            else
            {
                ImGui::TextDisabled("Selected mesh has no skeleton");
            }
        }
    }
    else
    {
        ImGui::TextDisabled("No skeletal meshes loaded");
    }

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

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // === Save & Load ===
    ImGui::Text("Save & Load");
    ImGui::Spacing();

    // Save Animation (Full) - JSON
    if (ImGui::Button("Save Animation (Full)", ImVec2(leftWidth - 20.0f, 0)))
    {
        if (!AnimSequence)
        {
            ImGui::EndChild(); // LeftPanel
            ImGui::SameLine(0, 0);
            ImGui::BeginChild("CenterRightBottomArea", ImVec2(centerWidth + rightWidth, totalHeight), false, ImGuiWindowFlags_NoScrollbar);
            ImGui::BeginChild("TopArea", ImVec2(centerWidth + rightWidth, centerHeight), false, ImGuiWindowFlags_NoScrollbar);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
            ImGui::BeginChild("CenterViewport", ImVec2(centerWidth, centerHeight), true, ImGuiWindowFlags_NoScrollbar);
            ImGui::PopStyleVar();
            ImGui::EndChild();
            ImGui::EndChild();
            ImGui::EndChild();
            ImGui::PopStyleVar();
            return;
        }

        FString SavePath = AnimSequence->GetFilePath();
        if (SavePath.empty())
        {
            SavePath = "Data/Animations/modified_anim.json";
        }
        else
        {
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
        AnimSequence->SaveToFile(SavePath);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Save complete animation data to JSON file");

    // Save Animation (.anim) - Binary
    if (ImGui::Button("Save Animation (.anim)", ImVec2(leftWidth - 20.0f, 0)))
    {
        if (!AnimSequence)
        {
            ImGui::EndChild(); // LeftPanel
            ImGui::SameLine(0, 0);
            ImGui::BeginChild("CenterRightBottomArea", ImVec2(centerWidth + rightWidth, totalHeight), false, ImGuiWindowFlags_NoScrollbar);
            ImGui::BeginChild("TopArea", ImVec2(centerWidth + rightWidth, centerHeight), false, ImGuiWindowFlags_NoScrollbar);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
            ImGui::BeginChild("CenterViewport", ImVec2(centerWidth, centerHeight), true, ImGuiWindowFlags_NoScrollbar);
            ImGui::PopStyleVar();
            ImGui::EndChild();
            ImGui::EndChild();
            ImGui::EndChild();
            ImGui::PopStyleVar();
            return;
        }

        FString DefaultFileName;
        FString SourcePath = AnimSequence->GetFilePath();
        if (!SourcePath.empty())
        {
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

        OPENFILENAMEA ofn = {};
        char szFile[260] = {};
        strncpy_s(szFile, DefaultFileName.c_str(), sizeof(szFile) - 1);

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
            try
            {
                FWindowsBinWriter Writer(SavePath);
                if (Writer.IsOpen())
                {
                    // 원본 애니메이션 경로 저장 (이 애니메이션의 Skeleton을 따라감)
                    if (AnimSequence->SourceAnimationPath.empty())
                    {
                        AnimSequence->SourceAnimationPath = SourcePath;
                    }

                    Writer << *AnimSequence;
                    Writer.Close();

                    FWindowsBinReader Reader(SavePath);
                    if (Reader.IsOpen())
                    {
                        UAnimSequence* LoadedAnimSeq = NewObject<UAnimSequence>();
                        Reader << *LoadedAnimSeq;
                        Reader.Close();

                        LoadedAnimSeq->Skeleton = AnimSequence->Skeleton;
                        LoadedAnimSeq->SetFilePath(SavePath);

                        FString FileName = SavePath;
                        size_t lastSlash = FileName.find_last_of("/\\");
                        if (lastSlash != std::string::npos)
                        {
                            FileName = FileName.substr(lastSlash + 1);
                        }
                        size_t lastDot = FileName.find_last_of('.');
                        if (lastDot != std::string::npos)
                        {
                            FileName = FileName.substr(0, lastDot);
                        }
                        LoadedAnimSeq->ObjectName = FName(FileName);

                        UResourceManager::GetInstance().Add<UAnimSequence>(SavePath, LoadedAnimSeq);
                    }
                }
            }
            catch (const std::exception& e)
            {
            }
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Save animation data to binary .anim file (notify, curve, etc.)");

    // Load Animation (.anim) - Binary
    if (ImGui::Button("Load Animation (.anim)", ImVec2(leftWidth - 20.0f, 0)))
    {
        OPENFILENAMEA ofn = {};
        char szFile[260] = {};

        std::filesystem::path InitialDir = std::filesystem::current_path() / "Data" / "Fbx";
        std::string InitialDirStr = InitialDir.string();

        ofn.lStructSize = sizeof(OPENFILENAMEA);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "Animation Files (*.anim)\0*.anim\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = InitialDirStr.c_str();
        ofn.lpstrTitle = "Load Animation from .anim file";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn) == TRUE)
        {
            FString AnimFilePath = ofn.lpstrFile;
            try
            {
                USkeletalMeshComponent* SkeletalComp = ActiveState->PreviewActor ? ActiveState->PreviewActor->GetSkeletalMeshComponent() : nullptr;
                if (!SkeletalComp || !SkeletalComp->GetSkeletalMesh() || !SkeletalComp->GetSkeletalMesh()->GetSkeleton())
                {
                    ImGui::EndChild(); // LeftPanel
                    ImGui::SameLine(0, 0);
                    ImGui::BeginChild("CenterRightBottomArea", ImVec2(centerWidth + rightWidth, totalHeight), false, ImGuiWindowFlags_NoScrollbar);
                    ImGui::BeginChild("TopArea", ImVec2(centerWidth + rightWidth, centerHeight), false, ImGuiWindowFlags_NoScrollbar);
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
                    ImGui::BeginChild("CenterViewport", ImVec2(centerWidth, centerHeight), true, ImGuiWindowFlags_NoScrollbar);
                    ImGui::PopStyleVar();
                    ImGui::EndChild();
                    ImGui::EndChild();
                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                    return;
                }

                const FSkeleton* CurrentSkeleton = SkeletalComp->GetSkeletalMesh()->GetSkeleton();

                FWindowsBinReader Reader(AnimFilePath);
                if (!Reader.IsOpen())
                {
                    ImGui::EndChild(); // LeftPanel
                    ImGui::SameLine(0, 0);
                    ImGui::BeginChild("CenterRightBottomArea", ImVec2(centerWidth + rightWidth, totalHeight), false, ImGuiWindowFlags_NoScrollbar);
                    ImGui::BeginChild("TopArea", ImVec2(centerWidth + rightWidth, centerHeight), false, ImGuiWindowFlags_NoScrollbar);
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
                    ImGui::BeginChild("CenterViewport", ImVec2(centerWidth, centerHeight), true, ImGuiWindowFlags_NoScrollbar);
                    ImGui::PopStyleVar();
                    ImGui::EndChild();
                    ImGui::EndChild();
                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                    return;
                }

                UAnimSequence* LoadedAnimSeq = NewObject<UAnimSequence>();
                Reader << *LoadedAnimSeq;
                Reader.Close();

                LoadedAnimSeq->Skeleton = const_cast<FSkeleton*>(CurrentSkeleton);
                LoadedAnimSeq->SetFilePath(AnimFilePath);

                FString FileName = AnimFilePath;
                size_t lastSlash = FileName.find_last_of("/\\");
                if (lastSlash != std::string::npos)
                {
                    FileName = FileName.substr(lastSlash + 1);
                }
                size_t lastDot = FileName.find_last_of('.');
                if (lastDot != std::string::npos)
                {
                    FileName = FileName.substr(0, lastDot);
                }
                LoadedAnimSeq->ObjectName = FName(FileName);

                UResourceManager::GetInstance().Add<UAnimSequence>(AnimFilePath, LoadedAnimSeq);
                UE_LOG("Registered in ResourceManager");

                UAnimSingleNodeInstance* SingleNodeInstance = Cast<UAnimSingleNodeInstance>(SkeletalComp->AnimInstance);
                if (SingleNodeInstance)
                {
                    UE_LOG("Reusing existing AnimInstance");
                    SingleNodeInstance->SetAnimationAsset(LoadedAnimSeq);
                }
                else
                {
                    UE_LOG("Creating new AnimInstance");
                    UAnimSingleNodeInstance* AnimInstance = NewObject<UAnimSingleNodeInstance>();
                    AnimInstance->SetAnimationAsset(LoadedAnimSeq);
                    SkeletalComp->SetAnimInstance(AnimInstance);
                }

                if (UAnimSingleNodeInstance* Inst = Cast<UAnimSingleNodeInstance>(SkeletalComp->AnimInstance))
                {
                    Inst->SetInteralTime(0.0f);
                    SkeletalComp->TickAnimation(0.0f);
                }

                ActiveState->bBoneLinesDirty = true;
                strncpy_s(ActiveState->AnimationPathBuffer, AnimFilePath.c_str(), sizeof(ActiveState->AnimationPathBuffer) - 1);

                UE_LOG("Animation loaded successfully!");
                UE_LOG("==========================================================");
            }
            catch (const std::exception& e)
            {
                UE_LOG("ERROR: Failed to load .anim file: %s", e.what());
                UE_LOG("==========================================================");
            }
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Load animation from binary .anim file");

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

    // 뷰포트 영역의 화면 좌표를 먼저 계산 (렌더링에 필요)
    ImVec2 viewportMin = ImGui::GetCursorScreenPos();
    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    ImVec2 viewportMax = ImVec2(viewportMin.x + contentSize.x, viewportMin.y + contentSize.y);

    // CenterRect 설정 (렌더 타겟 유무와 관계없이 항상 설정)
    CenterRect.Left = viewportMin.x;
    CenterRect.Top = viewportMin.y;
    CenterRect.Right = viewportMax.x;
    CenterRect.Bottom = viewportMax.y;
    CenterRect.UpdateMinMax();

    // ImGui::Image로 뷰포트 렌더 타겟 표시
    if (ViewportRenderTarget.IsValid() && ViewportRenderTarget.SRV)
    {
        ImGui::Image((ImTextureID)ViewportRenderTarget.SRV, contentSize);

        // 마우스 입력 처리
        if (ActiveState && ActiveState->Viewport && ImGui::IsItemHovered())
        {
            // 마우스 위치 (글로벌)
            ImVec2 mousePos = ImGui::GetMousePos();

            // 로컬 좌표 계산 (이미지 내부 좌표, 0,0 기준)
            FVector2D localPos(mousePos.x - viewportMin.x, mousePos.y - viewportMin.y);

            // 마우스 이동
            ActiveState->Viewport->ProcessMouseMove((int32)localPos.X, (int32)localPos.Y);

            // 마우스 클릭
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                ActiveState->Viewport->ProcessMouseButtonDown((int32)localPos.X, (int32)localPos.Y, 0);
            }
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                ActiveState->Viewport->ProcessMouseButtonDown((int32)localPos.X, (int32)localPos.Y, 1);
            }
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
            {
                ActiveState->Viewport->ProcessMouseButtonDown((int32)localPos.X, (int32)localPos.Y, 2);
            }

            // 마우스 릴리스
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                ActiveState->Viewport->ProcessMouseButtonUp((int32)localPos.X, (int32)localPos.Y, 0);
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
            {
                ActiveState->Viewport->ProcessMouseButtonUp((int32)localPos.X, (int32)localPos.Y, 1);
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle))
            {
                ActiveState->Viewport->ProcessMouseButtonUp((int32)localPos.X, (int32)localPos.Y, 2);
            }
        }
    }

    ImGui::EndChild(); // CenterViewport

    ImGui::SameLine(0, 0);

    // Right Panel
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::BeginChild("RightPanel", ImVec2(rightWidth, centerHeight), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    // Display selected bone properties
    if (ActiveState->SelectedBoneIndex >= 0 && ActiveState->CurrentMesh)
    {
        const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
        if (Skeleton && ActiveState->SelectedBoneIndex < Skeleton->Bones.size())
        {
            const FBone& SelectedBone = Skeleton->Bones[ActiveState->SelectedBoneIndex];

            // Selected bone header
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.90f, 0.40f, 1.0f));
            ImGui::Text("> Selected Bone");
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.95f, 1.00f, 1.0f));
            ImGui::TextWrapped("%s", SelectedBone.Name.c_str());
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.45f, 0.55f, 0.70f, 0.8f));
            ImGui::Separator();
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // Bone Index
            ImGui::Text("Bone Index: %d", ActiveState->SelectedBoneIndex);

            // Parent Index
            if (SelectedBone.ParentIndex >= 0)
            {
                ImGui::Text("Parent Index: %d", SelectedBone.ParentIndex);
                if (SelectedBone.ParentIndex < Skeleton->Bones.size())
                {
                    ImGui::TextDisabled("  (%s)", Skeleton->Bones[SelectedBone.ParentIndex].Name.c_str());
                }
            }
            else
            {
                ImGui::Text("Parent: (Root)");
            }

            ImGui::Spacing();
            ImGui::TextDisabled("(Bone properties)");
            ImGui::Spacing();
        }
    }
    else
    {
        ImGui::TextDisabled("No bone selected");
    }

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

    // Timeline Panel (Top 60% of bottom area - Notify section)
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::BeginChild("TimelinePanel", ImVec2(centerWidth + rightWidth, bottomHeight * 0.6f), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    float WindowWidth = ImGui::GetWindowWidth();
    float WindowHeight = ImGui::GetWindowHeight();

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    // Layout: Left side = Notify tracks + controls, Right side = Timeline
    const float LeftControlWidth = 280.0f; // Increased width for controls
    const float RightTimelineWidth = WindowWidth - LeftControlWidth;

    // === LEFT SIDE: Notify Tracks + Playback Controls ===
    ImGui::BeginChild("LeftControlArea", ImVec2(LeftControlWidth, WindowHeight), false, ImGuiWindowFlags_NoScrollbar);

    // Top part: Notify Tracks
    float controlsHeight = 75.0f; // Reduced to increase Track list height
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

    // Header area - exact height to match timeline
    float headerStartY = ImGui::GetCursorPosY();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.85f, 0.9f, 1.0f));
    ImGui::SetCursorPosY(headerStartY + 5.0f); // Center text vertically
    ImGui::Text("Tracks");
    ImGui::PopStyleColor();
    // Force exact header height alignment
    ImGui::SetCursorPosY(headerStartY + HeaderHeight);

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
        sprintf_s(newTrackName, "Track %d", NotifyTracks.Num() + 1);
        NewTrack.Name = newTrackName;
        NotifyTracks.Add(NewTrack);
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

    float buttonSize = 20.0f; // Reduced for compact layout
    float spacing = 4.0f; // Increased spacing between buttons
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));

    // Align to left with small margin
    ImGui::SetCursorPosX(3.0f);
    ImGui::SetCursorPosY(5.0f); // Reduced vertical position

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

    // === Second Row: Reverse Play + Speed Control ===
    ImGui::SetCursorPosX(3.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f); // Move to next line with spacing

    // Reverse Play Checkbox
    bool bReversePlay = AnimSequence->bReversePlay;
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.3f, 1.0f)); // Orange text
    if (ImGui::Checkbox("Rev##Reverse", &bReversePlay))
    {
        AnimSequence->bReversePlay = bReversePlay;
        UE_LOG("Reverse Play: %s", bReversePlay ? "ON" : "OFF");
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(bReversePlay ? "Reverse Play: ON" : "Reverse Play: OFF");

    // Speed control (same line as Rev)
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
    ImGui::BeginChild("TimelineArea", ImVec2(RightTimelineWidth, WindowHeight), false);

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

    // Reserve space for scrolling - ensure ImGui knows the full content height
    // Use Dummy to properly extend the scrollable area
    float ContentHeight = TimeLineHeight + 70.0f; // Timeline + frame info + padding
    ImGui::Dummy(ImVec2(0.0f, ContentHeight));

    ImGui::EndChild(); // TimelineArea

    ImGui::EndChild(); // TimelinePanel

    // === CURVE EDITOR PANEL (Bottom 40% of bottom area) ===
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::BeginChild("CurveEditorPanel", ImVec2(centerWidth + rightWidth, bottomHeight * 0.4f), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    float CurveWindowWidth = ImGui::GetWindowWidth();
    float CurveWindowHeight = ImGui::GetWindowHeight();

    // Layout: Left side = Curve list, Right side = Curve graph
    const float CurveListWidth = 200.0f;
    const float CurveGraphWidth = CurveWindowWidth - CurveListWidth;

    // === LEFT SIDE: Curve List ===
    ImGui::BeginChild("CurveList", ImVec2(CurveListWidth, CurveWindowHeight), false);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.85f, 0.9f, 1.0f));
    ImGui::Text("Animation Curves");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    // Get CurveData from AnimSequence
    FAnimationCurveData& CurveData = AnimSequence->CurveData;

    // State variables
    static int SelectedCurveIndex = -1;
    static char NewCurveName[64] = "";
    static bool bShouldOpenRenamePopup = false;
    static int RenameCurveIndex = -1;
    static char RenameCurveBuffer[64] = "";

    // Curve list
    ImGui::BeginChild("CurveListScroll", ImVec2(CurveListWidth - 10.0f, CurveWindowHeight - 100.0f), true);
    for (int i = 0; i < CurveData.FloatCurves.Num(); i++)
    {
        const FFloatCurve& Curve = CurveData.FloatCurves[i];
        bool isSelected = (i == SelectedCurveIndex);
        char label[128];
        sprintf_s(label, "%s##Curve%d", Curve.CurveName.ToString().c_str(), i);

        if (ImGui::Selectable(label, isSelected))
        {
            SelectedCurveIndex = i;
        }

        // Context menu for curve
        ImGui::PushID(i);
        if (ImGui::BeginPopupContextItem("CurveContextMenu"))
        {
            if (ImGui::MenuItem("Rename Curve"))
            {
                RenameCurveIndex = i;
                strncpy_s(RenameCurveBuffer, Curve.CurveName.ToString().c_str(), sizeof(RenameCurveBuffer) - 1);
                bShouldOpenRenamePopup = true;
            }
            if (ImGui::MenuItem("Delete Curve"))
            {
                CurveData.FloatCurves.RemoveAt(i);
                if (SelectedCurveIndex == i)
                {
                    SelectedCurveIndex = -1;
                }
                else if (SelectedCurveIndex > i)
                {
                    SelectedCurveIndex--;
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::EndChild(); // CurveListScroll

    // Open Rename Popup if flagged
    if (bShouldOpenRenamePopup)
    {
        ImGui::OpenPopup("RenameCurvePopup");
        bShouldOpenRenamePopup = false;
    }

    // Rename Curve Popup
    if (ImGui::BeginPopup("RenameCurvePopup"))
    {
        ImGui::Text("Rename Curve");
        ImGui::Separator();

        bool bPressedEnter = ImGui::InputText("Name", RenameCurveBuffer, sizeof(RenameCurveBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Spacing();

        if (ImGui::Button("OK", ImVec2(80, 0)) || bPressedEnter)
        {
            if (RenameCurveIndex >= 0 && RenameCurveIndex < CurveData.FloatCurves.Num())
            {
                FName NewName = FName(RenameCurveBuffer);
                CurveData.FloatCurves[RenameCurveIndex].CurveName = NewName;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::Spacing();

    // Create Curve button
    if (ImGui::Button("Create Curve", ImVec2(CurveListWidth - 20.0f, 0)))
    {
        ImGui::OpenPopup("CreateCurvePopup");
    }

    // Create Curve Popup
    if (ImGui::BeginPopup("CreateCurvePopup"))
    {
        ImGui::Text("Create New Curve");
        ImGui::Separator();

        bool bPressedEnter = ImGui::InputText("Curve Name", NewCurveName, sizeof(NewCurveName), ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Spacing();

        if (ImGui::Button("Create", ImVec2(80, 0)) || bPressedEnter)
        {
            if (strlen(NewCurveName) > 0)
            {
                FName CurveName = FName(NewCurveName);
                FFloatCurve NewCurve(CurveName);

                // Auto-add first/last keyframes
                NewCurve.Keys.Add(FFloatKey(0.0f, 0.0f));
                NewCurve.Keys.Add(FFloatKey(AnimationLength, 0.0f));

                CurveData.AddCurve(std::move(NewCurve));
                memset(NewCurveName, 0, sizeof(NewCurveName));
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::EndChild(); // CurveList

    ImGui::SameLine();

    // === RIGHT SIDE: Curve Graph ===
    ImGui::BeginChild("CurveGraph", ImVec2(CurveGraphWidth, CurveWindowHeight), false);

    if (SelectedCurveIndex >= 0 && SelectedCurveIndex < CurveData.FloatCurves.Num())
    {
        FFloatCurve& SelectedCurve = CurveData.FloatCurves[SelectedCurveIndex];

        ImGui::Text("Curve: %s", SelectedCurve.CurveName.ToString().c_str());
        ImGui::Separator();

        // Range controls
        static float minVal = 0.0f;
        static float maxVal = 1.0f;
        static bool bAutoRange = false;

        ImGui::Checkbox("Auto Range", &bAutoRange);
        if (bAutoRange)
        {
            // Calculate min/max from keyframes
            if (!SelectedCurve.Keys.IsEmpty())
            {
                minVal = SelectedCurve.Keys[0].Value;
                maxVal = SelectedCurve.Keys[0].Value;
                for (const FFloatKey& Key : SelectedCurve.Keys)
                {
                    minVal = FMath::Min(minVal, Key.Value);
                    maxVal = FMath::Max(maxVal, Key.Value);
                }
                // Add padding
                float range = maxVal - minVal;
                if (range < 0.01f) range = 0.01f;
                minVal -= range * 0.1f;
                maxVal += range * 0.1f;
            }
        }
        else
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            ImGui::DragFloat("Min", &minVal, 0.01f);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            ImGui::DragFloat("Max", &maxVal, 0.01f);
        }

        ImGui::Spacing();

        // Graph area
        ImVec2 graphMin = ImGui::GetCursorScreenPos();
        ImVec2 graphSize(CurveGraphWidth - 20.0f, CurveWindowHeight - 80.0f);
        ImVec2 graphMax(graphMin.x + graphSize.x, graphMin.y + graphSize.y);

        ImDrawList* DrawList = ImGui::GetWindowDrawList();

        // Background
        DrawList->AddRectFilled(graphMin, graphMax, IM_COL32(30, 30, 35, 255));
        DrawList->AddRect(graphMin, graphMax, IM_COL32(100, 100, 120, 255));

        // Grid lines
        const int numGridLines = 5;
        for (int i = 0; i <= numGridLines; i++)
        {
            float t = (float)i / numGridLines;
            float y = graphMin.y + t * graphSize.y;
            DrawList->AddLine(ImVec2(graphMin.x, y), ImVec2(graphMax.x, y), IM_COL32(60, 60, 70, 100));

            float x = graphMin.x + t * graphSize.x;
            DrawList->AddLine(ImVec2(x, graphMin.y), ImVec2(x, graphMax.y), IM_COL32(60, 60, 70, 100));
        }

        // Draw curve line
        if (SelectedCurve.Keys.Num() >= 2)
        {
            for (int i = 0; i < SelectedCurve.Keys.Num() - 1; i++)
            {
                const FFloatKey& Key1 = SelectedCurve.Keys[i];
                const FFloatKey& Key2 = SelectedCurve.Keys[i + 1];

                float t1 = Key1.Time / AnimationLength;
                float v1 = (Key1.Value - minVal) / (maxVal - minVal);
                v1 = 1.0f - v1; // Invert Y

                float t2 = Key2.Time / AnimationLength;
                float v2 = (Key2.Value - minVal) / (maxVal - minVal);
                v2 = 1.0f - v2;

                ImVec2 p1(graphMin.x + t1 * graphSize.x, graphMin.y + v1 * graphSize.y);
                ImVec2 p2(graphMin.x + t2 * graphSize.x, graphMin.y + v2 * graphSize.y);

                DrawList->AddLine(p1, p2, IM_COL32(100, 200, 255, 255), 2.0f);
            }
        }

        // Draw keyframes and handle interaction
        static int DraggedKeyIndex = -1;
        static int HoveredKeyIndex = -1;
        static int ContextMenuKeyIndex = -1;
        static bool bOpenEditKeyframePopup = false;
        static float EditKeyTime = 0.0f;
        static float EditKeyValue = 0.0f;

        ImGui::SetCursorScreenPos(graphMin);
        ImGui::InvisibleButton("GraphArea", graphSize);
        bool bGraphHovered = ImGui::IsItemHovered();

        HoveredKeyIndex = -1;
        for (int i = 0; i < SelectedCurve.Keys.Num(); i++)
        {
            const FFloatKey& Key = SelectedCurve.Keys[i];

            float t = Key.Time / AnimationLength;
            float v = (Key.Value - minVal) / (maxVal - minVal);
            v = 1.0f - v;

            ImVec2 keyPos(graphMin.x + t * graphSize.x, graphMin.y + v * graphSize.y);

            // Draw keyframe
            DrawList->AddCircleFilled(keyPos, 5.0f, IM_COL32(255, 200, 100, 255));
            DrawList->AddCircle(keyPos, 5.0f, IM_COL32(255, 255, 255, 255), 0, 1.5f);

            // Check if mouse is hovering over this keyframe
            ImVec2 mousePos = ImGui::GetMousePos();
            float dist = sqrtf((mousePos.x - keyPos.x) * (mousePos.x - keyPos.x) + (mousePos.y - keyPos.y) * (mousePos.y - keyPos.y));
            if (dist < 8.0f)
            {
                HoveredKeyIndex = i;
            }
        }

        // Start dragging
        if (HoveredKeyIndex >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            DraggedKeyIndex = HoveredKeyIndex;
        }

        // Drag keyframe
        if (DraggedKeyIndex >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            FFloatKey& DraggedKey = SelectedCurve.Keys[DraggedKeyIndex];
            bool bIsDraggingFirstOrLastKey = (DraggedKeyIndex == 0 || DraggedKeyIndex == SelectedCurve.Keys.Num() - 1);

            ImVec2 mousePos = ImGui::GetMousePos();
            float normalizedTime = (mousePos.x - graphMin.x) / graphSize.x;
            float normalizedValue = (mousePos.y - graphMin.y) / graphSize.y;
            normalizedValue = 1.0f - normalizedValue;

            // Only allow Time change if not first/last key
            if (!bIsDraggingFirstOrLastKey)
            {
                normalizedTime = FMath::Clamp(normalizedTime, 0.0f, 1.0f);
                DraggedKey.Time = normalizedTime * AnimationLength;
            }

            // Always allow Value change
            normalizedValue = FMath::Clamp(normalizedValue, 0.0f, 1.0f);
            DraggedKey.Value = minVal + normalizedValue * (maxVal - minVal);
        }

        // Stop dragging
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            if (DraggedKeyIndex >= 0)
            {
                // Sort keys by time
                std::sort(SelectedCurve.Keys.begin(), SelectedCurve.Keys.end(), [](const FFloatKey& a, const FFloatKey& b) {
                    return a.Time < b.Time;
                });
            }
            DraggedKeyIndex = -1;
        }

        // Right-click menu
        if (HoveredKeyIndex >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            ContextMenuKeyIndex = HoveredKeyIndex;
            ImGui::OpenPopup("KeyframeContextMenu");
        }

        if (ImGui::BeginPopup("KeyframeContextMenu"))
        {
            bool bIsFirstOrLastKey = (ContextMenuKeyIndex == 0 || ContextMenuKeyIndex == SelectedCurve.Keys.Num() - 1);

            if (ImGui::MenuItem("Edit Keyframe"))
            {
                if (ContextMenuKeyIndex >= 0 && ContextMenuKeyIndex < SelectedCurve.Keys.Num())
                {
                    EditKeyTime = SelectedCurve.Keys[ContextMenuKeyIndex].Time;
                    EditKeyValue = SelectedCurve.Keys[ContextMenuKeyIndex].Value;
                    bOpenEditKeyframePopup = true;
                }
            }

            if (bIsFirstOrLastKey)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                ImGui::MenuItem("Delete Keyframe (First/Last key)", nullptr, false, false);
                ImGui::PopStyleColor();
            }
            else if (ImGui::MenuItem("Delete Keyframe"))
            {
                if (ContextMenuKeyIndex >= 0 && ContextMenuKeyIndex < SelectedCurve.Keys.Num())
                {
                    SelectedCurve.Keys.RemoveAt(ContextMenuKeyIndex);
                    ContextMenuKeyIndex = -1;
                }
            }

            ImGui::EndPopup();
        }

        // Open Edit Keyframe popup if flagged
        if (bOpenEditKeyframePopup)
        {
            ImGui::OpenPopup("EditKeyframePopup");
            bOpenEditKeyframePopup = false;
        }

        // Edit Keyframe Popup
        if (ImGui::BeginPopup("EditKeyframePopup"))
        {
            bool bIsEditingFirstOrLastKey = (ContextMenuKeyIndex == 0 || ContextMenuKeyIndex == SelectedCurve.Keys.Num() - 1);

            ImGui::Text("Edit Keyframe");
            ImGui::Separator();

            ImGui::Text("Time (seconds):");
            if (bIsEditingFirstOrLastKey)
            {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }
            ImGui::InputFloat("##KeyTime", &EditKeyTime, 0.01f, 0.1f, "%.3f");
            if (bIsEditingFirstOrLastKey)
            {
                ImGui::PopStyleVar();
                ImGui::PopItemFlag();
                ImGui::SameLine();
                ImGui::TextDisabled("(Fixed)");
            }

            ImGui::Text("Value:");
            ImGui::InputFloat("##KeyValue", &EditKeyValue, 0.01f, 0.1f, "%.3f");

            ImGui::Spacing();

            if (ImGui::Button("OK", ImVec2(80, 0)))
            {
                if (ContextMenuKeyIndex >= 0 && ContextMenuKeyIndex < SelectedCurve.Keys.Num())
                {
                    // Only update Time if not first/last key
                    if (!bIsEditingFirstOrLastKey)
                    {
                        SelectedCurve.Keys[ContextMenuKeyIndex].Time = FMath::Clamp(EditKeyTime, 0.0f, AnimationLength);
                    }
                    SelectedCurve.Keys[ContextMenuKeyIndex].Value = EditKeyValue;

                    // Sort keys by time
                    std::sort(SelectedCurve.Keys.begin(), SelectedCurve.Keys.end(), [](const FFloatKey& a, const FFloatKey& b) {
                        return a.Time < b.Time;
                    });
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        // Right-click on empty area to add keyframe
        if (bGraphHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && HoveredKeyIndex < 0)
        {
            ImGui::OpenPopup("AddKeyframeMenu");
        }

        if (ImGui::BeginPopup("AddKeyframeMenu"))
        {
            if (ImGui::MenuItem("Add Keyframe"))
            {
                ImVec2 mousePos = ImGui::GetMousePos();
                float normalizedTime = (mousePos.x - graphMin.x) / graphSize.x;
                float normalizedValue = (mousePos.y - graphMin.y) / graphSize.y;
                normalizedValue = 1.0f - normalizedValue;

                normalizedTime = FMath::Clamp(normalizedTime, 0.0f, 1.0f);
                normalizedValue = FMath::Clamp(normalizedValue, 0.0f, 1.0f);

                float newTime = normalizedTime * AnimationLength;
                float newValue = minVal + normalizedValue * (maxVal - minVal);

                SelectedCurve.Keys.Add(FFloatKey(newTime, newValue));

                // Sort keys by time
                std::sort(SelectedCurve.Keys.begin(), SelectedCurve.Keys.end(), [](const FFloatKey& a, const FFloatKey& b) {
                    return a.Time < b.Time;
                });
            }

            ImGui::EndPopup();
        }
    }
    else
    {
        ImGui::TextDisabled("Select a curve to edit");
    }

    ImGui::EndChild(); // CurveGraph

    ImGui::EndChild(); // CurveEditorPanel

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

        // Sync bone line visibility (NOTE: ClearLines() removed to avoid race condition with rendering)
        if (auto* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
        {
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
    // If window is closed, collapsed, or not visible, hide the viewport by resizing to 0
    if (!bIsOpen || !bShouldRenderViewport)
    {
        if (ActiveState && ActiveState->Viewport)
        {
            ActiveState->Viewport->Resize(0, 0, 0, 0);
            ActiveState->Viewport->SetRenderTarget(nullptr);
        }
        return;
    }

    if (ActiveState && ActiveState->Viewport && CenterRect.GetWidth() > 0 && CenterRect.GetHeight() > 0)
    {
        // 뷰포트 크기와 화면 위치
        const uint32 NewStartX = static_cast<uint32>(CenterRect.Left);
        const uint32 NewStartY = static_cast<uint32>(CenterRect.Top);
        const uint32 NewWidth = static_cast<uint32>(CenterRect.GetWidth());
        const uint32 NewHeight = static_cast<uint32>(CenterRect.GetHeight());

        // 렌더 타겟 생성/리사이즈
        URenderer* Renderer = URenderManager::GetInstance().GetRenderer();
        if (Renderer)
        {
            D3D11RHI* RHIDevice = Renderer->GetRHIDevice();
            if (RHIDevice)
            {
                RHIDevice->ResizeViewportRenderTarget(NewWidth, NewHeight, ViewportRenderTarget);
                ActiveState->Viewport->SetRenderTarget(&ViewportRenderTarget);
            }
        }

        // Viewport 크기와 위치 설정
        // StartX/Y는 화면 좌표 (GPU 피킹용), SizeX/Y는 렌더 타겟 크기
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

        // First, always try gizmo picking (pass to viewport)
        ActiveState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

        // Left click: if no gizmo was picked, try bone picking
        if (Button == 0 && ActiveState->PreviewActor && ActiveState->CurrentMesh && ActiveState->Client && ActiveState->World)
        {
            // Check if gizmo was picked by checking selection
            UActorComponent* SelectedComp = ActiveState->World->GetSelectionManager()->GetSelectedComponent();

            // Only do bone picking if gizmo wasn't selected
            if (!SelectedComp || !Cast<UBoneAnchorComponent>(SelectedComp))
            {
                // Get camera from viewport client
                ACameraActor* Camera = ActiveState->Client->GetCamera();
                if (Camera)
                {
                    // Get camera vectors
                    FVector CameraPos = Camera->GetActorLocation();
                    FVector CameraRight = Camera->GetRight();
                    FVector CameraUp = Camera->GetUp();
                    FVector CameraForward = Camera->GetForward();

                    // Calculate viewport-relative mouse position
                    FVector2D ViewportMousePos(MousePos.X - CenterRect.Left, MousePos.Y - CenterRect.Top);
                    FVector2D ViewportSize(CenterRect.GetWidth(), CenterRect.GetHeight());

                    // Generate ray from mouse position
                    FRay Ray = MakeRayFromViewport(
                        Camera->GetViewMatrix(),
                        Camera->GetProjectionMatrix(CenterRect.GetWidth() / CenterRect.GetHeight(), ActiveState->Viewport),
                        CameraPos,
                        CameraRight,
                        CameraUp,
                        CameraForward,
                        ViewportMousePos,
                        ViewportSize
                    );

                    // Try to pick a bone
                    float HitDistance;
                    int32 PickedBoneIndex = ActiveState->PreviewActor->PickBone(Ray, HitDistance);

                    if (PickedBoneIndex >= 0)
                    {
                        // Bone was picked
                        ActiveState->SelectedBoneIndex = PickedBoneIndex;
                        ActiveState->bBoneLinesDirty = true;

                        ExpandToSelectedBone(ActiveState, PickedBoneIndex);

                        // Move gizmo to the selected bone
                        ActiveState->PreviewActor->RepositionAnchorToBone(PickedBoneIndex);
                        if (USceneComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                        {
                            ActiveState->World->GetSelectionManager()->SelectActor(ActiveState->PreviewActor);
                            ActiveState->World->GetSelectionManager()->SelectComponent(Anchor);
                        }
                    }
                    else
                    {
                        // No bone was picked - clear selection
                        ActiveState->SelectedBoneIndex = -1;
                        ActiveState->bBoneLinesDirty = true;

                        // Hide gizmo and clear selection
                        if (UBoneAnchorComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                        {
                            Anchor->SetVisibility(false);
                            Anchor->SetEditability(false);
                        }
                        ActiveState->World->GetSelectionManager()->ClearSelection();
                    }
                }
            }
        }
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

void SAnimSequenceEditorWindow::ExpandToSelectedBone(AnimSequenceEditorState* State, int32 BoneIndex)
{
    if (!State || !State->CurrentMesh)
        return;

    const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= Skeleton->Bones.size())
        return;

    // Expand all parent bones up to the selected bone
    int32 CurrentIndex = BoneIndex;
    while (CurrentIndex >= 0)
    {
        State->ExpandedBoneIndices.insert(CurrentIndex);
        CurrentIndex = Skeleton->Bones[CurrentIndex].ParentIndex;
    }
}
