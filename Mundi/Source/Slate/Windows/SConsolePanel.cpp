#include "pch.h"
#include "SConsolePanel.h"
#include "ConsoleWindow.h"
#include "ImGui/imgui.h"

SConsolePanel::SConsolePanel()
{
}

SConsolePanel::~SConsolePanel()
{
}

void SConsolePanel::Initialize(UConsoleWindow* InConsoleWindow)
{
	ConsoleWindow = InConsoleWindow;
}

void SConsolePanel::OnRender()
{
	if (!ConsoleWindow)
		return;

	// Set up ImGui window to fit the console panel area
	ImGui::SetNextWindowPos(ImVec2(Rect.Min.X, Rect.Min.Y));
	ImGui::SetNextWindowSize(ImVec2(GetWidth(), GetHeight()));

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	if (ImGui::Begin("##ConsolePanel", nullptr, flags))
	{
		// 전체 영역을 BeginChild로 감싸서 일관된 배경색 유지
		if (ImGui::BeginChild("##ConsolePanelContent", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			// Add padding inside BeginChild
			ImGui::Indent(8.0f);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);

			// Render console title
			ImGui::TextColored(ImVec4(0.52f, 0.88f, 0.75f, 1.0f), "Console");
			ImGui::Separator();

			// Render console widget
			ConsoleWindow->RenderWidget();

			ImGui::Unindent(8.0f);
		}
		ImGui::EndChild();
	}
	ImGui::End();

	ImGui::PopStyleVar(3);
}

void SConsolePanel::OnUpdate(float DeltaTime)
{
	if (ConsoleWindow)
	{
		ConsoleWindow->Update(DeltaTime);
	}
}
