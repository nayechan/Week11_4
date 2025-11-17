#include "pch.h"
#pragma warning(push)
#pragma warning(disable: 4244) // FBX SDK의 double에서 float 변환 경고 비활성화
#include "FbxMesh.h"
#include "FbxMaterial.h"
#include "FbxConvert.h"
#include "fbxsdk/scene/geometry/fbxcluster.h"
#include "Material.h"
#include "ResourceManager.h"
#include "GlobalConsole.h"

// ========================================
// FBX 메시 추출 구현
// ========================================

/**
 * ShouldReverseWindingOrder
 *
 * 메시의 winding order를 자동 감지하여 reverse가 필요한지 판단
 *
 * @param Vertices - 메시의 정점 데이터
 * @param IndexList - 메시의 인덱스 리스트
 * @return true면 CCW (reverse 필요), false면 CW (reverse 불필요)
 *
 * 알고리즘:
 * 1. 첫 번째 삼각형의 정점으로 외적 계산 (계산된 법선 벡터)
 * 2. FBX에서 제공하는 법선과 내적 비교
 * 3. 음수면 CCW (법선 반대 방향), 양수면 CW (법선 같은 방향)
 */
static bool ShouldReverseWindingOrder(
	const TArray<FSkinnedVertex>& Vertices,
	const TArray<uint32>& IndexList)
{
	// 최소 1개의 삼각형 필요
	if (IndexList.Num() < 3 || Vertices.Num() == 0)
	{
		// 기본값: reverse 수행 (안전한 fallback)
		return true;
	}

	// 첫 번째 유효한 삼각형 찾기 (degenerate triangle 스킵)
	for (int32 i = 0; i < IndexList.Num(); i += 3)
	{
		if (i + 2 >= IndexList.Num()) break;

		uint32 i0 = IndexList[i];
		uint32 i1 = IndexList[i + 1];
		uint32 i2 = IndexList[i + 2];

		// 인덱스 범위 검증
		if (i0 >= (uint32)Vertices.Num() || i1 >= (uint32)Vertices.Num() || i2 >= (uint32)Vertices.Num())
		{
			continue;
		}

		const FSkinnedVertex& v0 = Vertices[i0];
		const FSkinnedVertex& v1 = Vertices[i1];
		const FSkinnedVertex& v2 = Vertices[i2];

		// 외적으로 계산된 법선 벡터 계산 (오른손 법칙)
		FVector edge1 = v1.Position - v0.Position;
		FVector edge2 = v2.Position - v0.Position;
		FVector computedNormal = FVector::Cross(edge1, edge2);

		// 퇴화 삼각형 체크 (면적이 0인 경우)
		float computedLength = computedNormal.Size();
		if (computedLength < 1e-6f)
		{
			continue; // 다음 삼각형 시도
		}

		computedNormal.Normalize();

		// FBX에서 제공하는 평균 법선
		FVector fbxNormal = (v0.Normal + v1.Normal + v2.Normal) / 3.0f;

		// 법선이 영벡터인 경우 처리
		float fbxNormalLength = fbxNormal.Size();
		if (fbxNormalLength < 1e-6f)
		{
			continue; // 다음 삼각형 시도
		}

		fbxNormal.Normalize();

		// 내적으로 방향 비교
		float dot = FVector::Dot(computedNormal, fbxNormal);

		// 결과 해석:
		// dot > 0: 법선 방향 일치 → CW (Mundi 타겟) → reverse 불필요
		// dot < 0: 법선 방향 반대 → CCW → reverse 필요
		return (dot < 0.0f);
	}

	// 모든 삼각형이 degenerate인 경우, 기본값으로 reverse 수행
	return true;
}

/**
 * ExtractMeshFromNode
 *
 * FBX 노드를 재귀적으로 순회하며 메시 데이터 추출
 */
