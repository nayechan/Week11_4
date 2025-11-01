#include "pch.h"
#include "PlatformProcess.h"
#include <windows.h>
#include <shellapi.h>

using std::filesystem::path;
namespace fs = std::filesystem;

// FWideString 즉 UTF-16 형식으로 된 문자열만 열 수 있음
void FPlatformProcess::OpenFileInDefaultEditor(const FWideString& RelativePath)
{
    // 1. 상대 경로를 기반으로 절대 경로를 생성
    //    (현재 작업 디렉토리 기준)
    fs::path AbsolutePath;
    try
    {
        AbsolutePath = fs::absolute(RelativePath);
    }
    catch (const fs::filesystem_error& e)
    {
        // 경로 변환 실패 처리 (예: GConsole->LogError(...))
        MessageBoxA(NULL, "경로 변환에 실패했습니다.", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    // 2. 변환된 절대 경로를 사용
    HINSTANCE hInst = ShellExecuteA(
        NULL,
        "open",
        AbsolutePath.string().c_str(), // .string()으로 std::string을 얻어 c_str() 호출
        NULL,
        NULL,
        SW_SHOWNORMAL
    );

    if ((INT_PTR)hInst <= 32)
    {
        MessageBoxA(NULL, "파일 열기에 실패했습니다.", "Error", MB_OK | MB_ICONERROR);
    }
}

std::filesystem::path FPlatformProcess::OpenSaveFileDialog(const FWideString BaseDir, const FWideString Extension, const FWideString Description, const FWideString DefaultFileName)
{
    OPENFILENAMEW ofn;
    wchar_t szFile[260] = {}; // 선택된 전체 파일 경로를 수신할 버퍼
    wchar_t szInitialDir[260] = {};

    // GetSaveFileNameW 호출 전에 szFile 버퍼에 기본 파일명 복사
    if (!DefaultFileName.empty())
    {
        // 예: "Untitled.scene"
        wcscpy_s(szFile, _countof(szFile), DefaultFileName.c_str());
    }

    // 1. 기본 경로 설정
    fs::path baseDir = fs::absolute(BaseDir);
    // (디렉토리가 없으면 생성)
    if (!fs::exists(baseDir) || !fs::is_directory(baseDir))
    {
        fs::create_directories(baseDir);
    }
    wcscpy_s(szInitialDir, baseDir.wstring().c_str());

    // --- [수정 1] 확장자 정리 (e.g., ".scene" -> "scene") ---
    // (FString에 .StartsWith, .RightChop이 있다고 가정)
    FWideString CleanExtension = Extension;
    if (CleanExtension.starts_with(L"."))
    {
        CleanExtension = CleanExtension.substr(1);
    }

    // --- [수정 2] 필터 문자열 동적 생성 ---
    // "Scene Files\0*.scene\0All Files\0*.*\0\0" 형식
    FWideString FilterString;
    FilterString += Description.c_str();           // e.g., "Scene Files"
    FilterString.push_back(L'\0');                 // \0
    FilterString += L"*." + CleanExtension; // e.g., "*.scene"
    FilterString.push_back(L'\0');                 // \0
    FilterString += L"All Files";
    FilterString.push_back(L'\0');                 // \0
    FilterString += L"*.*";
    FilterString.push_back(L'\0');                 // \0
    FilterString.push_back(L'\0');                 // \0 (Double null-termination)

    // --- [수정 3] 다이얼로그 타이틀 동적 생성 ---
    FWideString TitleString = L"Save " + Description;

    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);

    // --- [수정 4] 동적 값 할당 ---
    ofn.lpstrFilter = FilterString.c_str(); // std::wstring의 c_str() 사용
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = szInitialDir;
    ofn.lpstrTitle = TitleString.c_str();     // FString::c_str() 사용
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ENABLESIZING;
    ofn.lpstrDefExt = CleanExtension.c_str(); // FString::c_str() 사용

    UE_LOG("MainToolbar: Opening Save Dialog (Modal)...");

    if (GetSaveFileNameW(&ofn) == TRUE)
    {
        UE_LOG("MainToolbar: Save Dialog Closed");

        // --- [수정 5] 파일 경로가 아닌 '부모 디렉토리'가 없으면 생성 ---
        fs::path FilePath = szFile;
        fs::path ParentDir = FilePath.parent_path();

        if (!fs::exists(ParentDir))
        {
            fs::create_directories(ParentDir);
            UE_LOG("MainToolbar: Created parent directory");
        }

        return FilePath;
    }

    UE_LOG("MainToolbar: Save Dialog Cancelled");
    return L"";
}