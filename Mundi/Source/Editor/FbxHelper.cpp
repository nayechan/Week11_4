#include "pch.h"
#include "FbxHelper.h"
#include <algorithm>
#include <sstream>

// ========================================
// FBX 노드 및 객체 이름 유틸리티
// ========================================

/**
 * GetFbxNodeHierarchyName
 *
 * 노드의 전체 계층 경로를 역순으로 탐색하여 생성
 * 예: "Root/Armature/Spine/Head"
 */
FString FFbxHelper::GetFbxNodeHierarchyName(const FbxNode* Node)
{
	if (!Node)
	{
		return "";
	}

	// 계층 경로를 역순으로 수집
	TArray<FString> HierarchyNames;
	const FbxNode* CurrentNode = Node;

	while (CurrentNode)
	{
		FString NodeName = GetFbxObjectName(CurrentNode, false);
		if (!NodeName.empty())
		{
			HierarchyNames.push_back(NodeName);
		}
		CurrentNode = CurrentNode->GetParent();
	}

	// 역순으로 수집했으므로 뒤집기
	std::reverse(HierarchyNames.begin(), HierarchyNames.end());

	// "/" 구분자로 결합
	std::ostringstream PathBuilder;
	for (size_t i = 0; i < HierarchyNames.size(); ++i)
	{
		if (i > 0)
		{
			PathBuilder << "/";
		}
		PathBuilder << HierarchyNames[i];
	}

	return PathBuilder.str();
}

/**
 * GetFbxObjectName
 *
 * FBX 객체의 이름을 가져오고 정리
 */
FString FFbxHelper::GetFbxObjectName(const FbxObject* Object, bool bIncludeNamespace)
{
	if (!Object)
	{
		return "";
	}

	FString ObjectName = Object->GetName();

	// 네임스페이스 처리
	if (!bIncludeNamespace)
	{
		ObjectName = RemoveNamespace(ObjectName);
	}

	// 이름 정리
	ObjectName = SanitizeName(ObjectName, false);

	return ObjectName;
}

/**
 * GetMeshName
 *
 * 메시의 고유 이름 생성
 * 노드 이름을 기반으로 하며, 인덱스를 추가하여 중복 방지
 */
FString FFbxHelper::GetMeshName(const FbxNode* MeshNode, int32 MeshIndex)
{
	if (!MeshNode)
	{
		return "Unknown_Mesh";
	}

	FString BaseName = GetFbxObjectName(MeshNode, false);

	// 비어있는 경우 기본 이름
	if (BaseName.empty())
	{
		BaseName = "Mesh";
	}

	// 여러 메시가 있는 경우 인덱스 추가
	if (MeshIndex > 0)
	{
		std::ostringstream NameBuilder;
		NameBuilder << BaseName << "_" << MeshIndex;
		return NameBuilder.str();
	}

	return BaseName;
}

/**
 * GetMeshUniqueID
 *
 * 메시의 유니크 ID 생성 (페이로드 키로 사용)
 * 계층 경로를 포함하여 전역적으로 유니크함을 보장
 */
FString FFbxHelper::GetMeshUniqueID(const FbxNode* MeshNode, int32 MeshIndex)
{
	if (!MeshNode)
	{
		return "Unknown";
	}

	// 계층 경로를 ID로 사용
	FString HierarchyPath = GetFbxNodeHierarchyName(MeshNode);

	// 메시 인덱스 추가
	if (MeshIndex > 0)
	{
		std::ostringstream IDBuilder;
		IDBuilder << HierarchyPath << "_Mesh" << MeshIndex;
		return IDBuilder.str();
	}

	return HierarchyPath;
}

/**
 * SanitizeName
 *
 * 유효하지 않은 문자 제거 및 이름 정리
 * 파일 시스템 및 엔진에서 안전한 이름 생성
 */
FString FFbxHelper::SanitizeName(const FString& InName, bool bRemoveNamespace)
{
	FString Result = InName;

	// 네임스페이스 제거
	if (bRemoveNamespace)
	{
		Result = RemoveNamespace(Result);
	}

	// 유효하지 않은 문자 제거
	// 파일 시스템에서 문제가 될 수 있는 문자: < > : " / \ | ? *
	const char InvalidChars[] = {'<', '>', ':', '\"', '/', '\\', '|', '?', '*'};
	for (char InvalidChar : InvalidChars)
	{
		std::replace(Result.begin(), Result.end(), InvalidChar, '_');
	}

	// 앞뒤 공백 제거
	auto NotSpace = [](unsigned char ch) { return !std::isspace(ch); };

	// 앞쪽 공백 제거
	Result.erase(Result.begin(), std::find_if(Result.begin(), Result.end(), NotSpace));

	// 뒤쪽 공백 제거
	Result.erase(std::find_if(Result.rbegin(), Result.rend(), NotSpace).base(), Result.end());

	// 비어있는 경우 기본 이름
	if (Result.empty())
	{
		Result = "Unnamed";
	}

	return Result;
}

/**
 * GetMaterialName
 *
 * 머티리얼 이름 생성 및 정리
 */
FString FFbxHelper::GetMaterialName(const FbxSurfaceMaterial* Material)
{
	if (!Material)
	{
		return "DefaultMaterial";
	}

	FString MaterialName = Material->GetName();
	MaterialName = SanitizeName(MaterialName, true);

	// 비어있는 경우 기본 이름
	if (MaterialName.empty())
	{
		MaterialName = "Material";
	}

	return MaterialName;
}

/**
 * RemoveNamespace
 *
 * FBX 네임스페이스 제거
 * 예: "Character:Mesh" -> "Mesh"
 *     "Armature:Bone01" -> "Bone01"
 */
FString FFbxHelper::RemoveNamespace(const FString& InName)
{
	// ':' 문자를 찾아 그 뒤의 부분만 반환
	size_t ColonPos = InName.find_last_of(':');
	if (ColonPos != FString::npos && ColonPos < InName.length() - 1)
	{
		return InName.substr(ColonPos + 1);
	}

	return InName;
}

/**
 * NormalizeNodeName
 *
 * 노드 이름 정규화
 * 일관된 이름 형식 유지
 */
FString FFbxHelper::NormalizeNodeName(const FString& NodeName)
{
	FString Result = NodeName;

	// 네임스페이스 제거
	Result = RemoveNamespace(Result);

	// 유효하지 않은 문자 제거
	Result = SanitizeName(Result, false);

	// 연속된 공백을 단일 공백으로 변환
	auto NewEnd = std::unique(Result.begin(), Result.end(),
		[](char a, char b) { return std::isspace(a) && std::isspace(b); });
	Result.erase(NewEnd, Result.end());

	return Result;
}
