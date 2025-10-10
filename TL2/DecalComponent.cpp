#include "pch.h"
#include <cmath>
#include "DecalComponent.h"
#include "OBB.h"
#include "StaticMeshComponent.h"

UDecalComponent::UDecalComponent()
{
	UResourceManager::GetInstance().Load<UMaterial>("DecalVS.hlsl", EVertexLayoutType::PositionColorTexturNormal);
	UResourceManager::GetInstance().Load<UMaterial>("DecalPS.hlsl", EVertexLayoutType::PositionColorTexturNormal);

	DecalTexture = UResourceManager::GetInstance().Load<UTexture>("Data/cube_texture.dds");
}

void UDecalComponent::Serialize(bool bIsLoading, FDecalData& InOut)
{
}

void UDecalComponent::DuplicateSubObjects()
{
    
}


void UDecalComponent::RenderAffectedPrimitives(URenderer* Renderer, UPrimitiveComponent* Target, const FMatrix& View, const FMatrix& Proj)
{
	UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Target);
	if (!SMC)
	{
		return;
	}


	D3D11RHI* RHIDevice = Renderer->GetRHIDevice();

	// Constatn Buffer 업데이트
	RHIDevice->UpdateConstantBuffers(Target->GetWorldMatrix(), View, Proj);

	const FMatrix DecalMatrix = GetDecalProjectionMatrix();
	RHIDevice->UpdateDecalBuffer(DecalMatrix);

	// Shader 설정
	UShader* VS = UResourceManager::GetInstance().Load<UShader>("DecalVS.hlsl");
	UShader* PS = UResourceManager::GetInstance().Load<UShader>("DecalPS.hlsl");

	RHIDevice->GetDeviceContext()->VSSetShader(VS->GetVertexShader(), nullptr, 0);
	RHIDevice->GetDeviceContext()->PSSetShader(PS->GetPixelShader(), nullptr, 0);
	RHIDevice->GetDeviceContext()->IASetInputLayout(VS->GetInputLayout());

	// VertexBuffer, IndexBuffer 설정
	UStaticMesh* Mesh = SMC->GetStaticMesh();

	ID3D11Buffer* VertexBuffer = Mesh->GetVertexBuffer();
	ID3D11Buffer* IndexBuffer = Mesh->GetIndexBuffer();
	uint32 VertexCount = Mesh->GetVertexCount();
	uint32 IndexCount = Mesh->GetIndexCount();
	UINT Stride = sizeof(FVertexDynamic);
	UINT Offset = 0;

	RHIDevice->GetDeviceContext()->IASetVertexBuffers(
		0, 1, &VertexBuffer, &Stride, &Offset
	);
	RHIDevice->GetDeviceContext()->IASetIndexBuffer(
		IndexBuffer, DXGI_FORMAT_R32_UINT, 0
	);

	RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	RHIDevice->PSSetDefaultSampler(0);

	if (DecalTexture)
	{
		ID3D11ShaderResourceView* SRV = DecalTexture->GetShaderResourceView();
		RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &SRV);
	}
	else
	{
		UE_LOG("Decal Texture is nullptr!");
	}

	// DrawCall
	RHIDevice->GetDeviceContext()->DrawIndexed(IndexCount, 0, 0);
	
}

void UDecalComponent::RenderDebugVolume(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) const
{
	// 라인 색상
	const FVector4 BoxColor(1.0f, 1.0f, 0.0f, 1.0f); // 노란색

	// AddLines 함수에 전달할 데이터 배열들을 준비
	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;
	
	TArray<FVector> Coners = GetOBB().GetCorners();

	const int Edges[12][2] = {
		{6, 4}, {7, 5}, {6, 7}, {4, 5}, // 앞면
		{4, 0}, {5, 1}, {6, 2}, {7, 3}, // 옆면
		{0, 2}, {1, 3}, {0, 1}, {2, 3}  // 뒷면
	};

	// 12개의 선 데이터를 배열에 채워 넣습니다.
	for (int i = 0; i < 12; ++i)
	{
		// 월드 좌표로 변환
		const FVector WorldStart = Coners[Edges[i][0]];
		const FVector WorldEnd = Coners[Edges[i][1]];

		StartPoints.Add(WorldStart);
		EndPoints.Add(WorldEnd);
		Colors.Add(BoxColor);
	}

	// 모든 데이터를 준비한 뒤, 단 한 번의 호출로 렌더러에 전달합니다.
	Renderer->AddLines(StartPoints, EndPoints, Colors);
}

