#pragma once
#include "String.h"
#include "fbxsdk.h"

/**
 * FFbxHelper
 *
 * FBX 이름 생성 및 유틸리티 함수 모음
 * 메시 이름, 노드 경로, 머티리얼 이름 등을 표준화하고 정리
 *
 * UE5 Pattern: FFbxHelper (static utility functions)
 * Location: Engine/Plugins/Interchange/Runtime/Source/Parsers/Fbx/Private/FbxHelper.h
 *
 * Refactoring Phase 2 (v2.0): FBXLoader의 naming 로직을 중앙화
 * NOTE: FPayloadContextBase 제거됨 (지연 로딩 패턴 미사용)
 */
struct FFbxHelper
{
	/**
	 * GetFbxNodeHierarchyName
	 *
	 * FBX 노드의 전체 계층 경로를 생성 (예: "Root/Armature/Bone01")
	 *
	 * @param Node - 경로를 생성할 FBX 노드
	 * @return 계층적 경로 문자열
	 */
	static FString GetFbxNodeHierarchyName(const FbxNode* Node);

	/**
	 * GetFbxObjectName
	 *
	 * FBX 객체 이름을 정리하고 표준화
	 * 특수 문자 제거, 공백 처리 등
	 *
	 * @param Object - 이름을 가져올 FBX 객체
	 * @param bIncludeNamespace - 네임스페이스 포함 여부
	 * @return 정리된 객체 이름
	 */
	static FString GetFbxObjectName(const FbxObject* Object, bool bIncludeNamespace = false);

	/**
	 * GetMeshName
	 *
	 * 메시의 고유 이름 생성
	 * 중복 방지를 위해 계층 정보 포함
	 *
	 * @param MeshNode - 메시 노드
	 * @param MeshIndex - 동일 노드 내 메시 인덱스 (다중 메시 지원)
	 * @return 생성된 메시 이름
	 */
	static FString GetMeshName(const FbxNode* MeshNode, int32 MeshIndex = 0);

	/**
	 * GetMeshUniqueID
	 *
	 * 메시의 유니크 ID 생성
	 * 계층 경로를 포함하여 전역적으로 유니크함을 보장
	 *
	 * @param MeshNode - 메시 노드
	 * @param MeshIndex - 메시 인덱스
	 * @return 유니크 ID 문자열
	 */
	static FString GetMeshUniqueID(const FbxNode* MeshNode, int32 MeshIndex = 0);

	/**
	 * SanitizeName
	 *
	 * 이름에서 유효하지 않은 문자 제거
	 * 파일 시스템 및 엔진에서 안전한 이름 생성
	 *
	 * @param InName - 입력 이름
	 * @param bRemoveNamespace - 네임스페이스 제거 여부
	 * @return 정리된 이름
	 */
	static FString SanitizeName(const FString& InName, bool bRemoveNamespace = false);

	/**
	 * GetMaterialName
	 *
	 * 머티리얼 이름 생성 및 정리
	 * 중복 방지 및 표준화
	 *
	 * @param Material - FBX 머티리얼
	 * @return 정리된 머티리얼 이름
	 */
	static FString GetMaterialName(const FbxSurfaceMaterial* Material);

	/**
	 * RemoveNamespace
	 *
	 * FBX 네임스페이스 제거 (예: "Character:Mesh" -> "Mesh")
	 *
	 * @param InName - 입력 이름
	 * @return 네임스페이스가 제거된 이름
	 */
	static FString RemoveNamespace(const FString& InName);

	/**
	 * NormalizeNodeName
	 *
	 * 노드 이름 정규화
	 * 대소문자, 공백, 특수문자 처리
	 *
	 * @param NodeName - 입력 노드 이름
	 * @return 정규화된 이름
	 */
	static FString NormalizeNodeName(const FString& NodeName);
};
