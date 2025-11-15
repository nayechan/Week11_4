#include "pch.h"
#pragma warning(push)
#pragma warning(disable: 4244) // Disable double to float conversion warning for FBX SDK
#include "FbxMaterial.h"
#include "FbxHelper.h"
#include "TextureConverter.h"
#include "Material.h"
#include "ResourceManager.h"
#include "PathUtils.h"
#include "GlobalConsole.h"
#include <filesystem>

// ========================================
// FBX 머티리얼 및 텍스처 추출
// ========================================

/**
 * ExtractMaterialProperties
 *
 * FBX 머티리얼에서 모든 속성과 텍스처 추출
 */
void FFbxMaterial::ExtractMaterialProperties(
	FbxSurfaceMaterial* Material,
	FMaterialInfo& OutMaterialInfo,
	const FString& CurrentFbxPath)
{
	if (!Material)
	{
		return;
	}

	// 머티리얼 이름 설정
	OutMaterialInfo.MaterialName = FFbxHelper::GetMaterialName(Material);

	// Phong 또는 Lambert 머티리얼 속성 추출
	if (Material->GetClassId().Is(FbxSurfacePhong::ClassId))
	{
		FbxSurfacePhong* PhongMaterial = static_cast<FbxSurfacePhong*>(Material);
		ExtractPhongMaterialProperties(PhongMaterial, OutMaterialInfo);
	}
	else if (Material->GetClassId().Is(FbxSurfaceLambert::ClassId))
	{
		FbxSurfaceLambert* LambertMaterial = static_cast<FbxSurfaceLambert*>(Material);
		ExtractLambertMaterialProperties(LambertMaterial, OutMaterialInfo);
	}

	// 모든 텍스처 슬롯 추출
	ExtractMaterialTextures(Material, OutMaterialInfo, CurrentFbxPath);
}

/**
 * ExtractPhongMaterialProperties
 *
 * Phong 머티리얼의 속성 추출 (Diffuse, Ambient, Specular, Shininess 등)
 */
void FFbxMaterial::ExtractPhongMaterialProperties(
	FbxSurfacePhong* PhongMaterial,
	FMaterialInfo& OutMaterialInfo)
{
	if (!PhongMaterial)
	{
		return;
	}

	// Diffuse (확산광)
	FbxPropertyT<FbxDouble3> DiffuseProp = PhongMaterial->Diffuse;
	OutMaterialInfo.DiffuseColor = FVector(
		DiffuseProp.Get()[0],
		DiffuseProp.Get()[1],
		DiffuseProp.Get()[2]);

	// Ambient (환경광)
	FbxPropertyT<FbxDouble3> AmbientProp = PhongMaterial->Ambient;
	OutMaterialInfo.AmbientColor = FVector(
		AmbientProp.Get()[0],
		AmbientProp.Get()[1],
		AmbientProp.Get()[2]);

	// Specular (반사광) - Factor 포함
	FbxPropertyT<FbxDouble3> SpecularProp = PhongMaterial->Specular;
	FbxPropertyT<FbxDouble> SpecularFactorProp = PhongMaterial->SpecularFactor;
	OutMaterialInfo.SpecularColor = FVector(
		SpecularProp.Get()[0],
		SpecularProp.Get()[1],
		SpecularProp.Get()[2]) * SpecularFactorProp.Get();

	// Shininess (광택 지수)
	FbxPropertyT<FbxDouble> ShininessProp = PhongMaterial->Shininess;
	OutMaterialInfo.SpecularExponent = ShininessProp.Get();

	// Transparency (투명도)
	FbxPropertyT<FbxDouble> TransparencyProp = PhongMaterial->TransparencyFactor;
	OutMaterialInfo.Transparency = TransparencyProp.Get();

	// NOTE: Emissive는 HDR 미지원으로 주석 처리
	// FbxPropertyT<FbxDouble3> EmissiveProp = PhongMaterial->Emissive;
	// OutMaterialInfo.EmissiveColor = FVector(...);
}

/**
 * ExtractLambertMaterialProperties
 *
 * Lambert 머티리얼의 속성 추출 (Diffuse, Ambient, Transparency)
 */
