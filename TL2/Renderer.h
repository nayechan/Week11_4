#pragma once
#include "RHIDevice.h"
#include "LineDynamicMesh.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMeshComponent;
class URHIDevice;
class UShader;
class UStaticMesh;
struct FMaterialSlot;

class URenderer
{
public:
    URenderer(URHIDevice* InDevice);

    ~URenderer();

public:
	void BeginFrame();

    void PrepareShader(FShader& InShader);

    void PrepareShader(UShader* InShader);

    void OMSetBlendState(bool bIsChecked);

    void RSSetState(EViewModeIndex ViewModeIndex);

    void UpdateConstantBuffer(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix);

    void UpdateHighLightConstantBuffer(const uint32 InPicked, const FVector& InColor, const uint32 X, const uint32 Y, const uint32 Z, const uint32 Gizmo);

    void UpdateBillboardConstantBuffers(const FVector& pos, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix, const FVector& CameraRight, const FVector& CameraUp);

    void UpdatePixelConstantBuffers(const FObjMaterialInfo& InMaterialInfo, bool bHasMaterial, bool bHasTexture);

    void UpdateColorBuffer(const FVector4& Color);

    void DrawIndexedPrimitiveComponent(UStaticMesh* InMesh, D3D11_PRIMITIVE_TOPOLOGY InTopology, const TArray<FMaterialSlot>& InComponentMaterialSlots);

    void UpdateUVScroll(const FVector2D& Speed, float TimeSec);

    void DrawIndexedPrimitiveComponent(UTextRenderComponent* Comp, D3D11_PRIMITIVE_TOPOLOGY InTopology);

    void SetViewModeType(EViewModeIndex ViewModeIndex);
    // Batch Line Rendering System
    void BeginLineBatch();
    void AddLine(const FVector& Start, const FVector& End, const FVector4& Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    void AddLines(const TArray<FVector>& StartPoints, const TArray<FVector>& EndPoints, const TArray<FVector4>& Colors);
    void EndLineBatch(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix);
    void ClearLineBatch();

	void EndFrame();

    void OMSetDepthStencilState(EComparisonFunc Func);

    URHIDevice* GetRHIDevice() { return RHIDevice; }
private:
	URHIDevice* RHIDevice;

    // Batch Line Rendering System using UDynamicMesh for efficiency
    ULineDynamicMesh* DynamicLineMesh = nullptr;
    FMeshData* LineBatchData = nullptr;
    UShader* LineShader = nullptr;
    bool bLineBatchActive = false;
    static const uint32 MAX_LINES = 200000;  // Maximum lines per batch (safety headroom)

    void InitializeLineBatch();

};

