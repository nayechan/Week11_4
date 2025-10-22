#pragma once
#include "ResourceBase.h"
#include <filesystem>

struct FShaderMacro
{
	FString Name;
	FString Definition;
};

class UShader : public UResourceBase
{
public:
	DECLARE_CLASS(UShader, UResourceBase)

	void Load(const FString& ShaderPath, ID3D11Device* InDevice, const TArray<FShaderMacro>& InMacros = TArray<FShaderMacro>());

	ID3D11InputLayout* GetInputLayout() const { return InputLayout; }
	ID3D11VertexShader* GetVertexShader() const { return VertexShader; }
	ID3D11PixelShader* GetPixelShader() const { return PixelShader; }

	// Hot Reload Support
	bool IsOutdated() const;
	bool Reload(ID3D11Device* InDevice);
	const TArray<FShaderMacro>& GetMacros() const { return Macros; }
	
	// Get the actual shader file path (without macro suffix)
	const FString& GetActualFilePath() const { return ActualFilePath; }

protected:
	virtual ~UShader();

private:
	ID3DBlob* VSBlob = nullptr;
	ID3DBlob* PSBlob = nullptr;

	ID3D11InputLayout* InputLayout = nullptr;
	ID3D11VertexShader* VertexShader = nullptr;
	ID3D11PixelShader* PixelShader = nullptr;

	// Store macros for hot reload
	TArray<FShaderMacro> Macros;

	// Store actual file path (e.g., "Shaders/Materials/UberLit.hlsl")
	// FilePath (from base class) stores the unique key with macros
	FString ActualFilePath;

	// Store included files (e.g., "Shaders/Common/LightingCommon.hlsl")
	// Used for hot reload - if any included file changes, reload this shader
	TArray<FString> IncludedFiles;
	TMap<FString, std::filesystem::file_time_type> IncludedFileTimestamps;

	void CreateInputLayout(ID3D11Device* Device, const FString& InShaderPath);
	void ReleaseResources();

	// Include 파일 파싱 및 추적
	void ParseIncludeFiles(const FString& ShaderPath);
	void UpdateIncludeTimestamps();
};

struct FVertexPositionColor
{
	static const D3D11_INPUT_ELEMENT_DESC* GetLayout()
	{
		static const D3D11_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		return layout;
	}
	static uint32 GetLayoutCount() { return 2; }
};

struct FVertexPositionColorTexturNormal
{
	static const D3D11_INPUT_ELEMENT_DESC* GetLayout()
	{
		static const D3D11_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		return layout;
	}

	static uint32 GetLayoutCount() { return 4; }
};

struct FVertexPositionBillBoard

{

	static const D3D11_INPUT_ELEMENT_DESC* GetLayout()

	{

		static const D3D11_INPUT_ELEMENT_DESC layout[] = {

		{ "WORLDPOSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },

		{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},

		{ "UVRECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},

		};

		return layout;

	}



	static uint32 GetLayoutCount() { return 3; }

};

// ======================== 오클루전 관련 메소드들 ============================
/*
// 실패 시 false 반환, *OutVS/*OutPS는 성공 시 유효
bool CompileVS(ID3D11Device* Dev, const wchar_t* FilePath, const char* Entry,
	ID3D11VertexShader** OutVS, ID3DBlob** OutVSBytecode = nullptr);

bool CompilePS(ID3D11Device* Dev, const wchar_t* FilePath, const char* Entry,
	ID3D11PixelShader** OutPS);
*/