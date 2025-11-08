#include "pch.h"
#include "ObjectFactory.h"
#include "FBXLoader.h"
#include "fbxsdk/fileio/fbxiosettings.h"

IMPLEMENT_CLASS(UFbxLoader)

UFbxLoader::UFbxLoader()
{
	// 메모리 관리, FbxManager 소멸시 Fbx 관련 오브젝트 모두 소멸
	SdkManager = FbxManager::Create();

}


UFbxLoader::~UFbxLoader()
{
	SdkManager->Destroy();
}
UFbxLoader& UFbxLoader::GetInstance()
{
	static UFbxLoader* FBXLoader = nullptr;
	if (!FBXLoader)
	{
		FBXLoader = ObjectFactory::NewObject<UFbxLoader>();
	}
	return *FBXLoader;
}

FSkeletalMeshData UFbxLoader::LoadFbxMesh(const FString& FilePath)
{
	FSkeletalMeshData MeshData;
	// 임포트 옵션 세팅 ( 에니메이션 임포트 여부, 머티리얼 임포트 여부 등등 IO에 대한 세팅 )
	// IOSROOT = IOSetting 객체의 기본 이름( Fbx매니저가 이름으로 관리하고 디버깅 시 매니저 내부의 객체를 이름으로 식별 가능)
	// 하지만 매니저 자체의 기본 설정이 있기 때문에 아직 안씀
	//FbxIOSettings* IoSetting = FbxIOSettings::Create(SdkManager, IOSROOT);
	//SdkManager->SetIOSettings(IoSetting);

	// 로드할때마다 임포터 생성
	FbxImporter* Importer = FbxImporter::Create(SdkManager, "");

	// 원하는 IO 세팅과 Fbx파일로 Importer initialize
	if (!Importer->Initialize(FilePath.c_str(), -1, SdkManager->GetIOSettings()))
	{
		UE_LOG("Call to FbxImporter::Initialize() Falied\n");
		UE_LOG("[FbxImporter::Initialize()] Error Reports: %s\n\n", Importer->GetStatus().GetErrorString());
		return FSkeletalMeshData();
	}

	// 임포터가 씬에 데이터를 채워줄 것임. 이름은 IoSetting과 마찬가지로 매니저가 이름으로 관리하고 Export될 때 표시할 씬 이름.
	FbxScene* Scene = FbxScene::Create(SdkManager, "My Scene");

	// 임포트해서 FBX를 씬에 넣어줌
	Importer->Import(Scene);

	// 임포트 후 제거
	Importer->Destroy();

	// 매시의 폴리곤이 삼각형이 아닐 수가 있음, Converter가 모두 삼각화해줌.
	FbxGeometryConverter IGeometryConverter(SdkManager);
	if (IGeometryConverter.Triangulate(Scene, true))
	{
		UE_LOG("Fbx 씬 삼각화 완료\n");
	}
	else
	{
		UE_LOG("Fbx 씬 삼각화가 실패했습니다, 매시가 깨질 수 있습니다\n");
	}

	// FBX파일에 씬은 하나만 존재하고 씬에 매쉬, 라이트, 카메라 등등의 element들이 트리 구조로 저장되어 있음.
	// 씬 Export 시에 루트 노드 말고 Child 노드만 저장됨. 노드 하나가 여러 Element를 저장할 수 있고 Element는 FbxNodeAttribute 클래스로 정의되어 있음.
	// 루트 노드 얻어옴
	FbxNode* RootNode = Scene->GetRootNode();

	// 뼈의 인덱스를 부여, 기본적으로 FBX는 정점이 아니라 뼈 중심으로 데이터가 저장되어 있음(뼈가 몇번 ControlPoint에 영향을 주는지)
	// 정점이 몇번 뼈의 영향을 받는지 표현하려면 직접 뼈 인덱스를 만들어야함.
	TMap<FbxNode*, int32> BoneToIndex;

	if (RootNode)
	{
		// 2번의 패스로 나눠서 처음엔 뼈의 인덱스를 결정하고 2번째 패스에서 뼈가 영향을 미치는 정점들을 구하고 정점마다 뼈 인덱스를 할당해 줄 것임(동시에 TPose 역행렬도 구함)
		for (int Index = 0; Index < RootNode->GetChildCount(); Index++)
		{
			LoadSkeletonFromNode(RootNode->GetChild(Index), MeshData, -1, BoneToIndex);
		}
		for (int Index = 0; Index < RootNode->GetChildCount(); Index++)
		{
			LoadMeshFromNode(RootNode->GetChild(Index), MeshData, BoneToIndex);
		}
	}

	return MeshData;
}


