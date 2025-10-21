#include "pch.h"
#include "Texture.h"
#include "TextureConverter.h"
#include "DirectXTK/DDSTextureLoader.h"
#include "DirectXTK/WICTextureLoader.h"
#include <filesystem>

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

FString UTexture::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
	assert(InDevice);

	// 실제로 로드할 파일 경로 결정
	FString ActualLoadPath = InFilePath;

#ifdef USE_DDS_CACHE
	// DDS 캐싱 활성화 시: DDS 변환 및 캐시 사용
	{
		// 확장자 판별
		std::filesystem::path SourcePath(InFilePath);
		std::string Extension = SourcePath.extension().string();
		std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::tolower);

		// DDS가 아닌 경우 → DDS 캐시 확인 및 생성
		if (Extension != ".dds")
		{
			FString DDSCachePath = FTextureConverter::GetDDSCachePath(InFilePath);

			// 캐시 유효성 검사
			if (FTextureConverter::ShouldRegenerateDDS(InFilePath, DDSCachePath))
			{
				UE_LOG("[UTexture] Converting texture to DDS: %s", InFilePath.c_str());

				// DDS 변환 시도
				if (FTextureConverter::ConvertToDDS(InFilePath, DDSCachePath))
				{
					ActualLoadPath = DDSCachePath; // DDS 캐시 사용
				}
				else
				{
					UE_LOG("[UTexture] DDS conversion failed, loading original format: %s",
					       InFilePath.c_str());
					// 변환 실패 시 원본 포맷으로 로드 (fallback)
				}
			}
			else
			{
				// 기존 DDS 캐시 사용
				ActualLoadPath = DDSCachePath;
				UE_LOG("[UTexture] Using cached DDS: %s", DDSCachePath.c_str());
			}
		}
	}
#else
	// DDS 캐싱 비활성화 시: 원본 파일만 로드
	UE_LOG("[UTexture] Loading original texture (DDS cache disabled): %s", InFilePath.c_str());
#endif

	// UTF-8 -> UTF-16 (Windows) 안전 변환: 한글/비ASCII 경로 대응
	int needed = ::MultiByteToWideChar(CP_UTF8, 0, ActualLoadPath.c_str(), -1, nullptr, 0);
	std::wstring WFilePath;
	if (needed > 0)
	{
		WFilePath.resize(needed - 1);
		::MultiByteToWideChar(CP_UTF8, 0, ActualLoadPath.c_str(), -1, WFilePath.data(), needed);
	}
	else
	{
		int needA = ::MultiByteToWideChar(CP_ACP, 0, ActualLoadPath.c_str(), -1, nullptr, 0);
		if (needA > 0)
		{
			WFilePath.resize(needA - 1);
			::MultiByteToWideChar(CP_ACP, 0, ActualLoadPath.c_str(), -1, WFilePath.data(), needA);
		}
	}

	// 최종 로드할 파일의 확장자 재확인
	std::filesystem::path LoadPath(ActualLoadPath);
	std::wstring ext = LoadPath.has_extension() ? LoadPath.extension().wstring() : L"";
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

	if (SUCCEEDED(hr))
	{
		if (Texture2D)
		{
			D3D11_TEXTURE2D_DESC desc;
			Texture2D->GetDesc(&desc);
			Width = desc.Width;
			Height = desc.Height;
			Format = desc.Format;
		}
	}
	else
	{
		UE_LOG("[UTexture] Failed to load texture: %s (HRESULT: 0x%08X)",
		       ActualLoadPath.c_str(), hr);
	}

	TextureName = ActualLoadPath; // 실제 로드된 경로 저장 (DDS 캐시 사용 시 DDS 경로)

	// 실제 로드된 경로 반환 (ResourceManager가 올바른 키로 등록하도록)
	return ActualLoadPath;
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
