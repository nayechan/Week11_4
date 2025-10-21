/**
 * @file TextureConverter.cpp
 * @brief DirectXTex를 사용한 텍스처 변환 유틸리티 구현
 */

#include "pch.h"
#include "TextureConverter.h"
#include <DirectXTex.h>
#include <algorithm>

// 헬퍼 함수: UTF-8 문자열을 와이드 문자열로 변환
static std::wstring UTF8ToWide(const std::string& str)
{
	if (str.empty()) return std::wstring();

	int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(),
	                                       static_cast<int>(str.size()), NULL, 0);
	if (size_needed <= 0) return std::wstring();

	std::wstring result(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()),
	                    &result[0], size_needed);
	return result;
}

// 헬퍼 함수: 와이드 문자열을 UTF-8 문자열로 변환
static std::string WideToUTF8(const std::wstring& wstr)
{
	if (wstr.empty()) return std::string();

	int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(),
	                                       static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
	if (size_needed <= 0) return std::string();

	std::string result(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()),
	                    &result[0], size_needed, NULL, NULL);
	return result;
}

bool FTextureConverter::ConvertToDDS(
	const FString& SourcePath,
	const FString& OutputPath,
	DXGI_FORMAT Format)
{
	using namespace DirectX;

	// 1. 원본 이미지 로드
	std::wstring WSourcePath = UTF8ToWide(SourcePath);
	std::filesystem::path SourceFile(WSourcePath);

	if (!std::filesystem::exists(SourceFile))
	{
		UE_LOG("[TextureConverter] Source file not found: %s", SourcePath.c_str());
		return false;
	}

	TexMetadata metadata;
	ScratchImage image;

	// 파일 확장자에 따라 로드
	std::wstring ext = SourceFile.extension().wstring();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

	HRESULT hr = E_FAIL;

	if (ext == L".dds")
	{
		// 이미 DDS 포맷이면 변환 불필요
		return true;
	}
	else if (ext == L".tga")
	{
		hr = LoadFromTGAFile(WSourcePath.c_str(), &metadata, image);
	}
	else if (ext == L".hdr")
	{
		hr = LoadFromHDRFile(WSourcePath.c_str(), &metadata, image);
	}
	else
	{
		// 일반적인 포맷(PNG, JPG, BMP 등)은 WIC 사용
		hr = LoadFromWICFile(WSourcePath.c_str(), WIC_FLAGS_NONE, &metadata, image);
	}

	if (FAILED(hr))
	{
		UE_LOG("[TextureConverter] Failed to load source image: %s (HRESULT: 0x%08X)",
		       SourcePath.c_str(), hr);
		return false;
	}

	// 2. 블록 압축 사용 시 4픽셀 정렬로 리사이즈
	if (IsCompressed(Format))
	{
		size_t width = metadata.width;
		size_t height = metadata.height;

		// 4의 배수로 올림
		size_t alignedWidth = (width + 3) & ~3;
		size_t alignedHeight = (height + 3) & ~3;

		if (width != alignedWidth || height != alignedHeight)
		{
			UE_LOG("[TextureConverter] Resizing %s from %dx%d to %dx%d for block compression",
			       SourcePath.c_str(), (int)width, (int)height,
			       (int)alignedWidth, (int)alignedHeight);

			ScratchImage resized;
			hr = Resize(image.GetImages(), image.GetImageCount(), metadata,
			            alignedWidth, alignedHeight, TEX_FILTER_DEFAULT, resized);

			if (SUCCEEDED(hr))
			{
				image = std::move(resized);
				metadata = image.GetMetadata();
			}
			else
			{
				UE_LOG("[TextureConverter] Warning: Resize failed, continuing with original size");
			}
		}
	}

	// 3. 필요 시 밉맵 생성
	ScratchImage mipChain;
	if (bShouldGenerateMipmaps && metadata.mipLevels == 1)
	{
		hr = GenerateMipMaps(image.GetImages(), image.GetImageCount(), metadata,
		                     TEX_FILTER_DEFAULT, 0, mipChain);
		if (SUCCEEDED(hr))
		{
			image = std::move(mipChain);
			metadata = image.GetMetadata();
		}
	}

	// 4. 대상 포맷으로 압축
	ScratchImage compressed;
	if (IsCompressed(Format))
	{
		// 빠른 멀티스레드 압축
		hr = Compress(image.GetImages(), image.GetImageCount(), metadata,
		              Format, TEX_COMPRESS_PARALLEL | TEX_COMPRESS_DITHER,
		              TEX_THRESHOLD_DEFAULT, compressed);

		if (FAILED(hr))
		{
			UE_LOG("[TextureConverter] Compression failed: %s (HRESULT: 0x%08X)",
			       SourcePath.c_str(), hr);
			return false;
		}
	}
	else
	{
		// 비압축 포맷은 그냥 변환
		compressed = std::move(image);
	}

	// 5. 출력 경로 결정
	FString FinalOutputPath = OutputPath.empty() ? GetDDSCachePath(SourcePath) : OutputPath;
	EnsureCacheDirectoryExists(FinalOutputPath);

	// 6. DDS로 저장
	std::wstring WOutputPath = UTF8ToWide(FinalOutputPath);
	hr = SaveToDDSFile(compressed.GetImages(), compressed.GetImageCount(),
	                   compressed.GetMetadata(), DDS_FLAGS_NONE, WOutputPath.c_str());

	if (FAILED(hr))
	{
		UE_LOG("[TextureConverter] Failed to save DDS file: %s (HRESULT: 0x%08X)",
		       FinalOutputPath.c_str(), hr);
		return false;
	}

	UE_LOG("[TextureConverter] Successfully converted: %s -> %s",
	       SourcePath.c_str(), FinalOutputPath.c_str());
	return true;
}

