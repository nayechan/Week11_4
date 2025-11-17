#include "pch.h"
#include "SSkeletalMeshViewerWindow.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Source/Runtime/Engine/SkeletalViewer/SkeletalViewerBootstrap.h"
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

SSkeletalMeshViewerWindow::SSkeletalMeshViewerWindow()
{
    CenterRect = FRect(0, 0, 0, 0);
}

SSkeletalMeshViewerWindow::~SSkeletalMeshViewerWindow()
{
    // Clean up tabs if any
    for (int i = 0; i < Tabs.Num(); ++i)
    {
        ViewerState* State = Tabs[i];
        SkeletalViewerBootstrap::DestroyViewerState(State);
    }
    Tabs.Empty();
    ActiveState = nullptr;
}

bool SSkeletalMeshViewerWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice)
{
    World = InWorld;
    Device = InDevice;

    IconPause = UResourceManager::GetInstance().Load<UTexture>(GDataDir + "/Icon/Pause.png");
    IconResume = UResourceManager::GetInstance().Load<UTexture>(GDataDir + "/Icon/Resume.png");
    
    SetRect(StartX, StartY, StartX + Width, StartY + Height);

    // Create first tab/state
    OpenNewTab("Viewer 1");
    if (ActiveState && ActiveState->Viewport)
    {
        ActiveState->Viewport->Resize((uint32)StartX, (uint32)StartY, (uint32)Width, (uint32)Height);
    }

    bRequestFocus = true;
    return true;
}

