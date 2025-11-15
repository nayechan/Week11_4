#include "pch.h"
#include "ExceptionHandler.h"

void CauseCrash()
{
	// volatile: 최적화 못 하게 함.
	volatile int* Trash = nullptr;

	*Trash = 342;
}
//UnhandledException이 발생했을 때(Catch구문이 잡지 못한 예외) 호출될 콜백함수
static LONG WINAPI CustumUnhandledExceptionFilter(_EXCEPTION_POINTERS* ExceptionInfo)
{
	SYSTEMTIME Time;
	GetLocalTime(&Time);

	char DumpFileName[64];
	sprintf_s(DumpFileName, 64, "MundiDumpFile_%04d%02d%02d_%02d%02d.dmp",
		Time.wYear, Time.wMonth, Time.wDay, Time.wHour, Time.wMinute);

	HANDLE DumpFile = CreateFileA(
		DumpFileName,
		// 쓰기 할 파일
		GENERIC_WRITE,
		// 쓰는 동안 다른 프로그램이 읽기는 가능(다른 건 못함)
		FILE_SHARE_READ,
		// 파일 보안 설정(안 함)
		NULL,
		// 없으면 만들고 있으면 덮어씌움
		CREATE_ALWAYS,
		// 파일에 Archive 속성 부여(백업대상)
		FILE_ATTRIBUTE_ARCHIVE,
		// 속성 복사해 올 템플릿 파일(없어서 NULL)
		NULL
	);

	if (DumpFile == INVALID_HANDLE_VALUE)
	{
		// OS한테 넘겨버림
		return EXCEPTION_CONTINUE_SEARCH;
		// EXCEPTION_CONTINUE_EXECUTION: 예외코드 재실행(프로그램 종료 안 함)
		// EXCEPTION_EXECUTE_HANDLER: OS에 안 넘기고 종료(이미 내가 덤프파일 만들었으니까)
	}

	MINIDUMP_EXCEPTION_INFORMATION MiniDumpInfo;
	MiniDumpInfo.ThreadId = GetCurrentThreadId();
	MiniDumpInfo.ExceptionPointers = ExceptionInfo;
	// FALSE: 예외 정보가 Client(지금 이 프로세스)에 있음. TRUE: 예외 정보가 덤프 대상(다른 타겟 프로세스)에 있음(다른 프로그램의 예외를 덤프 뜨는 경우)
	MiniDumpInfo.ClientPointers = FALSE;

	// MiniDumpNormal: 콜 스택, 스레드 목록 등 디버깅에 필수적인 최소 정보
	// MiniDumpWithDataSegs: 데이터 세그먼트(전역 변수) 포함.
	// MiniDumpWithFullMemory: 모든 메모리 덤프(고소당함)
	bool Result = MiniDumpWriteDump(
		GetCurrentProcess(),
		GetCurrentProcessId(),
		DumpFile,
		MiniDumpWithDataSegs,
		&MiniDumpInfo,
		NULL,
		NULL
	);

	CloseHandle(DumpFile);

	return EXCEPTION_EXECUTE_HANDLER;
}

// 이전에 OS나 vs가 자동으로 등록한 콜백함수 포인터.
// 이제 새로 등록할 것이라서 기존에 쓰던 콜백함수를 저장해뒀다가 소멸할때 되돌려 놓을 것임
// 그냥 멤버로 저장하면 객체가 생성될때마다 새로운 FIlter가 설정되고 소멸 순서에 따라 원래 OS, vs에서 쓰던 콜백함수를 복구하지 못 할 수도 있음.
// 그렇다고 전역으로 카운트를 저장하면 안 되는 이유가 UnhandledExceptionFilter는 스레드 단위로 설정됨.
// thread_local 키워드가 붙은 변수는 생명주기가 스레드와 같고 스레드마다 독립적으로 존재함(생명주기가 함수와 같은 스택과 다름)
// 그래서 Thread마다 하나씩만 Filter를 설정하도록 하는 것임.
static thread_local LPTOP_LEVEL_EXCEPTION_FILTER PrevUnhandledExceptionFilter = nullptr;
static thread_local int32 ThreadFilterCounter = 0;

FExceptionHandler::FExceptionHandler()
{
	// 하나만 설정
	if (ThreadFilterCounter == 0)
	{
		// SetUnhandledExceptionFilter는 새로운 콜백함수 등록 이전에 등록된 콜백함수를 리턴함
		PrevUnhandledExceptionFilter = SetUnhandledExceptionFilter(CustumUnhandledExceptionFilter);
	}
	ThreadFilterCounter++;
}

FExceptionHandler::~FExceptionHandler()
{
	ThreadFilterCounter--;

	// 누구든지 마지막으로 소멸할때 스레드가 전역적으로 관리하는 PrevFilter를 설정해주면 됨.
	if (ThreadFilterCounter == 0)
	{
		SetUnhandledExceptionFilter(PrevUnhandledExceptionFilter);
	}

}