void UFbxLoader::LoadMeshFromNode(FbxNode* InNode, FSkeletalMeshData& MeshData, TMap<FbxNode*, int32>& BoneToIndex)
{

	// 부모노드로부터 상대좌표 리턴
	/*FbxDouble3 Translation = InNode->LclTranslation.Get();
	FbxDouble3 Rotation = InNode->LclRotation.Get();
	FbxDouble3 Scaling  = InNode->LclScaling.Get();*/

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
			LoadMesh((FbxMesh*)Attribute, MeshData, BoneToIndex);
		}
	}

	for (int Index = 0; Index < InNode->GetChildCount(); Index++)
	{
		LoadMeshFromNode(InNode->GetChild(Index), MeshData, BoneToIndex);
	}
}

// Skeleton은 계층구조까지 표현해야하므로 깊이 우선 탐색, ParentNodeIndex 명시.
void UFbxLoader::LoadSkeletonFromNode(FbxNode* InNode, FSkeletalMeshData& MeshData, int32 ParentNodeIndex, TMap<FbxNode*, int32>& BoneToIndex)
{
	for (int Index = 0; Index < InNode->GetNodeAttributeCount(); Index++)
	{
		int32 BoneIndex = ParentNodeIndex;
		FbxNodeAttribute* Attribute = InNode->GetNodeAttributeByIndex(Index);
		if (!Attribute)
		{
			continue;
		}

		if (Attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
		{
			FBone BoneInfo;

			BoneInfo.Name = InNode->GetName();
			
			BoneInfo.ParentIndex = ParentNodeIndex;

			// 뼈 리스트에 추가
			MeshData.Skeleton.Bones.Add(BoneInfo);
			
			// 뼈 인덱스 우리가 정해줌(방금 추가한 마지막 원소)
			BoneIndex = MeshData.Skeleton.Bones.Num() - 1;
			
			// 뼈 이름으로 인덱스 서치 가능하게 함.
			MeshData.Skeleton.BoneNameToIndex.Add(BoneInfo.Name, BoneIndex);

			// 매시 로드할때 써야되서 맵에 인덱스 저장
			BoneToIndex.Add(InNode, BoneIndex);
		}

		for (int Index = 0; Index < InNode->GetChildCount(); Index++)
		{
			// 깊이 우선 탐색 부모 인덱스 설정(InNOde가 eSkeleton이 아니면 기존 부모 인덱스가 넘어감(BoneIndex = ParentNodeIndex)
			LoadSkeletonFromNode(InNode->GetChild(Index), MeshData, BoneIndex, BoneToIndex);
		}
	}
}

// 예시 코드
void UFbxLoader::LoadMeshFromAttribute(FbxNodeAttribute* InAttribute, FSkeletalMeshData& MeshData)
{

	/*if (!InAttribute)
	{
		return;
	}*/
	//FbxString TypeName = GetAttributeTypeName(InAttribute);
	// 타입과 별개로 Element 자체의 이름도 있음
	//FbxString AttributeName = InAttribute->GetName();

	// Buffer함수로 FbxString->char* 변환
	//UE_LOG("<Attribute Type = %s, Name = %s\n", TypeName.Buffer(), AttributeName.Buffer());
}

void UFbxLoader::LoadMesh(FbxMesh* InMesh, FSkeletalMeshData& MeshData, TMap<FbxNode*, int32>& BoneToIndex)
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

	// Deformer: 매시의 모양을 변형시키는 모든 기능, ex) skin, blendShape(모프 타겟, 두 표정 미리 만들고 블랜딩해서 서서히 변화시킴)
	// 99.9퍼센트는 스킨이 하나만 있고 완전 복잡한 얼굴 표정을 표현하기 위해서 2개 이상을 쓰기도 하는데 0번만 쓰도록 해도 문제 없음(AAA급 게임에서 2개 이상을 처리함)
	// 2개 이상의 스킨이 들어가면 뼈 인덱스가 16개까지도 늘어남. 
	if (InMesh->GetDeformerCount(FbxDeformer::eSkin) > 0)
	{
		// 클러스터: 뼈라고 봐도 됨(뼈와 그 뼈가 영향을 주는 정점과의 관계)
		for (int Index = 0; Index < ((FbxSkin*)InMesh->GetDeformer(0, FbxDeformer::eSkin))->GetClusterCount(); Index++)
		{
			FbxCluster* Cluster = ((FbxSkin*)InMesh->GetDeformer(0, FbxDeformer::eSkin))->GetCluster(Index);

			int IndexCount = Cluster->GetControlPointIndicesCount();
			// 클러스터가 영향을 주는 ControlPointIndex를 구함.
			int* Indices = Cluster->GetControlPointIndices();
			double* Weights = Cluster->GetControlPointWeights();

			for (int ControlPointIndex = 0; ControlPointIndex < IndexCount; ControlPointIndex++)
			{
				// GetLink -> 아까 저장한 노드 To Index맵의 노드 (Cluster에 대응되는 뼈 인덱스를 ControlPointIndex에 대응시키는 과정)
				// ControlPointIndex = 클러스터가 저장하는 ControlPointIndex 배열에 대한 Index
				TArray<IndexWeight>& IndexWeightArray = ControlPointToBoneWeight[Indices[ControlPointIndex]];
				IndexWeightArray.Add(IndexWeight(BoneToIndex[Cluster->GetLink()], Weights[ControlPointIndex]));
			}
		}
	}


	// 로드는 TriangleList를 가정하고 할 것임. 
	// TriangleStrip은 한번 만들면 편집이 사실상 불가능함, FBX같은 호환성이 중요한 모델링 포멧이 유연성 부족한 모델을 저장할 이유도 없고
	// 엔진 최적화 측면에서도 GPU의 Vertex Cache가 Strip과 비슷한 성능을 내면서도 직관적이고 유연해서 잘 쓰지도 않기 때문에 그냥 안 씀.
	int PolygonCount = InMesh->GetPolygonCount();
	
	// ControlPoints는 정점의 위치 정보를 배열로 저장함, Vertex마다 ControlIndex로 참조함.
	FbxVector4* ControlPoints = InMesh->GetControlPoints();

	
	// Vertex 위치가 같아도 서로 다른 Normal, Tangent, UV좌표를 가질 수 있음, FBX는 하나의 인덱스 배열에서 이들을 서로 다른 인덱스로 관리하길 강제하지 않고 
	// Vertex 위치는 ControlPoint로 관리하고 그 외의 정보들은 선택적으로 분리해서 관리하도록 함. 그래서 ControlPoint를 Index로 쓸 수도 없어서 따로 만들어야 하고, 
	// 위치정보 외의 정보를 참조할때는 매핑 방식별로 분기해서 저장해야함. 만약 매핑 방식이 eByPolygonVertex(꼭짓점 기준)인 경우 폴리곤의 꼭짓점을 순회하는 순서
	// 그대로 참조하면 됨, 그래서 VertexId를 꼭짓점 순회하는 순서대로 증가시키면서 매핑할 것임.
	int VertexId = 0;

	// 위의 이유로 ControlPoint를 인덱스 버퍼로 쓸 수가 없어서 Vertex마다 대응되는 Index Map을 따로 만들어서 계산할 것임.
	TMap<FSkinnedVertex, uint32> IndexMap;
	
	FSkinnedVertex SkinnedVertex;
	for (int PolygonIndex = 0; PolygonIndex < PolygonCount; PolygonIndex++)
	{
		// 하나의 Polygon 내에서의 VertexIndex, PolygonSize가 다를 수 있지만 위에서 삼각화를 해줬기 때문에 무조건 3임
		for (int VertexIndex = 0; VertexIndex < InMesh->GetPolygonSize(PolygonIndex); VertexIndex++)
		{
			// 폴리곤 인덱스와 폴리곤 내에서의 vertexIndex로 ControlPointIndex 얻어냄
			int ControlPointIndex = InMesh->GetPolygonVertex(PolygonIndex, VertexIndex);

			const FbxVector4& Position = ControlPoints[ControlPointIndex];
			SkinnedVertex.Position = FVector(Position.mData[0], Position.mData[1], Position.mData[2]);

			
			if (ControlPointToBoneWeight.Contains(ControlPointIndex))
			{
				const TArray<IndexWeight>& WeightArray = ControlPointToBoneWeight[ControlPointIndex];
				// 5개 이상이 있어도 4개만 처리할 것임.
				for (int BoneIndex = 0; BoneIndex < WeightArray.Num() && BoneIndex < 4; BoneIndex++)
				{
					// ControlPoint에 대응하는 뼈 인덱스, 가중치를 모두 저장
					SkinnedVertex.BoneIndices[BoneIndex] = ControlPointToBoneWeight[ControlPointIndex][BoneIndex].BoneIndex;
					SkinnedVertex.BoneWeights[BoneIndex] = ControlPointToBoneWeight[ControlPointIndex][BoneIndex].BoneWeight;
				}
			}


			// 함수명과 다르게 매시가 가진 버텍스 컬러 레이어 개수를 리턴함.( 0번 : Diffuse, 1번 : 블랜딩 마스크 , 기타..)
			// 엔진에서는 항상 0번만 사용하거나 Count가 0임. 그래서 하나라도 있으면 그냥 0번 쓰게 함.
			// 왜 이렇게 지어졌나? -> FBX가 3D 모델링 관점에서 만들어졌기 때문, 모델링 툴에서는 여러 개의 컬러 레이어를 하나에 매시에 만들 수 있음.
			// 컬러 뿐만 아니라 UV Normal Tangent 모두 다 레이어로 저장하고 모두 다 0번만 쓰면 됨.
			if (InMesh->GetElementVertexColorCount() > 0)
			{
				// 왜 FbxLayerElement를 안 쓰지? -> 구버전 API
				FbxGeometryElementVertexColor* VertexColors = InMesh->GetElementVertexColor(0);
				int MappingIndex;
				// 확장성을 고려하여 switch를 씀, ControlPoint와 PolygonVertex말고 다른 모드들도 있음.
				switch(VertexColors->GetMappingMode())
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
							SkinnedVertex.Tangent = FVector4(Tangent.mData[0], Tangent.mData[1], Tangent.mData[2], Tangent.mData[3]);
						}
						break;
					case FbxGeometryElement::eIndexToDirect:
						{
							int Id = Tangents->GetIndexArray().GetAt(MappingIndex);
							const FbxVector4& Tangent = Tangents->GetDirectArray().GetAt(Id);
							SkinnedVertex.Tangent = FVector4(Tangent.mData[0], Tangent.mData[1], Tangent.mData[2], Tangent.mData[3]);
						}
						break;
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
						SkinnedVertex.Normal = FVector(Normal.mData[0], Normal.mData[1], Normal.mData[2]);
					}
					break;
				case FbxGeometryElement::eIndexToDirect:
					{
						int Id = Normals->GetIndexArray().GetAt(MappingIndex);
						const FbxVector4& Normal = Normals->GetDirectArray().GetAt(MappingIndex);
						SkinnedVertex.Normal = FVector(Normal.mData[0], Normal.mData[1], Normal.mData[2]);
					}
					break;
				default:
					break;
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
							SkinnedVertex.UV = FVector2D(UV.mData[0], UV.mData[1]);
						}
						break;
					case FbxGeometryElement::eIndexToDirect:
						{
							int Id = UVs->GetIndexArray().GetAt(ControlPointIndex);
							const FbxVector2& UV = UVs->GetDirectArray().GetAt(ControlPointIndex);
							SkinnedVertex.UV = FVector2D(UV.mData[0], UV.mData[1]);
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
									SkinnedVertex.UV = FVector2D(UV.mData[0], UV.mData[1]);
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
			// 인덱스 리스트에 최종 인덱스 추가(Vertex 리스트와 대응)
			MeshData.Indices.Add(IndexOfVertex);

			

			// Vertex 하나 저장했고 Vertex마다 Id를 사용하므로 +1
			VertexId++;
		} // for PolygonSize
	} // for PolygonCount
}

