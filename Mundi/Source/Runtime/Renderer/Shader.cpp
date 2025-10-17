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
			// FWideString을 FString으로 변환하여 로그 출력
			UE_LOG("Shader '%s' compile error: %s", FString(InFilePath.begin(), InFilePath.end()).c_str(), Msg);
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

	FWideString WFilePath(InShaderPath.begin(), InShaderPath.end());

	// --- [핵심] TArray<FShaderMacro>를 D3D_SHADER_MACRO* 형태로 변환 ---
	std::vector<D3D_SHADER_MACRO> Defines;
	std::vector<std::pair<std::string, std::string>> MacroStrings; // 포인터 유효성 유지를 위한 저장소
	MacroStrings.reserve(InMacros.Num());
	for (const FShaderMacro& Macro : InMacros)
	{
		MacroStrings.emplace_back(
			std::string(Macro.Name.begin(), Macro.Name.end()),
			std::string(Macro.Definition.begin(), Macro.Definition.end())
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