void FFbxMesh::ExtractMeshFromNode(
	FbxNode* InNode,
	FSkeletalMeshData& MeshData,
	TMap<int32, TArray<uint32>>& MaterialGroupIndexList,
	TMap<FbxNode*, int32>& BoneToIndex,
	TMap<FbxSurfaceMaterial*, int32>& MaterialToIndex,
	TArray<FMaterialInfo>& MaterialInfos,
	FbxScene* Scene,
	const FString& CurrentFbxPath,
	const FbxAMatrix& JointOrientationMatrix)
{
	// 최적화, 메시 로드 전에 미리 머티리얼로부터 인덱스를 해시맵을 이용해서 얻고 그걸 TArray에 저장하면 됨.
	// 노드의 머티리얼 리스트는 슬롯으로 참조함(내가 정한 MaterialIndex는 슬롯과 다름), 슬롯에 대응하는 머티리얼 인덱스를 캐싱하는 것
	// 그럼 폴리곤 순회하면서 해싱할 필요가 없음
	TArray<int32> MaterialSlotToIndex;

	// Attribute 참조 함수
	for (int Index = 0; Index < InNode->GetNodeAttributeCount(); Index++)
	{
		FbxNodeAttribute* Attribute = InNode->GetNodeAttributeByIndex(Index);
		if (!Attribute)
		{
			continue;
		}

		if (Attribute->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			FbxMesh* Mesh = (FbxMesh*)Attribute;
			// 위의 MaterialSlotToIndex는 MaterialToIndex 해싱을 안 하기 위함이고, MaterialGroupIndexList도 머티리얼이 없거나 1개만 쓰는 경우 해싱을 피할 수 있음.
			// 이를 위한 최적화 코드를 작성함.

			// 0번이 기본 머티리얼이고 1번 이상은 블렌딩 머티리얼이라고 함. 근데 엄청 고급 기능이라서 일반적인 로더는 0번만 쓴다고 함.
			if (Mesh->GetElementMaterialCount() > 0)
			{
				// 머티리얼 슬롯 인덱싱 해주는 클래스 (ex. materialElement->GetIndexArray() : 폴리곤마다 어떤 머티리얼 슬롯을 쓰는지 Array로 표현)
				FbxGeometryElementMaterial* MaterialElement = Mesh->GetElementMaterial(0);
				// 머티리얼이 폴리곤 단위로 매핑함 -> 모든 폴리곤이 같은 머티리얼을 쓰지 않음(같은 머티리얼을 쓰는 경우 = eAllSame)
				// MaterialCount랑은 전혀 다른 동작임(슬롯이 2개 이상 있어도 매핑 모드가 eAllSame이라서 머티리얼을 하나만 쓰는 경우가 있음)
				if (MaterialElement->GetMappingMode() == FbxGeometryElement::eByPolygon)
				{
					for (int MaterialSlot = 0; MaterialSlot < InNode->GetMaterialCount(); MaterialSlot++)
					{
						int MaterialIndex = 0;
						FbxSurfaceMaterial* Material = InNode->GetMaterial(MaterialSlot);
						if (MaterialToIndex.Contains(Material))
						{
							MaterialIndex = MaterialToIndex[Material];
						}
						else
						{
							FMaterialInfo MaterialInfo{};
							FFbxMaterial::ExtractMaterialProperties(Material, MaterialInfo, CurrentFbxPath);

							// UMaterial 생성 및 등록
							UMaterial* NewMaterial = NewObject<UMaterial>();
							UMaterial* Default = UResourceManager::GetInstance().GetDefaultMaterial();
							NewMaterial->SetMaterialInfo(MaterialInfo);
							NewMaterial->SetShader(Default->GetShader());
							NewMaterial->SetShaderMacros(Default->GetShaderMacros());
							MaterialInfos.Add(MaterialInfo);
							UResourceManager::GetInstance().Add<UMaterial>(MaterialInfo.MaterialName, NewMaterial);

							// 새로운 머티리얼, 머티리얼 인덱스 설정
							MaterialIndex = MaterialToIndex.Num();
							MaterialToIndex.Add(Material, MaterialIndex);
							MeshData.GroupInfos.Add(FGroupInfo());
							MeshData.GroupInfos[MaterialIndex].InitialMaterialName = MaterialInfo.MaterialName;
						}
						// MaterialSlot에 대응하는 전역 MaterialIndex 저장
						MaterialSlotToIndex.Add(MaterialIndex);
					}
				}
				// 노드가 하나의 머티리얼만 쓰는 경우
				else if (MaterialElement->GetMappingMode() == FbxGeometryElement::eAllSame)
				{
					int MaterialIndex = 0;
					int MaterialSlot = MaterialElement->GetIndexArray().GetAt(0);
					FbxSurfaceMaterial* Material = InNode->GetMaterial(MaterialSlot);
					if (MaterialToIndex.Contains(Material))
					{
						MaterialIndex = MaterialToIndex[Material];
					}
					else
					{
						FMaterialInfo MaterialInfo{};
						FFbxMaterial::ExtractMaterialProperties(Material, MaterialInfo, CurrentFbxPath);

						// UMaterial 생성 및 등록
						UMaterial* NewMaterial = NewObject<UMaterial>();
						UMaterial* Default = UResourceManager::GetInstance().GetDefaultMaterial();
						NewMaterial->SetMaterialInfo(MaterialInfo);
						NewMaterial->SetShader(Default->GetShader());
						NewMaterial->SetShaderMacros(Default->GetShaderMacros());
						MaterialInfos.Add(MaterialInfo);
						UResourceManager::GetInstance().Add<UMaterial>(MaterialInfo.MaterialName, NewMaterial);

						// 새로운 머티리얼, 머티리얼 인덱스 설정
						MaterialIndex = MaterialToIndex.Num();

						MaterialToIndex.Add(Material, MaterialIndex);
						MeshData.GroupInfos.Add(FGroupInfo());
						MeshData.GroupInfos[MaterialIndex].InitialMaterialName = MaterialInfo.MaterialName;
					}
					// MaterialSlotToIndex에 추가할 필요 없음(머티리얼 하나일때 해싱 패스하고 Material Index로 바로 그룹핑 할 거라서 안 씀)
					ExtractMesh(Mesh, MeshData, MaterialGroupIndexList, BoneToIndex, MaterialSlotToIndex, Scene, MaterialIndex, JointOrientationMatrix);
					continue;
				}
			}

			ExtractMesh(Mesh, MeshData, MaterialGroupIndexList, BoneToIndex, MaterialSlotToIndex, Scene, 0, JointOrientationMatrix);
		}
	}

	for (int Index = 0; Index < InNode->GetChildCount(); Index++)
	{
		ExtractMeshFromNode(InNode->GetChild(Index), MeshData, MaterialGroupIndexList, BoneToIndex, MaterialToIndex, MaterialInfos, Scene, CurrentFbxPath, JointOrientationMatrix);
	}
}

/**
 * ExtractMesh
 *
 * FbxMesh에서 정점, 인덱스, 스키닝, 머티리얼 데이터 추출
 */