bool FTextureConverter::ShouldRegenerateDDS(
	const FString& SourcePath,
	const FString& DDSPath)
{
	namespace fs = std::filesystem;

	// DDS 캐시 존재 여부 확인
	fs::path SourceFile(UTF8ToWide(SourcePath));
	fs::path DDSFile(UTF8ToWide(DDSPath));

	if (!fs::exists(DDSFile))
	{
		return true; // 캐시가 없으면 재생성
	}

	if (!fs::exists(SourceFile))
	{
		return false; // 원본이 없으면 기존 캐시 사용
	}

	// 타임스탬프 비교
	auto SourceTime = fs::last_write_time(SourceFile);
	auto DDSTime = fs::last_write_time(DDSFile);

	// 원본이 캐시보다 최신이면 재생성
	return SourceTime > DDSTime;
}

FString FTextureConverter::GetDDSCachePath(const FString& SourcePath)
{
	namespace fs = std::filesystem;

	// 원본 경로를 와이드 문자열로 변환 (한글 경로 지원)
	std::wstring WSourcePath = UTF8ToWide(SourcePath);
	fs::path SourceFile(WSourcePath);
	fs::path RelativePath;

	// Data 디렉토리 경로 (상대 경로 방식)
	fs::path DataDir("Data");

	if (SourceFile.is_absolute())
	{
		// 절대 경로인 경우: Data 디렉토리 기준 상대 경로로 변환 시도
		fs::path CurrentDir = fs::current_path();
		fs::path AbsDataDir = CurrentDir / DataDir;

		std::error_code ec;
		RelativePath = fs::relative(SourceFile, AbsDataDir, ec);

		if (ec || RelativePath.empty() || RelativePath.wstring().find(L"..") == 0)
		{
			// Data 디렉토리 외부 파일이면 파일명만 사용
			RelativePath = SourceFile.filename();
		}
	}
	else
	{
		// 이미 상대 경로인 경우
		RelativePath = SourceFile;

		// "Data/"로 시작하면 제거
		std::wstring RelStr = RelativePath.wstring();
		if (RelStr.find(L"Data/") == 0 || RelStr.find(L"Data\\") == 0)
		{
			RelativePath = fs::path(RelStr.substr(5)); // "Data/" 제거 (5글자)
		}
	}

	// 캐시 경로 생성: Data/TextureCache/<relative_path>/<filename>.dds (상대 경로)
	fs::path CachePath = DataDir / "TextureCache" / RelativePath;
	CachePath.replace_extension(".dds");

	// 와이드 문자열을 UTF-8로 변환하여 반환 (한글 경로 지원)
	return WideToUTF8(CachePath.wstring());
}

bool FTextureConverter::IsSupportedFormat(const FString& Extension)
{
	std::string ext = Extension;
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	static const std::unordered_set<std::string> SupportedFormats = {
		".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr", ".dds", ".tif", ".tiff"
	};

	return SupportedFormats.find(ext) != SupportedFormats.end();
}

void FTextureConverter::SetGenerateMipmaps(bool bGenerateMips)
{
	bShouldGenerateMipmaps = bGenerateMips;
}

DXGI_FORMAT FTextureConverter::GetRecommendedFormat(bool bHasAlpha)
{
	// BC3 (DXT5): 알파 포함 텍스처용 - 빠른 압축, 좋은 품질
	// BC1 (DXT1): 불투명 텍스처용 - 가장 빠른 압축, 작은 크기
	// BC7: 사용 가능하지만 매우 느림 (BC3보다 10~20배)

	return bHasAlpha ? DXGI_FORMAT_BC3_UNORM : DXGI_FORMAT_BC1_UNORM;
}

void FTextureConverter::EnsureCacheDirectoryExists(const FString& CachePath)
{
	namespace fs = std::filesystem;

	fs::path Path(UTF8ToWide(CachePath));
	fs::path Directory = Path.parent_path();

	if (!Directory.empty() && !fs::exists(Directory))
	{
		std::error_code ec;
		fs::create_directories(Directory, ec);

		if (ec)
		{
			UE_LOG("[TextureConverter] Failed to create cache directory: %s",
			       WideToUTF8(Directory.wstring()).c_str());
		}
	}
}
