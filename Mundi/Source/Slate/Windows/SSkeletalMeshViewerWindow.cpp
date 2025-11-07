#include "pch.h"
#include "SSkeletalMeshViewerWindow.h"
#include "ImGui/imgui.h"
#include "FViewport.h"
#include "FViewportClient.h"

SSkeletalMeshViewerWindow::SSkeletalMeshViewerWindow()
{
    CenterRect = FRect(0, 0, 0, 0);
}

SSkeletalMeshViewerWindow::~SSkeletalMeshViewerWindow()
{
    if (Viewport)
    {
        delete Viewport;
        Viewport = nullptr;
    }
    if (ViewportClient)
    {
        delete ViewportClient;
        ViewportClient = nullptr;
    }
}

bool SSkeletalMeshViewerWindow::Initialize(float StartX, float StartY, float Width, float Height, UWorld* InWorld, ID3D11Device* InDevice)
{
    World = InWorld;
    Device = InDevice;

    SetRect(StartX, StartY, StartX + Width, StartY + Height);

    // Create center viewport now; actual size is updated in OnUpdate via UpdatePanelLayout
    Viewport = new FViewport();
    if (!Viewport->Initialize(StartX, StartY, Width, Height, Device))
    {
        delete Viewport;
        Viewport = nullptr;
        return false;
    }

    ViewportClient = new FViewportClient();
    ViewportClient->SetWorld(World);
    Viewport->SetViewportClient(ViewportClient);

    bRequestFocus = true;
    return true;
}

void SSkeletalMeshViewerWindow::OnRender()
{
    // Parent detachable window (movable, top-level, no background so 3D shows through)
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;

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
    if (ImGui::Begin("Skeletal Mesh Viewer", nullptr, flags))
    {
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        Rect.Left = pos.x; Rect.Top = pos.y; Rect.Right = pos.x + size.x; Rect.Bottom = pos.y + size.y; Rect.UpdateMinMax();

        ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        float totalWidth = contentAvail.x;
        float totalHeight = contentAvail.y;

        float leftWidth = totalWidth * LeftPanelRatio;
        float rightWidth = totalWidth * RightPanelRatio;
        float centerWidth = totalWidth - leftWidth - rightWidth;

        // Left panel (opaque background)
        ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, totalHeight), true);
        ImGui::Text("Skeleton Tree (placeholder)");
        ImGui::Separator();
        ImGui::TextDisabled("Phase 1: UI scaffolding");
        ImGui::EndChild();

        ImGui::SameLine();

        // Center panel (viewport area) — draw with no background so viewport remains visible
        ImGui::BeginChild("SkeletalMeshViewport", ImVec2(centerWidth, totalHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
        ImVec2 rectMin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(centerWidth, totalHeight));
        ImVec2 rectMax(rectMin.x + centerWidth, rectMin.y + totalHeight);
        CenterRect.Left = rectMin.x; CenterRect.Top = rectMin.y; CenterRect.Right = rectMax.x; CenterRect.Bottom = rectMax.y; CenterRect.UpdateMinMax();
        ImGui::EndChild();

        ImGui::SameLine();

        // Right panel (opaque background)
        ImGui::BeginChild("RightPanel", ImVec2(rightWidth, totalHeight), true);
        ImGui::Text("Inspector (placeholder)");
        ImGui::Separator();
        ImGui::TextDisabled("Phase 1: UI scaffolding");
        ImGui::EndChild();
    }
    ImGui::End();

    bRequestFocus = false;

    if (Viewport)
    {
        // Ensure viewport matches current center rect before rendering
        const uint32 NewStartX = static_cast<uint32>(CenterRect.Left);
        const uint32 NewStartY = static_cast<uint32>(CenterRect.Top);
        const uint32 NewWidth  = static_cast<uint32>(CenterRect.Right - CenterRect.Left);
        const uint32 NewHeight = static_cast<uint32>(CenterRect.Bottom - CenterRect.Top);
        Viewport->Resize(NewStartX, NewStartY, NewWidth, NewHeight);

        Viewport->Render();
    }
}

void SSkeletalMeshViewerWindow::OnUpdate(float DeltaSeconds)
{
    if (!Viewport)
        return;

    // Resize the internal viewport to match the center region
    const uint32 NewStartX = static_cast<uint32>(CenterRect.Left);
    const uint32 NewStartY = static_cast<uint32>(CenterRect.Top);
    const uint32 NewWidth  = static_cast<uint32>(CenterRect.Right - CenterRect.Left);
    const uint32 NewHeight = static_cast<uint32>(CenterRect.Bottom - CenterRect.Top);
    Viewport->Resize(NewStartX, NewStartY, NewWidth, NewHeight);

    if (ViewportClient)
    {
        ViewportClient->Tick(DeltaSeconds);
    }
}

void SSkeletalMeshViewerWindow::OnMouseMove(FVector2D MousePos)
{
    if (!Viewport) return;

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        Viewport->ProcessMouseMove((int32)LocalPos.X, (int32)LocalPos.Y);
    }
}

void SSkeletalMeshViewerWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
    if (!Viewport) return;

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
    }
}

void SSkeletalMeshViewerWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
    if (!Viewport) return;

    if (CenterRect.Contains(MousePos))
    {
        FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);
        Viewport->ProcessMouseButtonUp((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);
    }
}