void FFbxMaterial::ExtractLambertMaterialProperties(
	FbxSurfaceLambert* LambertMaterial,
	FMaterialInfo& OutMaterialInfo)
{
	if (!LambertMaterial)
	{
		return;
	}

	// Diffuse (확산광)
	FbxPropertyT<FbxDouble3> DiffuseProp = LambertMaterial->Diffuse;
	OutMaterialInfo.DiffuseColor = FVector(
		DiffuseProp.Get()[0],
		DiffuseProp.Get()[1],
		DiffuseProp.Get()[2]);

	// Ambient (환경광)
	FbxPropertyT<FbxDouble3> AmbientProp = LambertMaterial->Ambient;
	OutMaterialInfo.AmbientColor = FVector(
		AmbientProp.Get()[0],
		AmbientProp.Get()[1],
		AmbientProp.Get()[2]);

	// Transparency (투명도)
	FbxPropertyT<FbxDouble> TransparencyProp = LambertMaterial->TransparencyFactor;
	OutMaterialInfo.Transparency = TransparencyProp.Get();

	// NOTE: Emissive는 HDR 미지원으로 주석 처리
	// NOTE: Lambert는 Specular 속성이 없음
}

/**
 * ExtractMaterialTextures
 *
 * 머티리얼의 모든 텍스처 슬롯에서 텍스처 경로 추출
 */
void FFbxMaterial::ExtractMaterialTextures(
	FbxSurfaceMaterial* Material,
	FMaterialInfo& OutMaterialInfo,
	const FString& CurrentFbxPath)
{
	if (!Material)
	{
		return;
	}

	// Diffuse Texture
	FbxProperty DiffuseProp = Material->FindProperty(FbxSurfaceMaterial::sDiffuse);
	OutMaterialInfo.DiffuseTextureFileName = ExtractTextureInfo(DiffuseProp, CurrentFbxPath);

	// Normal Map Texture
	FbxProperty NormalProp = Material->FindProperty(FbxSurfaceMaterial::sNormalMap);
	OutMaterialInfo.NormalTextureFileName = ExtractTextureInfo(NormalProp, CurrentFbxPath);

	// Ambient Texture
	FbxProperty AmbientProp = Material->FindProperty(FbxSurfaceMaterial::sAmbient);
	OutMaterialInfo.AmbientTextureFileName = ExtractTextureInfo(AmbientProp, CurrentFbxPath);

	// Specular Texture
	FbxProperty SpecularProp = Material->FindProperty(FbxSurfaceMaterial::sSpecular);
	OutMaterialInfo.SpecularTextureFileName = ExtractTextureInfo(SpecularProp, CurrentFbxPath);

	// Emissive Texture
	FbxProperty EmissiveProp = Material->FindProperty(FbxSurfaceMaterial::sEmissive);
	OutMaterialInfo.EmissiveTextureFileName = ExtractTextureInfo(EmissiveProp, CurrentFbxPath);

	// Transparency Texture
	FbxProperty TransparencyProp = Material->FindProperty(FbxSurfaceMaterial::sTransparencyFactor);
	OutMaterialInfo.TransparencyTextureFileName = ExtractTextureInfo(TransparencyProp, CurrentFbxPath);

	// Shininess Texture (Specular Exponent)
	FbxProperty ShininessProp = Material->FindProperty(FbxSurfaceMaterial::sShininess);
	OutMaterialInfo.SpecularExponentTextureFileName = ExtractTextureInfo(ShininessProp, CurrentFbxPath);
}

/**
 * ExtractTextureInfo
 *
 * FBX Property에서 텍스처 경로 추출 및 DDS 캐시 처리
 */
FString FFbxMaterial::ExtractTextureInfo(
	FbxProperty& Property,
	const FString& CurrentFbxPath)
{
	// 1. Property 유효성 검사
	if (!Property.IsValid())
	{
		return FString();
	}

	if (Property.GetSrcObjectCount<FbxFileTexture>() <= 0)
	{
		return FString();
	}

	FbxFileTexture* Texture = Property.GetSrcObject<FbxFileTexture>(0);
	if (!Texture)
	{
		return FString();
	}

	// 2. FBX에서 원본 텍스처 경로 가져오기
	FString OriginalTexturePath = FString(Texture->GetFileName());
	if (OriginalTexturePath.empty())
	{
		return FString();
	}

	// 3. 실제 파일 시스템 경로 찾기 (.fbm 폴더, 상대 경로 처리)
	FString ActualTexturePath = FindActualTexturePath(OriginalTexturePath, CurrentFbxPath);

	// 4. DDS 캐시 처리
	return ProcessDDSCache(ActualTexturePath, CurrentFbxPath);
}

