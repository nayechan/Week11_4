#include "pch.h"
#include "Widgets/RenderViewportSwitcherWidget.h"
#include "USlateManager.h"
#include "World.h"
#include "ImGui/imgui.h"

IMPLEMENT_CLASS(URenderViewportSwitcherWidget)

URenderViewportSwitcherWidget::URenderViewportSwitcherWidget()
{
}

URenderViewportSwitcherWidget::~URenderViewportSwitcherWidget()
{
}

void URenderViewportSwitcherWidget::Initialize()
{
}

void URenderViewportSwitcherWidget::Update()
{
}

void URenderViewportSwitcherWidget::RenderWidget()
{



    ImGui::Text("Viewport Mode:");
    ImGui::Separator();

    // === Single View 버튼 ===
    if (bUseMainViewport)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.65f, 0.52f, 1.0f));      // 에메랄드 그린
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.42f, 0.75f, 0.62f, 1.0f)); // 밝은 에메랄드
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.28f, 0.55f, 0.42f, 1.0f));  // 진한 에메랄드
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.22f, 0.21f, 1.0f));      // 테마 기본색
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.35f, 0.32f, 1.0f)); // 호버
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.48f, 0.44f, 1.0f));  // 활성
    }

    if (ImGui::Button("Single View", ImVec2(120, 35)))
    {
        SLATE.SwitchLayout(EViewportLayoutMode::SingleMain);
        bUseMainViewport = true;
        UE_LOG("UIManager: Switched to Main Viewport mode");
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Switch to single perspective viewport\nShortcut: F1");

    ImGui::PopStyleColor(3);

    ImGui::SameLine();

    // === Multi View 버튼 ===
    if (!bUseMainViewport)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.65f, 0.52f, 1.0f));      // 에메랄드 그린
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.42f, 0.75f, 0.62f, 1.0f)); // 밝은 에메랄드
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.28f, 0.55f, 0.42f, 1.0f));  // 진한 에메랄드
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.22f, 0.21f, 1.0f));      // 테마 기본색
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.35f, 0.32f, 1.0f)); // 호버
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.48f, 0.44f, 1.0f));  // 활성
    }

    if (ImGui::Button("Multi View", ImVec2(120, 35)))
    {
        SLATE.SwitchLayout(EViewportLayoutMode::FourSplit);
        bUseMainViewport = false;
        UE_LOG("UIManager: Switched to Multi Viewport mode");
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Switch to 4-panel viewport (Perspective, Front, Left, Top)\nShortcut: F2");

    ImGui::PopStyleColor(3);

}