void FFbxMesh::ExtractMesh(
	FbxMesh* InMesh,
	FSkeletalMeshData& MeshData,
	TMap<int32, TArray<uint32>>& MaterialGroupIndexList,
	TMap<FbxNode*, int32>& BoneToIndex,
	TArray<int32> MaterialSlotToIndex,
	FbxScene* Scene,
	int32 DefaultMaterialIndex,
	const FbxAMatrix& JointOrientationMatrix)
{
	// 위에서 뼈 인덱스를 구했으므로 일단 ControlPoint에 대응되는 뼈 인덱스와 가중치부터 할당할 것임(이후 MeshData를 채우면서 ControlPoint를 순회할 것이므로)
	struct IndexWeight
	{
		uint32 BoneIndex;
		float BoneWeight;
	};
	// ControlPoint에 대응하는 뼈 인덱스, 가중치를 저장하는 맵
	// ControlPoint에 대응하는 뼈가 여러개일 수 있으므로 TArray로 저장
	TMap<int32, TArray<IndexWeight>> ControlPointToBoneWeight;
	// 메시 로컬 좌표계를 Fbx Scene World 좌표계로 바꿔주는 행렬
	FbxAMatrix FbxSceneWorld{};
	// 역전치(노말용)
	FbxAMatrix FbxSceneWorldInverseTranspose{};

	// Deformer: 매시의 모양을 변형시키는 모든 기능, ex) skin, blendShape(모프 타겟, 두 표정 미리 만들고 블랜딩해서 서서히 변화시킴)
	// 99.9퍼센트는 스킨이 하나만 있고 완전 복잡한 얼굴 표정을 표현하기 위해서 2개 이상을 쓰기도 하는데 0번만 쓰도록 해도 문제 없음(AAA급 게임에서 2개 이상을 처리함)
	// 2개 이상의 스킨이 들어가면 뼈 인덱스가 16개까지도 늘어남.
	if (InMesh->GetDeformerCount(FbxDeformer::eSkin) > 0)
	{
		// 클러스터: 뼈라고 봐도 됨(뼈 정보와(Bind Pose 행렬) 그 뼈가 영향을 주는 정점, 가중치 저장)
		for (int Index = 0; Index < ((FbxSkin*)InMesh->GetDeformer(0, FbxDeformer::eSkin))->GetClusterCount(); Index++)
		{
			FbxCluster* Cluster = ((FbxSkin*)InMesh->GetDeformer(0, FbxDeformer::eSkin))->GetCluster(Index);

			// 주의: FbxSceneWorld는 이제 ComputeSkeletalMeshTotalMatrix()를 사용하여 계산됨
			// GeometricTransform을 올바르게 처리하기 위해 Cluster->GetTransformMatrix() 대신 사용

			int IndexCount = Cluster->GetControlPointIndicesCount();
			// 클러스터가 영향을 주는 ControlPointIndex를 구함.
			int* Indices = Cluster->GetControlPointIndices();
			double* Weights = Cluster->GetControlPointWeights();

			// ========================================
			// Cluster 기반 Bind Pose
			// ========================================

			// 중요: Cluster TransformLinkMatrix 사용 (스키닝 Bind Pose)
			// AnimationEvaluator Time 0 (Scene Pose) 사용 금지 - 둘이 다를 수 있음!
			FbxNode* BoneNode = Cluster->GetLink();

			// Cluster에서 TransformLinkMatrix 획득 (실제 스키닝 Bind Pose)
			FbxAMatrix TransformLinkMatrix;
			Cluster->GetTransformLinkMatrix(TransformLinkMatrix);

			// ========================================
			// UE5 Pattern: JointOrientationMatrix 사용
			// ========================================
			// UE5 Reference: FbxMesh.cpp Line 2180
			//
			// JointOrientationMatrix는 ConvertScene()에서 한 번 설정되고
			// 모든 BindPose 계산에서 재사용됨
			//
			// 값:
			// - bForceFrontXAxis = false: Identity (-Y Forward, 기본값)
			// - bForceFrontXAxis = true:  SetR(-90°, -90°, 0°) (+X Forward)
			TransformLinkMatrix = TransformLinkMatrix * JointOrientationMatrix;

			// Y-Flip 적용하여 Mundi FMatrix로 변환 (Right-Handed → Left-Handed)
			FMatrix GlobalBindPoseMatrix = FFbxConvert::ConvertMatrix(TransformLinkMatrix);

			// Y-Flip 적용하여 Inverse Bind Pose 계산
			FbxAMatrix InverseBindMatrix = TransformLinkMatrix.Inverse();
			FMatrix InverseBindPoseMatrix = FFbxConvert::ConvertMatrix(InverseBindMatrix);

			// Skeleton에 저장
			MeshData.Skeleton.Bones[BoneToIndex[BoneNode]].BindPose = GlobalBindPoseMatrix;
			MeshData.Skeleton.Bones[BoneToIndex[BoneNode]].InverseBindPose = InverseBindPoseMatrix;


			for (int ControlPointIndex = 0; ControlPointIndex < IndexCount; ControlPointIndex++)
			{
				// GetLink -> 아까 저장한 노드 To Index맵의 노드 (Cluster에 대응되는 뼈 인덱스를 ControlPointIndex에 대응시키는 과정)
				// ControlPointIndex = 클러스터가 저장하는 ControlPointIndex 배열에 대한 Index
				TArray<IndexWeight>& IndexWeightArray = ControlPointToBoneWeight[Indices[ControlPointIndex]];
				IndexWeightArray.Add(IndexWeight(BoneToIndex[Cluster->GetLink()], Weights[ControlPointIndex]));
			}
		}
	}

	bool bIsUniformScale = false;
	const FbxVector4& ScaleOfSceneWorld = FbxSceneWorld.GetS();
	// 비균등 스케일일 경우 그람슈미트 이용해서 탄젠트 재계산
	bIsUniformScale = ((FMath::Abs(ScaleOfSceneWorld[0] - ScaleOfSceneWorld[1]) < 0.001f) &&
		(FMath::Abs(ScaleOfSceneWorld[0] - ScaleOfSceneWorld[2]) < 0.001f));


	// ========================================
	// UE5 패턴: Cluster TransformMatrix 대신 TotalMatrix 사용
	// ========================================
	// 중요: TotalMatrix 계산 (GlobalTransform * GeometricTransform)
	// DCC 툴의 pivot, rotation offset, scaling offset 포함
	// 참조: UE5 FbxSkeletalMeshImport.cpp Line 1607, 1624-1625
	FbxNode* MeshNode = InMesh->GetNode();
	FbxSceneWorld = ComputeSkeletalMeshTotalMatrix(MeshNode, Scene);
	FbxSceneWorldInverseTranspose = FbxSceneWorld.Inverse().Transpose();

	// 로드는 TriangleList를 가정하고 할 것임.
	// TriangleStrip은 한번 만들면 편집이 사실상 불가능함, Fbx같은 호환성이 중요한 모델링 포멧이 유연성 부족한 모델을 저장할 이유도 없고
	// 엔진 최적화 측면에서도 GPU의 Vertex Cache가 Strip과 비슷한 성능을 내면서도 직관적이고 유연해서 잘 쓰지도 않기 때문에 그냥 안 씀.
	int PolygonCount = InMesh->GetPolygonCount();

	// ControlPoints는 정점의 위치 정보를 배열로 저장함, Vertex마다 ControlIndex로 참조함.
	FbxVector4* ControlPoints = InMesh->GetControlPoints();


	// Vertex 위치가 같아도 서로 다른 Normal, Tangent, UV좌표를 가질 수 있음, Fbx는 하나의 인덱스 배열에서 이들을 서로 다른 인덱스로 관리하길 강제하지 않고
	// Vertex 위치는 ControlPoint로 관리하고 그 외의 정보들은 선택적으로 분리해서 관리하도록 함. 그래서 ControlPoint를 Index로 쓸 수도 없어서 따로 만들어야 하고,
	// 위치정보 외의 정보를 참조할때는 매핑 방식별로 분기해서 저장해야함. 만약 매핑 방식이 eByPolygonVertex(꼭짓점 기준)인 경우 폴리곤의 꼭짓점을 순회하는 순서
	// 그대로 참조하면 됨, 그래서 VertexId를 꼭짓점 순회하는 순서대로 증가시키면서 매핑할 것임.
	int VertexId = 0;

	// 위의 이유로 ControlPoint를 인덱스 버퍼로 쓸 수가 없어서 Vertex마다 대응되는 Index Map을 따로 만들어서 계산할 것임.
	TMap<FSkinnedVertex, uint32> IndexMap;

	for (int PolygonIndex = 0; PolygonIndex < PolygonCount; PolygonIndex++)
	{
		// 최종적으로 사용할 머티리얼 인덱스를 구함, MaterialIndex 기본값이 0이므로 없는 경우 처리됨, 머티리얼이 하나일때 materialIndex가 1 이상이므로 처리됨.
		// 머티리얼이 여러개일때만 처리해주면 됌.

		// 머티리얼이 여러개인 경우(머티리얼이 하나 이상 있는데 materialIndex가 0이면 여러개, 하나일때는 MaterialIndex를 설정해주니까)
		// 이때는 해싱을 해줘야함
		int32 MaterialIndex = DefaultMaterialIndex;
		if (DefaultMaterialIndex == 0 && InMesh->GetElementMaterialCount() > 0)
		{
			FbxGeometryElementMaterial* Material = InMesh->GetElementMaterial(0);
			int MaterialSlot = Material->GetIndexArray().GetAt(PolygonIndex);
			MaterialIndex = MaterialSlotToIndex[MaterialSlot];
		}

		// 하나의 Polygon 내에서의 VertexIndex, PolygonSize가 다를 수 있지만 위에서 삼각화를 해줬기 때문에 무조건 3임
		for (int VertexIndex = 0; VertexIndex < InMesh->GetPolygonSize(PolygonIndex); VertexIndex++)
		{
			FSkinnedVertex SkinnedVertex{};
			// 폴리곤 인덱스와 폴리곤 내에서의 vertexIndex로 ControlPointIndex 얻어냄
			int ControlPointIndex = InMesh->GetPolygonVertex(PolygonIndex, VertexIndex);

			// ========================================
			// 정점 위치 Y-Flip
			// ========================================
			const FbxVector4& Position = FbxSceneWorld.MultT(ControlPoints[ControlPointIndex]);
			// Right-Handed → Left-Handed 변환을 위해 Y-Flip 적용
			SkinnedVertex.Position = FFbxConvert::ConvertPos(Position);

			if (ControlPointToBoneWeight.Contains(ControlPointIndex))
			{
				double TotalWeights = 0.0;


				const TArray<IndexWeight>& WeightArray = ControlPointToBoneWeight[ControlPointIndex];
				for (int BoneIndex = 0; BoneIndex < WeightArray.Num() && BoneIndex < 4; BoneIndex++)
				{
					// Total weight 구하기(정규화)
					TotalWeights += ControlPointToBoneWeight[ControlPointIndex][BoneIndex].BoneWeight;
				}
				// 5개 이상이 있어도 4개만 처리할 것임.
				for (int BoneIndex = 0; BoneIndex < WeightArray.Num() && BoneIndex < 4; BoneIndex++)
				{
					// ControlPoint에 대응하는 뼈 인덱스, 가중치를 모두 저장
					SkinnedVertex.BoneIndices[BoneIndex] = ControlPointToBoneWeight[ControlPointIndex][BoneIndex].BoneIndex;
					SkinnedVertex.BoneWeights[BoneIndex] = ControlPointToBoneWeight[ControlPointIndex][BoneIndex].BoneWeight / TotalWeights;
				}
			}


			// 함수명과 다르게 매시가 가진 버텍스 컬러 레이어 개수를 리턴함.( 0번 : Diffuse, 1번 : 블랜딩 마스크 , 기타..)
			// 엔진에서는 항상 0번만 사용하거나 Count가 0임. 그래서 하나라도 있으면 그냥 0번 쓰게 함.
			// 왜 이렇게 지어졌나? -> Fbx가 3D 모델링 관점에서 만들어졌기 때문, 모델링 툴에서는 여러 개의 컬러 레이어를 하나에 매시에 만들 수 있음.
			// 컬러 뿐만 아니라 UV Normal Tangent 모두 다 레이어로 저장하고 모두 다 0번만 쓰면 됨.
			if (InMesh->GetElementVertexColorCount() > 0)
			{
				// 왜 FbxLayerElement를 안 쓰지? -> 구버전 API
				FbxGeometryElementVertexColor* VertexColors = InMesh->GetElementVertexColor(0);
				int MappingIndex;
				// 확장성을 고려하여 switch를 씀, ControlPoint와 PolygonVertex말고 다른 모드들도 있음.
				switch (VertexColors->GetMappingMode())
				{
				case FbxGeometryElement::eByPolygon: //다른 모드 예시
				case FbxGeometryElement::eAllSame:
				case FbxGeometryElement::eNone:
				default:
					break;
					// 가장 단순한 경우, 그냥 ControlPoint(Vertex의 위치)마다 하나의 컬러값을 저장.
				case FbxGeometryElement::eByControlPoint:
					MappingIndex = ControlPointIndex;
					break;
					// 꼭짓점마다 컬러가 저장된 경우(같은 위치여도 다른 컬러 저장 가능), 위와 같지만 꼭짓점마다 할당되는 VertexId를 씀.
				case FbxGeometryElement::eByPolygonVertex:
					MappingIndex = VertexId;
					break;
				}

				// 매핑 방식에 더해서, 실제로 그 ControlPoint에서 어떻게 참조할 것인지가 다를 수 있음.(데이터 압축때문에 필요, IndexBuffer를 쓰는 것과 비슷함)
				switch (VertexColors->GetReferenceMode())
				{
					// 인덱스 자체가 데이터 배열의 인덱스인 경우(중복이 생길 수 있음)
				case FbxGeometryElement::eDirect:
				{
					// 바로 참조 가능.
					const FbxColor& Color = VertexColors->GetDirectArray().GetAt(MappingIndex);
					SkinnedVertex.Color = FVector4(Color.mRed, Color.mGreen, Color.mBlue, Color.mAlpha);
				}
				break;
				//인덱스 배열로 간접참조해야함
				case FbxGeometryElement::eIndexToDirect:
				{
					int Id = VertexColors->GetIndexArray().GetAt(MappingIndex);
					const FbxColor& Color = VertexColors->GetDirectArray().GetAt(Id);
					SkinnedVertex.Color = FVector4(Color.mRed, Color.mGreen, Color.mBlue, Color.mAlpha);
				}
				break;
				//외의 경우는 일단 배제
				default:
					break;
				}
			}

			if (InMesh->GetElementNormalCount() > 0)
			{
				FbxGeometryElementNormal* Normals = InMesh->GetElementNormal(0);

				// 각진 모서리 표현력 때문에 99퍼센트의 모델은 eByPolygonVertex를 쓴다고 함.
				// 근데 구 같이 각진 모서리가 아예 없는 경우, 부드러운 셰이딩 모델을 익스포트해서 eControlPoint로 저장될 수도 있음
				int MappingIndex;

				switch (Normals->GetMappingMode())
				{
				case FbxGeometryElement::eByControlPoint:
					MappingIndex = ControlPointIndex;
					break;
				case FbxGeometryElement::eByPolygonVertex:
					MappingIndex = VertexId;
					break;
				default:
					break;
				}

				switch (Normals->GetReferenceMode())
				{
				case FbxGeometryElement::eDirect:
				{
					const FbxVector4& Normal = Normals->GetDirectArray().GetAt(MappingIndex);
					FbxVector4 NormalWorld = FbxSceneWorldInverseTranspose.MultT(FbxVector4(Normal.mData[0], Normal.mData[1], Normal.mData[2], 0.0f));
					// 노말에 Y-Flip 적용 (Right-Handed → Left-Handed)
					SkinnedVertex.Normal = FFbxConvert::ConvertDir(NormalWorld);
				}
				break;
				case FbxGeometryElement::eIndexToDirect:
				{
					int Id = Normals->GetIndexArray().GetAt(MappingIndex);
					const FbxVector4& Normal = Normals->GetDirectArray().GetAt(Id);
					FbxVector4 NormalWorld = FbxSceneWorldInverseTranspose.MultT(FbxVector4(Normal.mData[0], Normal.mData[1], Normal.mData[2], 0.0f));
					// 노말에 Y-Flip 적용 (Right-Handed → Left-Handed)
					SkinnedVertex.Normal = FFbxConvert::ConvertDir(NormalWorld);
				}
				break;
				default:
					break;
				}
			}

			if (InMesh->GetElementTangentCount() > 0)
			{
				FbxGeometryElementTangent* Tangents = InMesh->GetElementTangent(0);

				// 왜 Color에서 계산한 Mapping Index를 안 쓰지? -> 컬러, 탄젠트, 노말, UV 모두 다 다른 매핑 방식을 사용 가능함.
				int MappingIndex;

				switch (Tangents->GetMappingMode())
				{
				case FbxGeometryElement::eByControlPoint:
					MappingIndex = ControlPointIndex;
					break;
				case FbxGeometryElement::eByPolygonVertex:
					MappingIndex = VertexId;
					break;
				default:
					break;
				}

				switch (Tangents->GetReferenceMode())
				{
				case FbxGeometryElement::eDirect:
				{
					const FbxVector4& Tangent = Tangents->GetDirectArray().GetAt(MappingIndex);
					FbxVector4 TangentWorld = FbxSceneWorld.MultT(FbxVector4(Tangent.mData[0], Tangent.mData[1], Tangent.mData[2], 0.0f));
					// 탄젠트에 Y-Flip 적용 (Right-Handed → Left-Handed), W (handedness) 보존
					FVector TangentConverted = FFbxConvert::ConvertDir(TangentWorld);
					SkinnedVertex.Tangent = FVector4(TangentConverted.X, TangentConverted.Y, TangentConverted.Z, Tangent.mData[3]);
				}
				break;
				case FbxGeometryElement::eIndexToDirect:
				{
					int Id = Tangents->GetIndexArray().GetAt(MappingIndex);
					const FbxVector4& Tangent = Tangents->GetDirectArray().GetAt(Id);
					FbxVector4 TangentWorld = FbxSceneWorld.MultT(FbxVector4(Tangent.mData[0], Tangent.mData[1], Tangent.mData[2], 0.0f));
					// 탄젠트에 Y-Flip 적용 (Right-Handed → Left-Handed), W (handedness) 보존
					FVector TangentConverted = FFbxConvert::ConvertDir(TangentWorld);
					SkinnedVertex.Tangent = FVector4(TangentConverted.X, TangentConverted.Y, TangentConverted.Z, Tangent.mData[3]);
				}
				break;
				default:
					break;
				}

				// 유니폼 스케일이 아니므로 그람슈미트, 노말이 필요하므로 노말 이후에 탄젠트 계산해야함
				if (!bIsUniformScale)
				{
					FVector Tangent = FVector(SkinnedVertex.Tangent.X, SkinnedVertex.Tangent.Y, SkinnedVertex.Tangent.Z);
					float Handedness = SkinnedVertex.Tangent.W;
					const FVector& Normal = SkinnedVertex.Normal;

					float TangentToNormalDir = FVector::Dot(Tangent, Normal);

					Tangent = Tangent - Normal * TangentToNormalDir;
					Tangent.Normalize();
					SkinnedVertex.Tangent = FVector4(Tangent.X, Tangent.Y, Tangent.Z, Handedness);
				}

			}

			// UV는 매핑 방식이 위와 다름(eByPolygonVertex에서 VertexId를 안 쓰고 TextureUvIndex를 씀, 참조방식도 위와 다름.)
			// 이유 : 3D 모델의 부드러운 면에 2D 텍스처 매핑을 위해 제봉선(가상의)을 만드는 경우가 생김, 그때 하나의 VertexId가 그 제봉선을 경계로
			//		  서로 다른 uv 좌표를 가져야 할 때가 생김. 그냥 VertexId를 더 나누면 안되나? => 아티스트가 싫어하고 직관적이지도 않음, 실제로
			//		  물리적으로 폴리곤이 찢어진 게 아닌데 텍스처를 입히겠다고 Vertex를 새로 만들고 폴리곤을 찢어야 함.
			//		  그래서 UV는 인덱싱을 나머지와 다르게함
			if (InMesh->GetElementUVCount() > 0)
			{
				FbxGeometryElementUV* UVs = InMesh->GetElementUV(0);

				switch (UVs->GetMappingMode())
				{
				case FbxGeometryElement::eByControlPoint:
					switch (UVs->GetReferenceMode())
					{
					case FbxGeometryElement::eDirect:
					{
						const FbxVector2& UV = UVs->GetDirectArray().GetAt(ControlPointIndex);
						SkinnedVertex.UV = FVector2D(UV.mData[0], 1 - UV.mData[1]);
					}
					break;
					case FbxGeometryElement::eIndexToDirect:
					{
						int Id = UVs->GetIndexArray().GetAt(ControlPointIndex);
						const FbxVector2& UV = UVs->GetDirectArray().GetAt(Id);
						SkinnedVertex.UV = FVector2D(UV.mData[0], 1 - UV.mData[1]);
					}
					break;
					default:
						break;
					}
					break;
				case FbxGeometryElement::eByPolygonVertex:
				{
					int TextureUvIndex = InMesh->GetTextureUVIndex(PolygonIndex, VertexIndex);
					switch (UVs->GetReferenceMode())
					{
					case FbxGeometryElement::eDirect:
					case FbxGeometryElement::eIndexToDirect:
					{
						const FbxVector2& UV = UVs->GetDirectArray().GetAt(TextureUvIndex);
						SkinnedVertex.UV = FVector2D(UV.mData[0], 1 - UV.mData[1]);
					}
					break;
					default:
						break;
					}
				}
				break;
				default:
					break;
				}
			}

			// 실제 인덱스 버퍼에서 사용할 인덱스
			uint32 IndexOfVertex;
			// 기존의 Vertex맵에 있으면 그 인덱스를 사용
			if (IndexMap.Contains(SkinnedVertex))
			{
				IndexOfVertex = IndexMap[SkinnedVertex];
			}
			else
			{
				// 없으면 Vertex 리스트에 추가하고 마지막 원소 인덱스를 사용
				MeshData.Vertices.Add(SkinnedVertex);
				IndexOfVertex = MeshData.Vertices.Num() - 1;

				// 인덱스 맵에 추가
				IndexMap.Add(SkinnedVertex, IndexOfVertex);
			}
			// 대응하는 머티리얼 인덱스 리스트에 추가
			TArray<uint32>& GroupIndexList = MaterialGroupIndexList[MaterialIndex];
			GroupIndexList.Add(IndexOfVertex);

			// 인덱스 리스트에 최종 인덱스 추가(Vertex 리스트와 대응)
			// 머티리얼 사용하면서 필요 없어짐.(머티리얼 소팅 후 한번에 복사할거임)
			//MeshData.Indices.Add(IndexOfVertex);

			// Vertex 하나 저장했고 Vertex마다 Id를 사용하므로 +1
			VertexId++;
		} // for PolygonSize
	} // for PolygonCount



	// FBX에 정점의 탄젠트 벡터가 존재하지 않을 시
	if (InMesh->GetElementTangentCount() == 0)
	{
		// 1. 계산된 탄젠트와 바이탄젠트(Bitangent)를 누적할 임시 저장소를 만듭니다.
		// MeshData.Vertices에 이미 중복 제거된 유일한 정점들이 들어있습니다.
		TArray<FVector> TempTangents(MeshData.Vertices.Num());
		TArray<FVector> TempBitangents(MeshData.Vertices.Num());

		// 2. 모든 머티리얼 그룹의 인덱스 리스트를 순회합니다.
		for (auto& Elem : MaterialGroupIndexList)
		{
			TArray<uint32>& GroupIndexList = Elem.second;

			// 인덱스 리스트를 3개씩(트라이앵글 단위로) 순회합니다.
			for (int32 i = 0; i < GroupIndexList.Num(); i += 3)
			{
				uint32 i0 = GroupIndexList[i];
				uint32 i1 = GroupIndexList[i + 1];
				uint32 i2 = GroupIndexList[i + 2];

				// 트라이앵글을 구성하는 3개의 정점 데이터를 가져옵니다.
				// 이 정점들은 MeshData.Vertices에 있는 *유일한* 정점입니다.
				const FSkinnedVertex& v0 = MeshData.Vertices[i0];
				const FSkinnedVertex& v1 = MeshData.Vertices[i1];
				const FSkinnedVertex& v2 = MeshData.Vertices[i2];

				// 위치(P)와 UV(W)를 가져옵니다.
				const FVector& P0 = v0.Position;
				const FVector& P1 = v1.Position;
				const FVector& P2 = v2.Position;

				const FVector2D& W0 = v0.UV;
				const FVector2D& W1 = v1.UV;
				const FVector2D& W2 = v2.UV;

				// 트라이앵글의 엣지(Edge)와 델타(Delta) UV를 계산합니다.
				FVector Edge1 = P1 - P0;
				FVector Edge2 = P2 - P0;
				FVector2D DeltaUV1 = W1 - W0;
				FVector2D DeltaUV2 = W2 - W0;

				// Lengyel's MikkTSpace/Schwarze Formula (분모)
				float r = 1.0f / (DeltaUV1.X * DeltaUV2.Y - DeltaUV1.Y * DeltaUV2.X);

				// r이 무한대(inf)나 NaN이 아닌지 확인 (UV가 겹치는 경우)
				if (isinf(r) || isnan(r))
				{
					r = 0.0f; // 이 트라이앵글은 계산에서 제외
				}

				// (정규화되지 않은) 탄젠트(T)와 바이탄젠트(B) 계산
				FVector T = (Edge1 * DeltaUV2.Y - Edge2 * DeltaUV1.Y) * r;
				FVector B = (Edge2 * DeltaUV1.X - Edge1 * DeltaUV2.X) * r;

				// 3개의 정점에 T와 B를 (정규화 없이) 누적합니다.
				// 이렇게 하면 동일한 정점을 공유하는 모든 트라이앵글의 T/B가 합산됩니다.
				TempTangents[i0] += T;
				TempTangents[i1] += T;
				TempTangents[i2] += T;

				TempBitangents[i0] += B;
				TempBitangents[i1] += B;
				TempBitangents[i2] += B;
			}
		}

		// 3. 모든 정점을 순회하며 누적된 T/B를 직교화(Gram-Schmidt)하고 저장합니다.
		for (int32 i = 0; i < MeshData.Vertices.Num(); ++i)
		{
			FSkinnedVertex& V = MeshData.Vertices[i]; // 실제 정점 데이터에 접근
			const FVector& N = V.Normal;
			const FVector& T_accum = TempTangents[i];
			const FVector& B_accum = TempBitangents[i];

			if (T_accum.IsZero() || N.IsZero())
			{
				// T 또는 N이 0이면 계산 불가. 유효한 기본값 설정
				FVector T_fallback = FVector(1.0f, 0.0f, 0.0f);
				if (FMath::Abs(FVector::Dot(N, T_fallback)) > 0.99f) // N이 X축과 거의 평행하면
				{
					T_fallback = FVector(0.0f, 1.0f, 0.0f); // Y축을 T로 사용
				}
				V.Tangent = FVector4(T_fallback.X, T_fallback.Y, T_fallback.Z, 1.0f);
				continue;
			}

			// Gram-Schmidt 직교화: T = T - (T dot N) * N
			// (T를 N에 투영한 성분을 T에서 빼서, N과 수직인 벡터를 만듭니다)
			FVector Tangent = (T_accum - N * (FVector::Dot(T_accum, N))).GetSafeNormal();

			// Handedness (W 컴포넌트) 계산:
			// 외적으로 구한 B(N x T)와 누적된 B(B_accum)의 방향을 비교합니다.
			float Handedness = (FVector::Dot((FVector::Cross(Tangent, N)), B_accum) > 0.0f) ? 1.0f : -1.0f;

			// 최종 탄젠트(T)와 Handedness(W)를 저장합니다.
			V.Tangent = FVector4(Tangent.X, Tangent.Y, Tangent.Z, Handedness);
		}
	}

	// ========================================
	// 조건부 인덱스 반전 (CCW → CW)
	// ========================================
	//
	// 중요: Y-Flip은 winding order를 변경하지 않음!
	// Y-Flip 이후에도 삼각형은 원래의 winding order를 유지함.
	// 하지만 Mundi 엔진은 CW (시계방향)를 Front Face로 사용 (D3D11 기본값).
	//
	// FBX 파일은 서로 다른 winding order를 가진 메시를 포함할 수 있음:
	// - CCW로 저장된 메시 (reverse 필요)
	// - CW로 저장된 메시 (이미 올바름)
	//
	// 해결책: 머티리얼 그룹별로 winding order 자동 감지
	// - ShouldReverseWindingOrder()를 사용하여 법선 방향 확인
	// - CCW로 감지된 경우만 reverse 수행
	//
	// 삼각형 인덱스 반전: [v0, v1, v2] → [v2, v1, v0]

	for (auto& Elem : MaterialGroupIndexList)
	{
		int32 MaterialIndex = Elem.first;
		TArray<uint32>& GroupIndexList = Elem.second;

		// 이 머티리얼 그룹의 winding order 자동 감지
		bool bNeedsReverse = ShouldReverseWindingOrder(MeshData.Vertices, GroupIndexList);

		if (bNeedsReverse)
		{
			// CCW 감지됨 → CW로 반전
			for (int32 i = 0; i < GroupIndexList.Num(); i += 3)
			{
				// 첫 번째와 세 번째 정점 인덱스 교환
				std::swap(GroupIndexList[i], GroupIndexList[i + 2]);
			}
		}
	}
}

