#pragma once
#include "String.h"
#include "fbxsdk.h"
#include "ResourceData.h"

/**
 * FFbxMaterial
 *
 * FBX 머티리얼 및 텍스처 추출 유틸리티
 * Phong/Lambert 머티리얼 속성 파싱, 텍스처 경로 추출 및 DDS 변환 처리
 *
 * UE5 Pattern: FFbxMaterial (static utility functions)
 * Location: Engine/Plugins/Interchange/Runtime/Source/Parsers/Fbx/Private/FbxMaterial.h
 *
 * Refactoring Phase 3 (v2.0): FBXLoader의 material 파싱 로직을 중앙화
 * NOTE: 즉시 로딩 패턴 사용 (지연 로딩 미사용)
 */
struct FFbxMaterial
{
	/**
	 * ExtractMaterialProperties
	 *
	 * FBX 머티리얼에서 속성을 추출하여 FMaterialInfo로 변환
	 * Phong, Lambert 머티리얼 타입 지원
	 *
	 * @param Material - FBX 머티리얼 객체
	 * @param OutMaterialInfo - 추출된 머티리얼 정보를 저장할 구조체
	 * @param CurrentFbxPath - 현재 로드 중인 FBX 파일 경로 (텍스처 경로 해석용)
	 *
	 * NOTE: UResourceManager에 머티리얼을 등록하고 MaterialInfos에 추가하는 것은
	 *       호출자(UFbxLoader)의 책임입니다.
	 */
	static void ExtractMaterialProperties(
		FbxSurfaceMaterial* Material,
		FMaterialInfo& OutMaterialInfo,
		const FString& CurrentFbxPath);

	/**
	 * ExtractTextureInfo
	 *
	 * FBX Property에서 텍스처 파일 경로를 추출하고 DDS 캐시 처리
	 * .fbm 폴더 처리, 상대 경로 해석, DDS 변환 등을 자동 처리
	 *
	 * @param Property - 텍스처 정보를 포함한 FBX Property
	 * @param CurrentFbxPath - 현재 로드 중인 FBX 파일 경로
	 * @return DDS 캐시 경로 또는 원본 텍스처 경로 (빈 문자열은 텍스처 없음)
	 *
	 * 처리 순서:
	 * 1. FBX에서 원본 텍스처 경로 추출
	 * 2. .fbm 폴더에서 embedded 텍스처 검색
	 * 3. 상대 경로 해석 (FBX 파일 기준)
	 * 4. DDS 캐시 생성/갱신 여부 확인
	 * 5. 필요 시 DDS 변환 수행
	 * 6. DDS 캐시 경로 반환
	 */
	static FString ExtractTextureInfo(
		FbxProperty& Property,
		const FString& CurrentFbxPath);

private:
	/**
	 * ExtractPhongMaterialProperties
	 *
	 * FbxSurfacePhong 머티리얼의 속성 추출
	 * Diffuse, Ambient, Specular, Shininess, Transparency 등
	 *
	 * @param PhongMaterial - Phong 머티리얼 객체
	 * @param OutMaterialInfo - 추출된 정보를 저장할 구조체
	 */
	static void ExtractPhongMaterialProperties(
		FbxSurfacePhong* PhongMaterial,
		FMaterialInfo& OutMaterialInfo);

	/**
	 * ExtractLambertMaterialProperties
	 *
	 * FbxSurfaceLambert 머티리얼의 속성 추출
	 * Diffuse, Ambient, Transparency 등
	 *
	 * @param LambertMaterial - Lambert 머티리얼 객체
	 * @param OutMaterialInfo - 추출된 정보를 저장할 구조체
	 */
	static void ExtractLambertMaterialProperties(
		FbxSurfaceLambert* LambertMaterial,
		FMaterialInfo& OutMaterialInfo);

	/**
	 * ExtractMaterialTextures
	 *
	 * 머티리얼의 모든 텍스처 슬롯에서 텍스처 경로 추출
	 * Diffuse, Normal, Ambient, Specular, Emissive, Transparency, Shininess 텍스처 처리
	 *
	 * @param Material - FBX 머티리얼 객체
	 * @param OutMaterialInfo - 텍스처 경로를 저장할 구조체
	 * @param CurrentFbxPath - 현재 FBX 파일 경로
	 */
	static void ExtractMaterialTextures(
		FbxSurfaceMaterial* Material,
		FMaterialInfo& OutMaterialInfo,
		const FString& CurrentFbxPath);

	/**
	 * FindActualTexturePath
	 *
	 * FBX에서 추출한 텍스처 경로를 실제 파일 시스템 경로로 해석
	 * .fbm 폴더 검색, 상대 경로 처리 등
	 *
	 * @param OriginalTexturePath - FBX에 저장된 원본 텍스처 경로
	 * @param CurrentFbxPath - 현재 FBX 파일 경로
	 * @return 실제 파일 시스템 경로
	 */
	static FString FindActualTexturePath(
		const FString& OriginalTexturePath,
		const FString& CurrentFbxPath);

	/**
	 * ProcessDDSCache
	 *
	 * DDS 캐시 생성/갱신 처리
	 * 타임스탬프 비교 및 변환 수행
	 *
	 * @param ActualTexturePath - 실제 텍스처 파일 경로
	 * @param CurrentFbxPath - 현재 FBX 파일 경로
	 * @return DDS 캐시 경로
	 */
	static FString ProcessDDSCache(
		const FString& ActualTexturePath,
		const FString& CurrentFbxPath);
};
