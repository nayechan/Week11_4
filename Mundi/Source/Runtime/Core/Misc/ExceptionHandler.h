#pragma once
#include <Windows.h>
#include <DbgHelp.h>

#pragma comment(lib, "dbghelp.lib")

void CauseCrash();

class FExceptionHandler
{
public:
	FExceptionHandler();
	~FExceptionHandler();
private:

};

class FCrasher
{
public:
	static void CauseCrash();
	static void StartRandomCrash();
	static bool IsCrashMode();
private:
	static bool bIsCrashMode;
};