/**
 * ComputeSkeletalMeshTotalMatrix
 *
 * 스켈레탈 메시의 TotalMatrix 계산
 * TotalMatrix = GlobalTransform * GeometricTransform
 */
FbxAMatrix FFbxMesh::ComputeSkeletalMeshTotalMatrix(FbxNode* MeshNode, FbxScene* Scene)
{
	// 1. GeometricTransform 추출 (Pivot, Rotation Offset, Scaling Offset)
	// 이러한 변환들은 DCC 툴 (Maya, Max, Blender)에 의해 메시 정점에 베이크됨
	FbxVector4 GeometricTranslation = MeshNode->GetGeometricTranslation(FbxNode::eSourcePivot);
	FbxVector4 GeometricRotation = MeshNode->GetGeometricRotation(FbxNode::eSourcePivot);
	FbxVector4 GeometricScaling = MeshNode->GetGeometricScaling(FbxNode::eSourcePivot);

	FbxAMatrix GeometryTransform;
	GeometryTransform.SetT(GeometricTranslation);
	GeometryTransform.SetR(GeometricRotation);
	GeometryTransform.SetS(GeometricScaling);

	// 2. GlobalTransform 가져오기 (메시 노드의 월드 공간 변환)
	FbxAMatrix GlobalTransform = Scene->GetAnimationEvaluator()
		->GetNodeGlobalTransform(MeshNode);

	// 3. TotalMatrix 계산 = GlobalTransform * GeometryTransform
	// 씬 그래프 변환과 기하학적 변환을 모두 결합함
	FbxAMatrix TotalMatrix = GlobalTransform * GeometryTransform;

	return TotalMatrix;

	// 참고: UE5는 선택적 Pivot Baking도 지원함 (bBakePivotInVertex)
	// 현재는 위의 기본 구현을 사용함.
	// pivot baking이 필요한 경우 추가:
	//   FbxVector4 RotationPivot = MeshNode->GetRotationPivot(FbxNode::eSourcePivot);
	//   FbxVector4 ScalingPivot = MeshNode->GetScalingPivot(FbxNode::eSourcePivot);
	//   ... (추가 pivot 행렬 계산)
}

#pragma warning(pop) // 경고 상태 복원
