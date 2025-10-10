#include "pch.h"
#include "DecalComponent.h"
#include "OBB.h"

void UDecalComponent::Serialize(bool bIsLoading, FDecalData& InOut)
{
}

void UDecalComponent::DuplicateSubObjects()
{
    
}

void UDecalComponent::RenderAffectedPrimitives(URenderer* Renderer, UPrimitiveComponent* Target, const FMatrix& View, const FMatrix& Proj)
{
    // TODO: 실제 렌더 부분
    //Renderer->GetRHIDevice()->Update
}

void UDecalComponent::RenderDebugVolume(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) const
{
	// 로컬 단위 큐브의 정점과 선 정보 정의 (위와 동일)
	const FVector4 LocalVertices[8] = {
		FVector4(-0.5f, -0.5f, -0.5f, 1.0f), FVector4(0.5f, -0.5f, -0.5f, 1.0f),
		FVector4(0.5f, 0.5f, -0.5f, 1.0f), FVector4(-0.5f, 0.5f, -0.5f, 1.0f),
		FVector4(-0.5f, -0.5f, 0.5f, 1.0f), FVector4(0.5f, -0.5f, 0.5f, 1.0f),
		FVector4(0.5f, 0.5f, 0.5f, 1.0f), FVector4(-0.5f, 0.5f, 0.5f, 1.0f)
	};

	const int Edges[12][2] = {
		{0, 1}, {1, 2}, {2, 3}, {3, 0}, // 하단
		{4, 5}, {5, 6}, {6, 7}, {7, 4}, // 상단
		{0, 4}, {1, 5}, {2, 6}, {3, 7}  // 기둥
	};

	// 컴포넌트의 월드 변환 행렬
	const FMatrix WorldMatrix = GetWorldMatrix();

	// 라인 색상
	const FVector4 BoxColor(1.0f, 1.0f, 0.0f, 1.0f); // 노란색

	// AddLines 함수에 전달할 데이터 배열들을 준비
	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;

	// 12개의 선 데이터를 배열에 채워 넣습니다.
	for (int i = 0; i < 12; ++i)
	{
		// 월드 좌표로 변환
		const FVector4 WorldStart = (LocalVertices[Edges[i][0]]) * WorldMatrix;
		const FVector4 WorldEnd = (LocalVertices[Edges[i][1]]) * WorldMatrix;

		StartPoints.Add(FVector(WorldStart.X, WorldStart.Y, WorldStart.Z));
		EndPoints.Add(FVector(WorldEnd.X, WorldEnd.Y, WorldEnd.Z));
		Colors.Add(BoxColor);
	}

	// 모든 데이터를 준비한 뒤, 단 한 번의 호출로 렌더러에 전달합니다.
	Renderer->AddLines(StartPoints, EndPoints, Colors);
}

void UDecalComponent::SetDecalTexture(UTexture* InTexture)
{
}

void UDecalComponent::SetDecalTexture(const FString& TexturePath)
{
}

FAABB UDecalComponent::GetWorldAABB() const
{
    return FAABB();
}

FOBB UDecalComponent::GetOBB() const
{
    return FOBB();
}

FMatrix UDecalComponent::GetDecalProjectionMatrix() const
{
    return FMatrix();
}
