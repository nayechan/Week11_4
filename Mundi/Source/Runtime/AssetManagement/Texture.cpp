#include "pch.h"
#include "Texture.h"
#include "DirectXTK/DDSTextureLoader.h"
#include "DirectXTK/WICTextureLoader.h"

IMPLEMENT_CLASS(UTexture)

// wstring을 UTF-8 string으로 안전하게 변환하는 헬퍼 함수 (한글 지원)
static std::string WideStringToUTF8(const std::wstring& wstr)
{
	if (wstr.empty()) return std::string();
	
	// 1단계: 필요한 버퍼 크기 계산
	int size_needed = WideCharToMultiByte(
		CP_UTF8,                    // UTF-8 코드 페이지
		0,                          // 플래그
		wstr.c_str(),               // 입력 wstring
		static_cast<int>(wstr.size()), // 입력 문자 수
		NULL,                       // 출력 버퍼 (NULL이면 크기만 계산)
		0,                          // 출력 버퍼 크기
		NULL,                       // 기본 문자
		NULL                        // 기본 문자 사용 여부
	);
	
	if (size_needed <= 0)
	{
		return std::string(); // 변환 실패
	}
	
	// 2단계: 실제 변환 수행
	std::string result(size_needed, 0);
	WideCharToMultiByte(
		CP_UTF8,
		0,
		wstr.c_str(),
		static_cast<int>(wstr.size()),
		&result[0],                 // 출력 버퍼
		size_needed,
		NULL,
		NULL
	);
	
	return result;
}

UTexture::UTexture()
{
	Width = 0;
	Height = 0;
	Format = DXGI_FORMAT_UNKNOWN;
}

UTexture::~UTexture()
{
	ReleaseResources();
}

void UTexture::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
	assert(InDevice);

	// UTF-8 -> UTF-16 (Windows) 안전 변환: 한글/비ASCII 경로 대응
	int needed = ::MultiByteToWideChar(CP_UTF8, 0, InFilePath.c_str(), -1, nullptr, 0);
	std::wstring WFilePath;
	if (needed > 0)
	{
		WFilePath.resize(needed - 1);
		::MultiByteToWideChar(CP_UTF8, 0, InFilePath.c_str(), -1, WFilePath.data(), needed);
	}
	else
	{
		int needA = ::MultiByteToWideChar(CP_ACP, 0, InFilePath.c_str(), -1, nullptr, 0);
		if (needA > 0)
		{
			WFilePath.resize(needA - 1);
			::MultiByteToWideChar(CP_ACP, 0, InFilePath.c_str(), -1, WFilePath.data(), needA);
		}
	}

	// 확장자 판별 (안전)
	std::filesystem::path realPath(InFilePath);
	std::wstring ext = realPath.has_extension() ? realPath.extension().wstring() : L"";
	for (auto& ch : ext) ch = static_cast<wchar_t>(::towlower(ch));

	HRESULT hr = E_FAIL;
	if (ext == L".dds")
	{
		hr = DirectX::CreateDDSTextureFromFile(
			InDevice,
			WFilePath.c_str(),
			reinterpret_cast<ID3D11Resource**>(&Texture2D),
			&ShaderResourceView
		);
	}
	else
	{
		hr = DirectX::CreateWICTextureFromFile(
			InDevice, 
			WFilePath.c_str(), 
			reinterpret_cast<ID3D11Resource**>(&Texture2D), 
			&ShaderResourceView
		);
	}

	if (FAILED(hr))
	{
		// 실패 시에도 경로를 UTF-8로 안전하게 변환하여 출력
		std::string displayPath = WideStringToUTF8(WFilePath);
		UE_LOG("!!!LOAD TEXTURE FAILED!!! Path: %s", displayPath.c_str());
	}
	else
	{
		if (Texture2D)
		{
			D3D11_TEXTURE2D_DESC desc;
			Texture2D->GetDesc(&desc);
			Width = desc.Width;
			Height = desc.Height;
			Format = desc.Format;
		}

		// 성공 시에도 경로를 UTF-8로 안전하게 변환하여 출력
		std::string displayPath = WideStringToUTF8(WFilePath);
		UE_LOG("Successfully loaded texture: %s (Format: %d, Size: %dx%d)", 
			displayPath.c_str(), Format, Width, Height);
	}

	TextureName = InFilePath;
}

void UTexture::ReleaseResources()
{
	if (Texture2D)
	{
		Texture2D->Release();
		Texture2D = nullptr;
	}

	if (ShaderResourceView)
	{
		ShaderResourceView->Release();
		ShaderResourceView = nullptr;
	}

	Width = 0;
	Height = 0;
	Format = DXGI_FORMAT_UNKNOWN;
}
