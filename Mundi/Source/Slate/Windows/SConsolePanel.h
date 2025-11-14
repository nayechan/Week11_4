#pragma once
#include "SWindow.h"

class UConsoleWindow;

/**
 * @brief Console panel that wraps UConsoleWindow in the SSplitter layout system
 */
class SConsolePanel : public SWindow
{
public:
	SConsolePanel();
	~SConsolePanel() override;

	void Initialize(UConsoleWindow* InConsoleWindow);

	void OnRender() override;
	void OnUpdate(float DeltaSeconds) override;

private:
	UConsoleWindow* ConsoleWindow = nullptr;
};
