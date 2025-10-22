#include "pch.h"
#include "Shader.h"

IMPLEMENT_CLASS(UShader)

// 컴파일 로직을 처리하는 비공개 헬퍼 함수
static bool CompileShaderInternal(
	const FWideString& InFilePath,
	const char* InEntryPoint,
	const char* InTarget,
	UINT InCompileFlags,
	const D3D_SHADER_MACRO* InDefines,
	ID3DBlob** OutBlob
)
{
	ID3DBlob* ErrorBlob = nullptr;
	HRESULT Hr = D3DCompileFromFile(
		InFilePath.c_str(),
		InDefines,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		InEntryPoint,
		InTarget,
		InCompileFlags,
		0,
		OutBlob,
		&ErrorBlob
	);

	if (FAILED(Hr))
	{
		if (ErrorBlob)
		{
			char* Msg = (char*)ErrorBlob->GetBufferPointer();

			// wstring을 UTF-8로 안전하게 변환 (한글 경로 지원)
			std::string NarrowPath = WideToUTF8(InFilePath);

			UE_LOG("Shader '%s' compile error: %s", NarrowPath.c_str(), Msg);
			ErrorBlob->Release();
		}
		if (*OutBlob) { (*OutBlob)->Release(); }
		return false;
	}

	if (ErrorBlob) { ErrorBlob->Release(); }
	return true;
}

UShader::~UShader()
{
	ReleaseResources();
}

// 두 개의 셰이더 파일을 받는 주요 Load 함수
void UShader::Load(const FString& InShaderPath, ID3D11Device* InDevice, const TArray<FShaderMacro>& InMacros)
{
	assert(InDevice);

	// Store the actual file path (without macro suffix)
	ActualFilePath = InShaderPath;

	// Store macros for hot reload
	Macros = InMacros;

	FWideString WFilePath(InShaderPath.begin(), InShaderPath.end());

	// Update timestamp using the actual file path
	try
	{
		auto FileTime = std::filesystem::last_write_time(ActualFilePath);
		SetLastModifiedTime(FileTime);
	}
	catch (...)
	{
		// If file doesn't exist yet, use current time
		SetLastModifiedTime(std::filesystem::file_time_type::clock::now());
	}

	// --- [핵심] TArray<FShaderMacro>를 D3D_SHADER_MACRO* 형태로 변환 ---
	TArray<D3D_SHADER_MACRO> Defines;
	TArray<TPair<FString, FString>> MacroStrings; // 포인터 유효성 유지를 위한 저장소
	MacroStrings.reserve(InMacros.Num());
	for (const FShaderMacro& Macro : InMacros)
	{
		MacroStrings.emplace_back(
			FString(Macro.Name.begin(), Macro.Name.end()),
			FString(Macro.Definition.begin(), Macro.Definition.end())
		);
		Defines.push_back({ MacroStrings.back().first.c_str(), MacroStrings.back().second.c_str() });
	}
	Defines.push_back({ NULL, NULL }); // 배열의 끝을 알리는 NULL 터미네이터
	// --- ----------------------------------------------------------------- ---

	UINT CompileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
	CompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	auto EndsWith = [](const FString& str, const FString& suffix)
		{
			if (str.size() < suffix.size()) return false;
			return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin(),
				[](char a, char b) { return ::tolower(a) == ::tolower(b); });
		};

	HRESULT Hr;

	if (EndsWith(InShaderPath, "_VS.hlsl"))
	{
		if (CompileShaderInternal(WFilePath, "mainVS", "vs_5_0", CompileFlags, Defines.data(), &VSBlob))
		{
			Hr = InDevice->CreateVertexShader(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), nullptr, &VertexShader);
			assert(SUCCEEDED(Hr));
			CreateInputLayout(InDevice, InShaderPath);
		}
	}
	else if (EndsWith(InShaderPath, "_PS.hlsl"))
	{
		if (CompileShaderInternal(WFilePath, "mainPS", "ps_5_0", CompileFlags, Defines.data(), &PSBlob))
		{
			Hr = InDevice->CreatePixelShader(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize(), nullptr, &PixelShader);
			assert(SUCCEEDED(Hr));
		}
	}
	else // Uber Shader (VS + PS)
	{
		bool bVsCompiled = CompileShaderInternal(WFilePath, "mainVS", "vs_5_0", CompileFlags, Defines.data(), &VSBlob);
		bool bPsCompiled = CompileShaderInternal(WFilePath, "mainPS", "ps_5_0", CompileFlags, Defines.data(), &PSBlob);

		if (bVsCompiled)
		{
			Hr = InDevice->CreateVertexShader(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), nullptr, &VertexShader);
			assert(SUCCEEDED(Hr));
			CreateInputLayout(InDevice, InShaderPath);
		}
		if (bPsCompiled)
		{
			Hr = InDevice->CreatePixelShader(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize(), nullptr, &PixelShader);
			assert(SUCCEEDED(Hr));
		}
	}

	// Include 파일 파싱 및 timestamp 추적
	ParseIncludeFiles(ActualFilePath);
}