/**
 * FindActualTexturePath
 *
 * FBX에서 추출한 텍스처 경로를 실제 파일 시스템 경로로 해석
 */
FString FFbxMaterial::FindActualTexturePath(
	const FString& OriginalTexturePath,
	const FString& CurrentFbxPath)
{
	namespace fs = std::filesystem;

	FString ActualTexturePath = OriginalTexturePath;

	// FBX 파일 경로가 없으면 원본 경로 반환
	if (CurrentFbxPath.empty())
	{
		return ActualTexturePath;
	}

	// FBX 파일 경로 파싱
	fs::path FbxPath(UTF8ToWide(CurrentFbxPath));

	// .fbm 폴더 경로 구성: "FbxFileName.fbx.fbm"
	fs::path FbmDir = FbxPath.parent_path() / (FbxPath.stem().wstring() + L".fbm");

	// 텍스처 파일명 추출
	fs::path TextureName = fs::path(UTF8ToWide(OriginalTexturePath)).filename();
	fs::path TextureInFbm = FbmDir / TextureName;

	// .fbm 폴더에 텍스처가 있으면 사용
	if (fs::exists(TextureInFbm))
	{
		return WideToUTF8(TextureInFbm.wstring());
	}

	// 원본 경로가 존재하지 않으면 FBX 기준 상대 경로 시도
	if (!fs::exists(UTF8ToWide(OriginalTexturePath)))
	{
		fs::path RelativePath = FbxPath.parent_path() / fs::path(UTF8ToWide(OriginalTexturePath)).filename();
		if (fs::exists(RelativePath))
		{
			return WideToUTF8(RelativePath.wstring());
		}
	}

	// 모든 시도 실패 시 원본 경로 반환
	return ActualTexturePath;
}

/**
 * ProcessDDSCache
 *
 * DDS 캐시 생성/갱신 처리
 */
FString FFbxMaterial::ProcessDDSCache(
	const FString& ActualTexturePath,
	const FString& CurrentFbxPath)
{
	namespace fs = std::filesystem;

	// 1. DDS 캐시 경로 생성
	FString DDSCachePath = FTextureConverter::GetDDSCachePath(ActualTexturePath);

	// 2. DDS 캐시 재생성 필요 여부 확인
	bool bShouldRegenerate = false;
	bool bIsFbmTexture = ActualTexturePath.find(".fbm") != FString::npos;

	if (bIsFbmTexture && !CurrentFbxPath.empty())
	{
		// .fbm 텍스처는 FBX 타임스탬프 기준으로 재생성 여부 판단
		bShouldRegenerate = FTextureConverter::ShouldRegenerateDDS_Fbm(
			ActualTexturePath, DDSCachePath, CurrentFbxPath);
	}
	else
	{
		// 일반 텍스처는 텍스처 파일 타임스탬프 기준
		bShouldRegenerate = FTextureConverter::ShouldRegenerateDDS(
			ActualTexturePath, DDSCachePath);
	}

	// 3. 필요 시 DDS 변환 수행
	if (bShouldRegenerate)
	{
		if (fs::exists(UTF8ToWide(ActualTexturePath)))
		{
			if (!FTextureConverter::ConvertToDDS(ActualTexturePath, DDSCachePath))
			{
				UE_LOG("[FFbxMaterial] Warning: DDS conversion failed for %s, using original path",
					ActualTexturePath.c_str());
				return ActualTexturePath;
			}
		}
		else
		{
			UE_LOG("[FFbxMaterial] Warning: Texture file not found: %s", ActualTexturePath.c_str());
			return ActualTexturePath;
		}
	}

	// 4. DDS 캐시 경로 반환
	return DDSCachePath;
}

#pragma warning(pop) // Restore warning state
