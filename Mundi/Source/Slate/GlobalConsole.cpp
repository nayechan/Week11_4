#include "pch.h"
#include "Widgets/ConsoleWidget.h"

IMPLEMENT_CLASS(UGlobalConsole)

UConsoleWidget* UGlobalConsole::ConsoleWidget = nullptr;

void UGlobalConsole::Initialize()
{
    // Nothing special to initialize
}

void UGlobalConsole::Shutdown()
{
    ConsoleWidget = nullptr;
}

void UGlobalConsole::SetConsoleWidget(UConsoleWidget* InConsoleWidget)
{
    ConsoleWidget = InConsoleWidget;
    if (InConsoleWidget)
    {
        UE_LOG("GlobalConsole: ConsoleWidget set successfully\n");
    }
    else
    {
        UE_LOG("GlobalConsole: ConsoleWidget set to null\n");
    }
}

UConsoleWidget* UGlobalConsole::GetConsoleWidget()
{
    return ConsoleWidget;
}

void UGlobalConsole::Log(const char* fmt, ...)
{
#ifdef _EDITOR
    va_list args;
    va_start(args, fmt);
    LogV(fmt, args);
    va_end(args);
#endif
}

void UGlobalConsole::LogV(const char* fmt, va_list args)
{
#ifdef _EDITOR
    // Format message once
    char tmp[1024];
    vsnprintf_s(tmp, _countof(tmp), fmt, args);

    // Always output to Visual Studio debugger
    OutputDebugStringA(tmp);
    if (tmp[strlen(tmp) - 1] != '\n')
    {
        OutputDebugStringA("\n");
    }

    // Also output to ConsoleWidget if available
    if (ConsoleWidget)
    {
        ConsoleWidget->AddLog(tmp);
    }
#endif
}

// Global C functions for compatibility
extern "C" void ConsoleLog(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    UGlobalConsole::LogV(fmt, args);
    va_end(args);
}

extern "C" void ConsoleLogV(const char* fmt, va_list args)
{
    UGlobalConsole::LogV(fmt, args);
}