void UShader::CreateInputLayout(ID3D11Device* Device, const FString& InShaderPath)
{
	TArray<D3D11_INPUT_ELEMENT_DESC> descArray = UResourceManager::GetInstance().GetProperInputLayout(InShaderPath);
	const D3D11_INPUT_ELEMENT_DESC* layout = descArray.data();
	uint32 layoutCount = static_cast<uint32>(descArray.size());

	// InputLayout을 사용하지 않는 VS를 위한 처리
	if (0 < layoutCount)
	{
		HRESULT hr = Device->CreateInputLayout(
			layout,
			layoutCount,
			VSBlob->GetBufferPointer(),
			VSBlob->GetBufferSize(),
			&InputLayout);
		assert(SUCCEEDED(hr));
	}
}

void UShader::ReleaseResources()
{
	if (VSBlob)
	{
		VSBlob->Release();
		VSBlob = nullptr;
	}
	if (PSBlob)
	{
		PSBlob->Release();
		PSBlob = nullptr;
	}
	if (InputLayout)
	{
		InputLayout->Release();
		InputLayout = nullptr;
	}
	if (VertexShader)
	{
		VertexShader->Release();
		VertexShader = nullptr;
	}
	if (PixelShader)
	{
		PixelShader->Release();
		PixelShader = nullptr;
	}
}

bool UShader::IsOutdated() const
{
	// Use the stored actual file path (not the unique key with macros)
	if (ActualFilePath.empty())
	{
		return false; // No file path stored
	}

	try
	{
		if (!std::filesystem::exists(ActualFilePath))
		{
			return false; // File doesn't exist, not outdated
		}

		// 메인 shader 파일 timestamp 체크
		auto CurrentFileTime = std::filesystem::last_write_time(ActualFilePath);
		if (CurrentFileTime > GetLastModifiedTime())
		{
			return true;
		}

		// Include 파일들의 timestamp 체크
		for (const auto& Pair : IncludedFileTimestamps)
		{
			const FString& IncludedFile = Pair.first;
			const auto& StoredTime = Pair.second;

			// Include 파일이 존재하는지 확인
			if (!std::filesystem::exists(IncludedFile))
			{
				continue; // 파일이 없으면 건너뛰기
			}

			// Include 파일의 현재 timestamp 확인
			auto CurrentIncludeTime = std::filesystem::last_write_time(IncludedFile);
			if (CurrentIncludeTime > StoredTime)
			{
				return true; // Include 파일이 변경됨
			}
		}

		return false; // 모든 파일이 최신 상태
	}
	catch (...)
	{
		return false; // Error checking file, assume not outdated
	}
}

bool UShader::Reload(ID3D11Device* InDevice)
{
	if (!InDevice)
	{
		return false;
	}

	// Use the stored actual file path
	if (ActualFilePath.empty())
	{
		UE_LOG("Hot Reload Failed: No actual file path stored for shader");
		return false;
	}

	UE_LOG("Hot Reloading Shader: %s (with %d macros)", ActualFilePath.c_str(), (int)Macros.size());

	// Store old resources in case reload fails
	ID3DBlob* OldVSBlob = VSBlob;
	ID3DBlob* OldPSBlob = PSBlob;
	ID3D11InputLayout* OldInputLayout = InputLayout;
	ID3D11VertexShader* OldVertexShader = VertexShader;
	ID3D11PixelShader* OldPixelShader = PixelShader;

	// Clear pointers so Load can create new ones
	VSBlob = nullptr;
	PSBlob = nullptr;
	InputLayout = nullptr;
	VertexShader = nullptr;
	PixelShader = nullptr;

	// Get device context to unbind resources
	ID3D11DeviceContext* Context = nullptr;
	InDevice->GetImmediateContext(&Context);
	
	if (Context)
	{
		// Unbind all shader resources from the pipeline to prevent device removal
		// This ensures the GPU is not using these resources when we release them
		
		// Unbind vertex shader
		if (OldVertexShader)
		{
			Context->VSSetShader(nullptr, nullptr, 0);
		}
		
		// Unbind pixel shader
		if (OldPixelShader)
		{
			Context->PSSetShader(nullptr, nullptr, 0);
		}
		
		// Unbind input layout
		if (OldInputLayout)
		{
			Context->IASetInputLayout(nullptr);
		}
		
		// Force GPU to finish all pending work before releasing resources
		Context->Flush();
		
		// Release the context reference (GetImmediateContext adds a ref)
		Context->Release();
	}

	// Try to reload with same macros using the actual file path
	Load(ActualFilePath, InDevice, Macros);

	// Check if reload was successful
	bool bReloadSuccessful = (VertexShader != nullptr || PixelShader != nullptr);

	if (bReloadSuccessful)
	{
		// Release old resources since new ones loaded successfully
		if (OldVSBlob) { OldVSBlob->Release(); }
		if (OldPSBlob) { OldPSBlob->Release(); }
		if (OldInputLayout) { OldInputLayout->Release(); }
		if (OldVertexShader) { OldVertexShader->Release(); }
		if (OldPixelShader) { OldPixelShader->Release(); }
	}
	else
	{
		// Reload failed, restore old resources
		VSBlob = OldVSBlob;
		PSBlob = OldPSBlob;
		InputLayout = OldInputLayout;
		VertexShader = OldVertexShader;
		PixelShader = OldPixelShader;
		
		UE_LOG("Hot Reload Failed: Restoring old shader for %s", ActualFilePath.c_str());
	}

	return bReloadSuccessful;
}

