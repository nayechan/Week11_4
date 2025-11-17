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

        if (ActiveState && ActiveState->bViewAnimation)
        {
            centerHeight = totalHeight - BottomHeight;

        }

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
        const uint32 NewStartX = static_cast<uint32>(CenterRect.Left);
        const uint32 NewStartY = static_cast<uint32>(CenterRect.Top);
        const uint32 NewWidth  = static_cast<uint32>(CenterRect.Right - CenterRect.Left);
        const uint32 NewHeight = static_cast<uint32>(CenterRect.Bottom - CenterRect.Top);
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


    const float NotifyAspect = 0.25f;
    ImVec2 ContentAvail = ImGui::GetContentRegionAvail();
    float NotifyWidth = ContentAvail.x * NotifyAspect;
    float BottomHeight = ContentAvail.y;
    float BottomWidth = ContentAvail.x * (1 - NotifyAspect);

    // Left Panel - Animation Info
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::BeginChild("AnimInfoPanel", ImVec2(NotifyWidth, BottomHeight), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.35f, 0.50f, 0.8f));
    const char* InfoHeaderText = "Animation Info";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(InfoHeaderText).x) * 0.5f);
    ImGui::Text(InfoHeaderText);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.45f, 0.60f, 0.7f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Animation Info Display
    float CurrentInternalTime = AnimSingleNodeInstance->GetInteralTime();
    const float AnimationLength = AnimSequence->GetPlayLength();
    int CurrentFrame = static_cast<int>((CurrentInternalTime / AnimationLength) * AnimSequence->NumberOfFrames);
    float CurrentFPS = AnimSequence->NumberOfFrames / AnimationLength;

    USkeletalMesh* CurrentMesh = ActiveState->CurrentMesh;

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.95f, 1.0f, 1.0f));
    ImGui::Text("Frame:");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.85f, 1.0f, 1.0f));

    // Extract filename from path
    FString animPath = ActiveState->AnimationPathBuffer;
    size_t lastSlash = animPath.find_last_of("/\\");
    FString animName = (lastSlash != FString::npos) ? animPath.substr(lastSlash + 1) : animPath;
    ImGui::Text("%s", animName.c_str());
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Text("LOD: 0");

    ImGui::Spacing();
    ImGui::Text("Animation LOD: 0.95");

    ImGui::Spacing();
    ImGui::Text("Frame Count: %d / %d", CurrentFrame, AnimSequence->NumberOfFrames);

    if (CurrentMesh)
    {
        ImGui::Spacing();
        // Note: You may need to add vertex/triangle count to USkeletalMesh
        ImGui::Text("Vertices: N/A");

        ImGui::Spacing();
        ImGui::Text("UV Sets: 1");
    }

    ImGui::Spacing();
    ImGui::Text("Framerate: %.0f fps", CurrentFPS);

    ImGui::PopStyleColor();

    ImGui::EndChild();

    ImGui::SameLine(0, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::BeginChild("TimelinePanel", ImVec2(BottomWidth, BottomHeight), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    float WindowWidth = ImGui::GetWindowWidth();
    float WindowHeight = ImGui::GetWindowHeight();
    // Reuse variables from AnimInfoPanel - already declared above
    CurrentInternalTime = AnimSingleNodeInstance->GetInteralTime();
    CurrentFrame = static_cast<int>((CurrentInternalTime / AnimationLength) * AnimSequence->NumberOfFrames);

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
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.85f, 0.9f, 1.0f));
    ImGui::Text("Notifies");
    ImGui::PopStyleColor();
    ImGui::Separator();

    // Notify track list (placeholder for now)
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::Selectable("Notify Track 1", false);
    ImGui::Selectable("Notify Track 2", false);
    ImGui::Selectable("Notify Track 3", false);
    ImGui::PopStyleColor();

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

    // First Frame
    if (ImGui::Button(u8"\u23EE##First", ImVec2(buttonSize, buttonSize)))
    {
        AnimSingleNodeInstance->Pause();
        AnimSingleNodeInstance->SetInteralTime(0.0f);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("First Frame");

    ImGui::SameLine();

    // Previous Frame
    if (ImGui::Button(u8"\u25C0##Prev", ImVec2(buttonSize, buttonSize)))
    {
        AnimSingleNodeInstance->Pause();
        float frameTime = AnimationLength / AnimSequence->NumberOfFrames;
        float newTime = std::max(0.0f, CurrentInternalTime - frameTime);
        AnimSingleNodeInstance->SetInteralTime(newTime);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Previous Frame");

    ImGui::SameLine();

    // Stop
    if (ImGui::Button(u8"\u23F9##Stop", ImVec2(buttonSize, buttonSize)))
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
        if (ImGui::Button(u8"\u23F8##Pause", ImVec2(buttonSize, buttonSize)))
        {
            AnimSingleNodeInstance->Pause();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Pause");
    }
    else
    {
        if (ImGui::Button(u8"\u25B6##Play", ImVec2(buttonSize, buttonSize)))
        {
            AnimSingleNodeInstance->Play(ActiveState->bLoopAnimation);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Play");
    }

    ImGui::SameLine();

    // Next Frame
    if (ImGui::Button(u8"\u25B6##Next", ImVec2(buttonSize, buttonSize)))
    {
        AnimSingleNodeInstance->Pause();
        float frameTime = AnimationLength / AnimSequence->NumberOfFrames;
        float newTime = std::min(AnimationLength, CurrentInternalTime + frameTime);
        AnimSingleNodeInstance->SetInteralTime(newTime);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Next Frame");

    ImGui::SameLine();

    // Last Frame
    if (ImGui::Button(u8"\u23ED##Last", ImVec2(buttonSize, buttonSize)))
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
    if (ImGui::Button(u8"\u21BB##Loop", ImVec2(buttonSize, buttonSize)))
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

    ImGui::PopStyleVar();

    // Speed control (next line)
    ImGui::SetCursorPosX(5.0f);
    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    float playRate = AnimSingleNodeInstance->GetPlayRate();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.85f, 0.9f, 1.0f));

    ImGui::SetCursorPosX(5.0f);
    ImGui::PushItemWidth(70.0f);

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

    ImGui::EndChild(); // PlaybackControls
    ImGui::EndChild(); // LeftControlArea
    ImGui::PopStyleVar();

    // === RIGHT SIDE: Timeline + Notify Display ===
    ImGui::SameLine(0, 0);
    ImGui::BeginChild("TimelineArea", ImVec2(RightTimelineWidth, WindowHeight), false, ImGuiWindowFlags_NoScrollbar);

    // Timeline with frame markers
    ImVec2 TimeLineStartPos = ImGui::GetCursorScreenPos();
    float TimeLineWidth = RightTimelineWidth - 20.0f;
    float TimeLineHeight = WindowHeight - 60.0f; // Leave space for bottom info
    TimeLineStartPos.x += 10.0f;
    TimeLineStartPos.y += 10.0f;
    ImVec2 TimeLineEndPos = ImVec2(TimeLineStartPos.x + TimeLineWidth, TimeLineStartPos.y + TimeLineHeight);

    // Background
    DrawList->AddRectFilled(TimeLineStartPos, TimeLineEndPos, IM_COL32(25, 25, 28, 255));

    // Timeline border
    DrawList->AddRect(TimeLineStartPos, TimeLineEndPos, IM_COL32(60, 60, 65, 255), 0.0f, 0, 1.0f);

    // Draw frame markers with labels
    int FrameStep = 5;
    if (AnimSequence->NumberOfFrames > 100) FrameStep = 10;
    if (AnimSequence->NumberOfFrames > 200) FrameStep = 20;

    for (int Frame = 0; Frame <= AnimSequence->NumberOfFrames; Frame += FrameStep)
    {
        float Xpos = TimeLineStartPos.x + (Frame / (float)AnimSequence->NumberOfFrames) * TimeLineWidth;

        // Vertical line
        DrawList->AddLine(
            ImVec2(Xpos, TimeLineStartPos.y),
            ImVec2(Xpos, TimeLineEndPos.y),
            IM_COL32(50, 50, 55, 255), 1.0f
        );

        // Frame number at top
        char frameLabel[16];
        sprintf_s(frameLabel, "%d", Frame);
        ImVec2 labelSize = ImGui::CalcTextSize(frameLabel);
        DrawList->AddText(ImVec2(Xpos - labelSize.x * 0.5f, TimeLineStartPos.y + 2.0f), IM_COL32(150, 150, 155, 255), frameLabel);
    }

    // Draw playhead (green vertical line)
    float HeadPosX = TimeLineStartPos.x + (CurrentInternalTime / AnimationLength) * TimeLineWidth;
    DrawList->AddLine(
        ImVec2(HeadPosX, TimeLineStartPos.y),
        ImVec2(HeadPosX, TimeLineEndPos.y),
        IM_COL32(80, 200, 120, 255), 2.0f);

    // Playhead draggable area
    ImGui::SetCursorScreenPos(TimeLineStartPos);
    ImGui::InvisibleButton("##TimeLineHeadButton", ImVec2(TimeLineWidth, TimeLineHeight));
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        float MouseX = ImGui::GetMousePos().x;
        float Ratio = (MouseX - TimeLineStartPos.x) / TimeLineWidth;
        Ratio = std::clamp(Ratio, 0.0f, 1.0f);

        CurrentInternalTime = Ratio * AnimationLength;
        AnimSingleNodeInstance->Pause();
        AnimSingleNodeInstance->SetInteralTime(CurrentInternalTime);
    }

    // Bottom info bar
    ImGui::SetCursorScreenPos(ImVec2(TimeLineStartPos.x, TimeLineEndPos.y + 10.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.85f, 0.9f, 1.0f));

    char frameInfo[64];
    sprintf_s(frameInfo, "%d        0        %d", CurrentFrame, AnimSequence->NumberOfFrames);
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