void UDecalComponent::SetDecalTexture(UTexture* InTexture)
{
	DecalTexture = InTexture;
}

void UDecalComponent::SetDecalTexture(const FString& TexturePath)
{
	DecalTexture = UResourceManager::GetInstance().Load<UTexture>(TexturePath);
	//UMaterial* const Material = UResourceManager::GetInstance().Load<UMaterial>(TexturePath);
	//const FObjMaterialInfo& MaterialInfo = Material->GetMaterialInfo();
	//if (!MaterialInfo.DiffuseTextureFileName.empty())
	//{
	//	// 반환 여기서 로드 
	//	if (FTextureData* TextureData = UResourceManager::GetInstance().CreateOrGetTextureData(WTextureFileName))
	//	{
	//		if (TextureData->TextureSRV)
	//		{
	//			srv = TextureData->TextureSRV;
	//			bHasTexture = true;
	//		}
	//	}
	//}
}

FAABB UDecalComponent::GetWorldAABB() const
{
    // Step 1: Build the decal's oriented box so we can inspect its world-space corners.
    const FOBB DecalOBB = GetOBB();

    // Step 2: Initialize min/max accumulators that will grow to the final axis-aligned bounds.
    FVector MinBounds(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector MaxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    // Step 3: Evaluate all 8 OBB corners in world-space.
    const FVector& Center = DecalOBB.Center;
    const FVector& HalfExtent = DecalOBB.HalfExtent;
    const FVector (&Axes)[3] = DecalOBB.Axes;

    for (int8 sx = -1; sx <= 1; sx += 2)
    {
        for (int8 sy = -1; sy <= 1; sy += 2)
        {
            for (int8 sz = -1; sz <= 1; sz += 2)
            {
                const FVector Corner = Center
                    + Axes[0] * (HalfExtent.X * static_cast<float>(sx))
                    + Axes[1] * (HalfExtent.Y * static_cast<float>(sy))
                    + Axes[2] * (HalfExtent.Z * static_cast<float>(sz));

                MinBounds.X = std::min(MinBounds.X, Corner.X);
                MinBounds.Y = std::min(MinBounds.Y, Corner.Y);
                MinBounds.Z = std::min(MinBounds.Z, Corner.Z);

                MaxBounds.X = std::max(MaxBounds.X, Corner.X);
                MaxBounds.Y = std::max(MaxBounds.Y, Corner.Y);
                MaxBounds.Z = std::max(MaxBounds.Z, Corner.Z);
            }
        }
    }

    // Step 4: Package the accumulated extremes into a world-space AABB.
    return FAABB(MinBounds, MaxBounds);
}

FOBB UDecalComponent::GetOBB() const
{
    const FVector Center = GetWorldLocation();
    const FVector HalfExtent = GetWorldScale() / 2.0f;

    const FQuat Quat = GetWorldRotation();

    FVector Axes[3];
    Axes[0] = Quat.GetForwardVector();
    Axes[1] = Quat.GetRightVector();
    Axes[2] = Quat.GetUpVector();

    FOBB Obb(Center, HalfExtent, Axes);

    return Obb;
}

FMatrix UDecalComponent::GetDecalProjectionMatrix() const
{
    const FOBB Obb = GetOBB();

	const FMatrix DecalWorld = FMatrix::FromTRS(GetWorldLocation(), GetWorldRotation(), {1.0f, 1.0f, 1.0f});
	const FMatrix DecalView = DecalWorld.InverseAffine();

	const FVector Scale = GetWorldScale();
	const FMatrix DecalProj = FMatrix::OrthoLH_XForward(Scale.Y * 2.0f, Scale.Z, -Obb.HalfExtent.X / 2.0f, Obb.HalfExtent.X / 2.0f);
	//const FMatrix DecalProj = FMatrix::OrthoLH(Scale.Y, Scale.Z, -Obb.HalfExtent.X, Obb.HalfExtent.X);

	FMatrix DecalViewProj = DecalView * DecalProj;

    return DecalViewProj;
}