void SSkeletalMeshViewerWindow::OnRender()
{
    // If window is closed, don't render
    if (!bIsOpen)
    {
        return;
    }

    // Parent detachable window (movable, top-level) with solid background
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
    bool bViewerVisible = false;
    if (ImGui::Begin("Skeletal Mesh Viewer", &bIsOpen, flags))
    {
        bViewerVisible = true;
        // Render tab bar and switch active state
        if (ImGui::BeginTabBar("SkeletalViewerTabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
        {
            for (int i = 0; i < Tabs.Num(); ++i)
            {
                ViewerState* State = Tabs[i];
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
                char label[32]; sprintf_s(label, "Viewer %d", Tabs.Num() + 1);
                OpenNewTab(label);
            }
            ImGui::EndTabBar();
        }
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        Rect.Left = pos.x; Rect.Top = pos.y; Rect.Right = pos.x + size.x; Rect.Bottom = pos.y + size.y; Rect.UpdateMinMax();

        ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        float totalWidth = contentAvail.x;
        float totalHeight = contentAvail.y;

        float leftWidth = totalWidth * LeftPanelRatio;
        float rightWidth = totalWidth * RightPanelRatio;
        float BottomHeight = totalHeight * BottomPanelRatio;
        float centerWidth = totalWidth - leftWidth - rightWidth;
        float centerHeight = totalHeight;

        // Ensure viewport dimensions are at least 1 pixel (prevent zero/negative)
        centerWidth = FMath::Max(centerWidth, 1.0f);

        if (ActiveState && ActiveState->bViewAnimation)
        {
            centerHeight = totalHeight - BottomHeight;
        }

        centerHeight = FMath::Max(centerHeight, 1.0f);

        // Remove spacing between panels
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        // Left panel - Asset Browser & Bone Hierarchy
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, totalHeight), true, ImGuiWindowFlags_NoScrollbar);

        if (ActiveState)
        {
            // Asset Browser Section
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

            // Mesh path section
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
            if (ImGui::Button("Browse...", ImVec2(buttonWidth, 32)))
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
                    //USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
                    //if (Mesh && ActiveState->PreviewActor)
                    //{
                    //    ActiveState->PreviewActor->SetSkeletalMesh(Path);
                    //    ActiveState->CurrentMesh = Mesh;
                    //    ActiveState->LoadedMeshPath = Path;  // Track for resource unloading
                    //    if (auto* Skeletal = ActiveState->PreviewActor->GetSkeletalMeshComponent())
                    //    {
                    //        Skeletal->SetVisibility(ActiveState->bShowMesh);
                    //    }
                    //    ActiveState->bBoneLinesDirty = true;
                    //    if (auto* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
                    //    {
                    //        LineComp->ClearLines();
                    //        LineComp->SetLineVisible(ActiveState->bShowBones);
                    //    }
                    //}
                }
            }
            ImGui::PopStyleColor(6);
            ImGui::EndGroup();

            ImGui::Spacing();

            // Animation Path Section
            ImGui::BeginGroup();
            
            ImGui::Text("Animation Path:");
            ImGui::PushItemWidth(-1.0f);
            ImGui::InputTextWithHint("##AnimationPath", "Browse for Animation file...", ActiveState->AnimationPathBuffer, sizeof(ActiveState->AnimationPathBuffer));
            ImGui::PopItemWidth();

            ImGui::Spacing();

            // Buttons
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.40f, 0.55f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.50f, 0.70f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.35f, 0.50f, 1.0f));

            if (ImGui::Button("Browse...##Animation", ImVec2(buttonWidth, 32)))
            {
                auto widePath = FPlatformProcess::OpenLoadFileDialog(UTF8ToWide(GDataDir), L"fbx", L"FBX Files");
                if (!widePath.empty())
                {
                    std::string s = widePath.string();
                    strncpy_s(ActiveState->AnimationPathBuffer, s.c_str(), sizeof(ActiveState->AnimationPathBuffer) - 1);
                }
            }
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.60f, 0.40f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.70f, 0.50f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.50f, 0.30f, 1.0f));
            if (ImGui::Button("Load Animation", ImVec2(buttonWidth, 32)))
            {
                FString Path = ActiveState->AnimationPathBuffer;
                if (!Path.empty())
                {
                    char Label[32]; sprintf_s(Label, "Viewer %d", Tabs.Num() + 1);
                    OpenNewTab(Label);
                    LoadSkeletalMesh(Path);
                    USkeletalMeshComponent* SkeletalMeshComponent = ActiveState->PreviewActor->GetSkeletalMeshComponent();
                    UAnimSequence* AnimSequence = UFbxLoader::GetInstance().LoadFbxAnimation(Path, SkeletalMeshComponent->GetSkeletalMesh()->GetSkeleton());
                    if (AnimSequence)
                    {
                        UAnimSingleNodeInstance* AnimInstance = NewObject<UAnimSingleNodeInstance>();
                        AnimInstance->SetAnimationAsset(AnimSequence);
                        SkeletalMeshComponent->SetAnimInstance(AnimInstance);
                        ActiveState->bViewAnimation = true;
                    }
                }
            }
            ImGui::PopStyleColor(6);
            ImGui::EndGroup();

            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            // Display Options
            ImGui::BeginGroup();
            ImGui::Text("Display Options:");
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.25f, 0.30f, 0.35f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.40f, 0.70f, 1.00f, 1.0f));

            if (ImGui::Checkbox("Show Mesh", &ActiveState->bShowMesh))
            {
                if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkeletalMeshComponent())
                {
                    ActiveState->PreviewActor->GetSkeletalMeshComponent()->SetVisibility(ActiveState->bShowMesh);
                }
            }

            ImGui::SameLine();
            if (ImGui::Checkbox("Show Bones", &ActiveState->bShowBones))
            {
                if (ActiveState->PreviewActor && ActiveState->PreviewActor->GetBoneLineComponent())
                {
                    ActiveState->PreviewActor->GetBoneLineComponent()->SetLineVisible(ActiveState->bShowBones);
                }
                if (ActiveState->bShowBones)
                {
                    ActiveState->bBoneLinesDirty = true;
                }
            }
            ImGui::PopStyleColor(2);
            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            // Bone Hierarchy Section
            ImGui::Text("Bone Hierarchy:");
            ImGui::Spacing();

            if (!ActiveState->CurrentMesh)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::TextWrapped("No skeletal mesh loaded.");
                ImGui::PopStyleColor();
            }
            else
            {
                const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
                if (!Skeleton || Skeleton->Bones.IsEmpty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                    ImGui::TextWrapped("This mesh has no skeleton data.");
                    ImGui::PopStyleColor();
                }
                else
                {
                    // Scrollable tree view
                    ImGui::BeginChild("BoneTreeView", ImVec2(0, 0), true);
                    const TArray<FBone>& Bones = Skeleton->Bones;
                    TArray<TArray<int32>> Children;
                    Children.resize(Bones.size());
                    for (int32 i = 0; i < Bones.size(); ++i)
                    {
                        int32 Parent = Bones[i].ParentIndex;
                        if (Parent >= 0 && Parent < Bones.size())
                        {
                            Children[Parent].Add(i);
                        }
                    }

                    std::function<void(int32)> DrawNode = [&](int32 Index)
                    {
                        const bool bLeaf = Children[Index].IsEmpty();
                        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
                        
                        if (bLeaf)
                        {
                            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                        }
                        
                        // 펼쳐진 노드는 명시적으로 열린 상태로 설정
                        if (ActiveState->ExpandedBoneIndices.count(Index) > 0)
                        {
                            ImGui::SetNextItemOpen(true);
                        }
                        
                        if (ActiveState->SelectedBoneIndex == Index)
                        {
                            flags |= ImGuiTreeNodeFlags_Selected;
                        }

                        ImGui::PushID(Index);
                        const char* Label = Bones[Index].Name.c_str();

                        if (ActiveState->SelectedBoneIndex == Index)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.35f, 0.55f, 0.85f, 0.8f));
                            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.40f, 0.60f, 0.90f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.50f, 0.80f, 1.0f));
                        }

                        bool open = ImGui::TreeNodeEx((void*)(intptr_t)Index, flags, "%s", Label ? Label : "<noname>");

                        if (ActiveState->SelectedBoneIndex == Index)
                        {
                            ImGui::PopStyleColor(3);
                            
                            // 선택된 본까지 스크롤
                            ImGui::SetScrollHereY(0.5f);
                        }

                        // 사용자가 수동으로 노드를 접거나 펼쳤을 때 상태 업데이트
                        if (ImGui::IsItemToggledOpen())
                        {
                            if (open)
                                ActiveState->ExpandedBoneIndices.insert(Index);
                            else
                                ActiveState->ExpandedBoneIndices.erase(Index);
                        }

                        if (ImGui::IsItemClicked())
                        {
                            if (ActiveState->SelectedBoneIndex != Index)
                            {
                                ActiveState->SelectedBoneIndex = Index;
                                ActiveState->bBoneLinesDirty = true;
                                
                                ExpandToSelectedBone(ActiveState, Index);

                                if (ActiveState->PreviewActor && ActiveState->World)
                                {
                                    ActiveState->PreviewActor->RepositionAnchorToBone(Index);
                                    if (USceneComponent* Anchor = ActiveState->PreviewActor->GetBoneGizmoAnchor())
                                    {
                                        ActiveState->World->GetSelectionManager()->SelectActor(ActiveState->PreviewActor);
                                        ActiveState->World->GetSelectionManager()->SelectComponent(Anchor);
                                    }
                                }
                            }
                        }
                        
                        if (!bLeaf && open)
                        {
                            for (int32 Child : Children[Index])
                            {
                                DrawNode(Child);
                            }
                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    };

                    for (int32 i = 0; i < Bones.size(); ++i)
                    {
                        if (Bones[i].ParentIndex < 0)
                        {
                            DrawNode(i);
                        }
                    }

                    ImGui::EndChild();
                }
            }
        }
        else
        {

            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::End();
            return;
        }
        ImGui::EndChild();

        ImGui::SameLine(0, 0); // No spacing between panels

        // Center panel (viewport area) — draw with border to see the viewport area
        ImGui::BeginChild("ViewportAndAnimSequence", ImVec2(centerWidth, totalHeight), false, ImGuiWindowFlags_NoScrollbar);
        ImGui::BeginChild("SkeletalMeshViewport", ImVec2(centerWidth, centerHeight), true, ImGuiWindowFlags_NoScrollbar);
        ImVec2 childPos = ImGui::GetWindowPos();
        ImVec2 childSize = ImGui::GetWindowSize();
        ImVec2 rectMin = childPos;
        ImVec2 rectMax(childPos.x + childSize.x, childPos.y + childSize.y);
        CenterRect.Left = rectMin.x; CenterRect.Top = rectMin.y; CenterRect.Right = rectMax.x; CenterRect.Bottom = rectMax.y; CenterRect.UpdateMinMax();
        ImGui::EndChild();

        RenderAnimationSquenceViewer();
        ImGui::EndChild();

        ImGui::SameLine(0, 0); // No spacing between panels

        // Right panel - Bone Properties
        ImGui::BeginChild("RightPanel", ImVec2(rightWidth, totalHeight), true);

        // Panel header
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.35f, 0.50f, 0.8f));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
        ImGui::Indent(8.0f);
        ImGui::Text("Bone Properties");
        ImGui::Unindent(8.0f);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // === 선택된 본의 트랜스폼 편집 UI ===
        if (ActiveState->SelectedBoneIndex >= 0 && ActiveState->CurrentMesh)
        {
            const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
            if (Skeleton && ActiveState->SelectedBoneIndex < Skeleton->Bones.size())
            {
                const FBone& SelectedBone = Skeleton->Bones[ActiveState->SelectedBoneIndex];

                // Selected bone header with icon-like prefix
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

                // 본의 현재 트랜스폼 가져오기 (편집 중이 아닐 때만)
                if (!ActiveState->bBoneRotationEditing)
                {
                    UpdateBoneTransformFromSkeleton(ActiveState);
                }

                ImGui::Spacing();

                // Location 편집
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
                ImGui::Text("Location");
                ImGui::PopStyleColor();

                ImGui::PushItemWidth(-1);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.28f, 0.20f, 0.20f, 0.6f));
                bBoneChanged |= ImGui::DragFloat("##BoneLocX", &ActiveState->EditBoneLocation.X, 0.1f, 0.0f, 0.0f, "X: %.3f");
                bBoneChanged |= ImGui::DragFloat("##BoneLocY", &ActiveState->EditBoneLocation.Y, 0.1f, 0.0f, 0.0f, "Y: %.3f");
                bBoneChanged |= ImGui::DragFloat("##BoneLocZ", &ActiveState->EditBoneLocation.Z, 0.1f, 0.0f, 0.0f, "Z: %.3f");
                ImGui::PopStyleColor();
                ImGui::PopItemWidth();

                if (bBoneChanged)
                {
                    ApplyBoneTransform(ActiveState);
                    ActiveState->bBoneLinesDirty = true;
                }

                ImGui::Spacing();

                // Rotation 편집
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
                ImGui::Text("Rotation");
                ImGui::PopStyleColor();

                ImGui::PushItemWidth(-1);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f, 0.28f, 0.20f, 0.6f));
                bool bRotationChanged = false;

                if (ImGui::IsAnyItemActive())
                {
                    ActiveState->bBoneRotationEditing = true;
                }

                bBoneChanged |= ImGui::DragFloat("##BoneRotX", &ActiveState->EditBoneRotation.X, 0.5f, -180.0f, 180.0f, "X: %.2f°");
                bBoneChanged |= ImGui::DragFloat("##BoneRotY", &ActiveState->EditBoneRotation.Y, 0.5f, -180.0f, 180.0f, "Y: %.2f°");
                bBoneChanged |= ImGui::DragFloat("##BoneRotZ", &ActiveState->EditBoneRotation.Z, 0.5f, -180.0f, 180.0f, "Z: %.2f°");
                ImGui::PopStyleColor();
                ImGui::PopItemWidth();

                if (!ImGui::IsAnyItemActive())
                {
                    ActiveState->bBoneRotationEditing = false;
                }

                if (bBoneChanged)
                {
                    ApplyBoneTransform(ActiveState);
                    ActiveState->bBoneLinesDirty = true;
                }

                ImGui::Spacing();

                // Scale 편집
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 1.0f, 1.0f));
                ImGui::Text("Scale");
                ImGui::PopStyleColor();

                ImGui::PushItemWidth(-1);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f, 0.20f, 0.28f, 0.6f));
                bool bScaleChanged = false;
                bBoneChanged |= ImGui::DragFloat("##BoneScaleX", &ActiveState->EditBoneScale.X, 0.01f, 0.001f, 100.0f, "X: %.3f");
                bBoneChanged |= ImGui::DragFloat("##BoneScaleY", &ActiveState->EditBoneScale.Y, 0.01f, 0.001f, 100.0f, "Y: %.3f");
                bBoneChanged |= ImGui::DragFloat("##BoneScaleZ", &ActiveState->EditBoneScale.Z, 0.01f, 0.001f, 100.0f, "Z: %.3f");
                ImGui::PopStyleColor();
                ImGui::PopItemWidth();

                if (bBoneChanged)
                {
                    ApplyBoneTransform(ActiveState);
                    ActiveState->bBoneLinesDirty = true;
                }
            }
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::TextWrapped("Select a bone from the hierarchy to edit its transform properties.");
            ImGui::PopStyleColor();
        }

        ImGui::EndChild(); // RightPanel

        // Pop the ItemSpacing style
        ImGui::PopStyleVar(2);
    }
    ImGui::End();

    // If collapsed or not visible, clear the center rect so we don't render a floating viewport
    if (!bViewerVisible)
    {
        CenterRect = FRect(0, 0, 0, 0);
        CenterRect.UpdateMinMax();
    }

    // If window was closed via X button, notify the manager to clean up
    if (!bIsOpen)
    {
        USlateManager::GetInstance().CloseSkeletalMeshViewer();
    }

    bRequestFocus = false;
}