// Include 파일 파싱 (재귀적으로 처리)
void UShader::ParseIncludeFiles(const FString& ShaderPath)
{
	// 이미 파싱된 파일 목록 초기화
	IncludedFiles.clear();

	// 파싱할 파일 큐
	TArray<FString> FilesToParse;
	FilesToParse.push_back(ShaderPath);

	// 이미 파싱한 파일 추적 (중복 방지 및 순환 참조 방지)
	TSet<FString> ParsedFiles;

	while (!FilesToParse.empty())
	{
		FString CurrentFile = FilesToParse.back();
		FilesToParse.pop_back();

		// 이미 파싱한 파일은 건너뛰기
		if (ParsedFiles.find(CurrentFile) != ParsedFiles.end())
		{
			continue;
		}
		ParsedFiles.insert(CurrentFile);

		// 파일 존재 여부 확인
		if (!std::filesystem::exists(CurrentFile))
		{
			continue;
		}

		// 파일 열기
		std::ifstream File(CurrentFile);
		if (!File.is_open())
		{
			continue;
		}

		// 현재 파일의 디렉토리 경로
		std::filesystem::path CurrentDir = std::filesystem::path(CurrentFile).parent_path();

		// 한 줄씩 읽으며 #include 찾기
		FString Line;
		while (std::getline(File, Line))
		{
			// 공백 제거
			size_t FirstNonSpace = Line.find_first_not_of(" \t\r\n");
			if (FirstNonSpace == FString::npos)
			{
				continue;
			}
			Line = Line.substr(FirstNonSpace);

			// #include 지시문 찾기
			if (Line.compare(0, 8, "#include") == 0)
			{
				// #include "filename" 패턴 파싱
				size_t QuoteStart = Line.find('"');
				size_t QuoteEnd = Line.find('"', QuoteStart + 1);

				if (QuoteStart != FString::npos && QuoteEnd != FString::npos)
				{
					FString IncludedFile = Line.substr(QuoteStart + 1, QuoteEnd - QuoteStart - 1);

					// 상대 경로 해석
					std::filesystem::path IncludePath;
					if (std::filesystem::path(IncludedFile).is_absolute())
					{
						IncludePath = IncludedFile;
					}
					else
					{
						IncludePath = CurrentDir / IncludedFile;
					}

					// 경로 정규화
					try
					{
						IncludePath = std::filesystem::canonical(IncludePath);
						FString NormalizedPath = IncludePath.string();

						// 포함된 파일 목록에 추가
						if (std::find(IncludedFiles.begin(), IncludedFiles.end(), NormalizedPath) == IncludedFiles.end())
						{
							IncludedFiles.push_back(NormalizedPath);
							// 재귀적으로 파싱하기 위해 큐에 추가
							FilesToParse.push_back(NormalizedPath);
						}
					}
					catch (...)
					{
						// 파일이 존재하지 않거나 경로 오류 - 무시
					}
				}
			}
		}

		File.close();
	}

	// Include 파일들의 timestamp 업데이트
	UpdateIncludeTimestamps();
}

// Include 파일들의 timestamp 업데이트
void UShader::UpdateIncludeTimestamps()
{
	IncludedFileTimestamps.clear();

	for (const FString& IncludedFile : IncludedFiles)
	{
		try
		{
			if (std::filesystem::exists(IncludedFile))
			{
				auto FileTime = std::filesystem::last_write_time(IncludedFile);
				IncludedFileTimestamps[IncludedFile] = FileTime;
			}
		}
		catch (...)
		{
			// 파일 타임스탬프 읽기 실패 - 무시
		}
	}
}