FbxString UFbxLoader::GetAttributeTypeName(FbxNodeAttribute* InAttribute)
{
	// Attribute타입에 대한 자료형, 이것으로 Skeleton만 빼낼 수 있을 듯
	FbxNodeAttribute::EType Type = InAttribute->GetAttributeType();
	switch (Type) {
	case FbxNodeAttribute::eUnknown: return "unidentified";
	case FbxNodeAttribute::eNull: return "null";
	case FbxNodeAttribute::eMarker: return "marker";
	case FbxNodeAttribute::eSkeleton: return "skeleton";
	case FbxNodeAttribute::eMesh: return "mesh";
	case FbxNodeAttribute::eNurbs: return "nurbs";
	case FbxNodeAttribute::ePatch: return "patch";
	case FbxNodeAttribute::eCamera: return "camera";
	case FbxNodeAttribute::eCameraStereo: return "stereo";
	case FbxNodeAttribute::eCameraSwitcher: return "camera switcher";
	case FbxNodeAttribute::eLight: return "light";
	case FbxNodeAttribute::eOpticalReference: return "optical reference";
	case FbxNodeAttribute::eOpticalMarker: return "marker";
	case FbxNodeAttribute::eNurbsCurve: return "nurbs curve";
	case FbxNodeAttribute::eTrimNurbsSurface: return "trim nurbs surface";
	case FbxNodeAttribute::eBoundary: return "boundary";
	case FbxNodeAttribute::eNurbsSurface: return "nurbs surface";
	case FbxNodeAttribute::eShape: return "shape";
	case FbxNodeAttribute::eLODGroup: return "lodgroup";
	case FbxNodeAttribute::eSubDiv: return "subdiv";
	default: return "unknown";
	}
}