void SSkeletalMeshViewerWindow::OnUpdate(float DeltaSeconds)
{
    if (!ActiveState || !ActiveState->Viewport)
        return;

    // Space key toggles animation play/pause
    if (ActiveState->bViewAnimation && ImGui::IsKeyPressed(ImGuiKey_Space, false))
    {
        UAnimInstance* AnimInstance = ActiveState->PreviewActor->GetSkeletalMeshComponent()->AnimInstance;
        if (AnimInstance)
        {
            UAnimSingleNodeInstance* AnimSingleNodeInstance = static_cast<UAnimSingleNodeInstance*>(AnimInstance);
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

    // Tick the preview world so editor actors (e.g., gizmo) update visibility/state
    if (ActiveState->World)
    {
        ActiveState->World->Tick(DeltaSeconds);
        if (ActiveState->World->GetGizmoActor())
            ActiveState->World->GetGizmoActor()->ProcessGizmoModeSwitch();
        // 에니메이션만 틱 돌림.
        ActiveState->PreviewActor->GetSkeletalMeshComponent()->TickAnimation(DeltaSeconds);
        if (bBoneChanged)
        {
            ApplyBoneTransform(ActiveState);
            ActiveState->bBoneLinesDirty = true;
            bBoneChanged = false;
        }
    }

    if (ActiveState && ActiveState->Client)
    {
        ActiveState->Client->Tick(DeltaSeconds);
    }
}

void SSkeletalMeshViewerWindow::OnMouseMove(FVector2D MousePos)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        ActiveState->Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);
    }
}

void SSkeletalMeshViewerWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
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

void SSkeletalMeshViewerWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
    if (!ActiveState || !ActiveState->Viewport) return;

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        ActiveState->Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
    }
}

void SSkeletalMeshViewerWindow::OnRenderViewport()
{
    if (ActiveState && ActiveState->Viewport && CenterRect.GetWidth() > 0 && CenterRect.GetHeight() > 0)
    {
        // Calculate visible viewport area (clip to screen bounds)
        // If viewport goes off-screen, only render the visible portion
        const float VisibleLeft = FMath::Max(0.0f, CenterRect.Left);
        const float VisibleTop = FMath::Max(0.0f, CenterRect.Top);

        const uint32 NewStartX = static_cast<uint32>(VisibleLeft);
        const uint32 NewStartY = static_cast<uint32>(VisibleTop);
        const uint32 NewWidth  = static_cast<uint32>(FMath::Max(1.0f, CenterRect.Right - VisibleLeft));
        const uint32 NewHeight = static_cast<uint32>(FMath::Max(1.0f, CenterRect.Bottom - VisibleTop));
        ActiveState->Viewport->Resize(NewStartX, NewStartY, NewWidth, NewHeight);

        // 본 오버레이 재구축
        if (ActiveState->bShowBones)
        {
            ActiveState->bBoneLinesDirty = true;
        }
        if (ActiveState->bShowBones && ActiveState->PreviewActor && ActiveState->CurrentMesh && ActiveState->bBoneLinesDirty)
        {
            if (ULineComponent* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
            {
                LineComp->SetLineVisible(true);
            }
            if (ActiveState->SelectedBoneIndex == -1)
            {
                ActiveState->PreviewActor->RebuildBoneLines();
            }
            else
            {
                ActiveState->PreviewActor->RebuildBoneLines(ActiveState->SelectedBoneIndex);
            }   
            ActiveState->bBoneLinesDirty = false;
        }

        // 뷰포트 렌더링 (ImGui보다 먼저)
        ActiveState->Viewport->Render();
    }
}

void SSkeletalMeshViewerWindow::OpenNewTab(const char* Name)
{
    ViewerState* State = SkeletalViewerBootstrap::CreateViewerState(Name, World, Device);
    if (!State) return;

    Tabs.Add(State);
    ActiveTabIndex = Tabs.Num() - 1;
    ActiveState = State;
}

void SSkeletalMeshViewerWindow::CloseTab(int Index)
{
    if (Index < 0 || Index >= Tabs.Num()) return;
    ViewerState* State = Tabs[Index];
    SkeletalViewerBootstrap::DestroyViewerState(State);
    Tabs.RemoveAt(Index);
    if (Tabs.Num() == 0) { ActiveTabIndex = -1; ActiveState = nullptr; }
    else { ActiveTabIndex = std::min(Index, Tabs.Num() - 1); ActiveState = Tabs[ActiveTabIndex]; }
}

void SSkeletalMeshViewerWindow::RenderAnimationSquenceViewer()
{
    UAnimInstance* AnimInstance = ActiveState->PreviewActor->GetSkeletalMeshComponent()->AnimInstance;
    if (!AnimInstance)
    {
        return;
    }
    UAnimSingleNodeInstance* AnimSingleNodeInstance = static_cast<UAnimSingleNodeInstance*>(AnimInstance);

    UAnimSequence* AnimSequence = AnimSingleNodeInstance->GetAnimSequence();


    ImVec2 ContentAvail = ImGui::GetContentRegionAvail();
    float BottomHeight = ContentAvail.y;
    float BottomWidth = ContentAvail.x;

    // Timeline Panel
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::BeginChild("TimelinePanel", ImVec2(BottomWidth, BottomHeight), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    float WindowWidth = ImGui::GetWindowWidth();
    float WindowHeight = ImGui::GetWindowHeight();

    // Animation timing variables
    float CurrentInternalTime = AnimSingleNodeInstance->GetInteralTime();
    const float AnimationLength = AnimSequence->GetPlayLength();
    int CurrentFrame = static_cast<int>((CurrentInternalTime / AnimationLength) * AnimSequence->NumberOfFrames);

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    // Layout: Left side = Notify tracks + controls, Right side = Timeline
    const float LeftControlWidth = 200.0f;
    const float RightTimelineWidth = WindowWidth - LeftControlWidth;

    // === LEFT SIDE: Notify Tracks + Playback Controls ===
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::BeginChild("LeftControlArea", ImVec2(LeftControlWidth, WindowHeight), false, ImGuiWindowFlags_NoScrollbar);

    // Top part: Notify Tracks
    float controlsHeight = 80.0f; // Height for playback controls
    float tracksHeight = WindowHeight - controlsHeight - 10.0f;

    ImGui::BeginChild("NotifyTracks", ImVec2(LeftControlWidth, tracksHeight), true);

    // Initialize default track if empty
    TArray<ViewerState::NotifyTrack>& NotifyTracks = ActiveState->NotifyTracks;
    if (NotifyTracks.Num() == 0)
    {
        ViewerState::NotifyTrack DefaultTrack;
        DefaultTrack.ID = ActiveState->NextTrackID++;
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
        const ViewerState::NotifyTrack& Track = NotifyTracks[i];
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
                    ViewerState::NotifyTrack temp = NotifyTracks[sourceIdx];
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
        ViewerState::NotifyTrack NewTrack;
        NewTrack.ID = ActiveState->NextTrackID++;
        char newTrackName[64];
        sprintf_s(newTrackName, "Track %d", NewTrack.ID);
        NewTrack.Name = newTrackName;
        NotifyTracks.Add(NewTrack);
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
        bBoneChanged = true;
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
    ImGui::PopStyleVar();

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
        const ViewerState::NotifyTrack& Track = NotifyTracks[trackIdx];
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
        const ViewerState::NotifyTrack& Track = NotifyTracks[trackIdx];
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
                    const ViewerState::NotifyTrack& Track = NotifyTracks[i];
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
}

void SSkeletalMeshViewerWindow::LoadSkeletalMesh(const FString& Path)
{
    if (!ActiveState || Path.empty())
        return;

    // Load the skeletal mesh using the resource manager
    USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(Path);
    if (Mesh && ActiveState->PreviewActor)
    {
        // Set the mesh on the preview actor
        ActiveState->PreviewActor->SetSkeletalMesh(Path);
        ActiveState->CurrentMesh = Mesh;
        ActiveState->LoadedMeshPath = Path;  // Track for resource unloading

        // Update mesh path buffer for display in UI
        strncpy_s(ActiveState->MeshPathBuffer, Path.c_str(), sizeof(ActiveState->MeshPathBuffer) - 1);

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

        UE_LOG("SSkeletalMeshViewerWindow: Loaded skeletal mesh from %s", Path.c_str());
    }
    else
    {
        UE_LOG("SSkeletalMeshViewerWindow: Failed to load skeletal mesh from %s", Path.c_str());
    }
}

void SSkeletalMeshViewerWindow::UpdateBoneTransformFromSkeleton(ViewerState* State)
{
    if (!State || !State->CurrentMesh || State->SelectedBoneIndex < 0)
        return;
        
    // 본의 로컬 트랜스폼에서 값 추출
    const FTransform& BoneTransform = State->PreviewActor->GetSkeletalMeshComponent()->GetBoneLocalTransform(State->SelectedBoneIndex);
    State->EditBoneLocation = BoneTransform.Translation;
    State->EditBoneRotation = BoneTransform.Rotation.ToEulerZYXDeg();
    State->EditBoneScale = BoneTransform.Scale3D;
}

void SSkeletalMeshViewerWindow::ApplyBoneTransform(ViewerState* State)
{
    if (!State || !State->CurrentMesh || State->SelectedBoneIndex < 0)
        return;

    FTransform NewTransform(State->EditBoneLocation, FQuat::MakeFromEulerZYX(State->EditBoneRotation), State->EditBoneScale);
    State->PreviewActor->GetSkeletalMeshComponent()->SetBoneLocalTransform(State->SelectedBoneIndex, NewTransform);
}

void SSkeletalMeshViewerWindow::ExpandToSelectedBone(ViewerState* State, int32 BoneIndex)
{
    if (!State || !State->CurrentMesh)
        return;
        
    const FSkeleton* Skeleton = State->CurrentMesh->GetSkeleton();
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= Skeleton->Bones.size())
        return;
    
    // 선택된 본부터 루트까지 모든 부모를 펼침
    int32 CurrentIndex = BoneIndex;
    while (CurrentIndex >= 0)
    {
        State->ExpandedBoneIndices.insert(CurrentIndex);
        CurrentIndex = Skeleton->Bones[CurrentIndex].ParentIndex;
    }
}