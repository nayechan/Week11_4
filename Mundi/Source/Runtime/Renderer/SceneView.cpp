#include "pch.h"
#include "SceneView.h"
#include "CameraActor.h"
#include "FViewport.h"
#include "Frustum.h"

FSceneView::FSceneView(FMinimalViewInfo* InMinimalViewInfo, FViewport* InViewport, URenderSettings* InRenderSettings)
	: Viewport(InViewport), RenderSettings(InRenderSettings)
{
	if (!InMinimalViewInfo || !Viewport || !RenderSettings)
	{
		UE_LOG("[FSceneView::FSceneView()]: InMinimalViewInfo 또는 Viewport 또는 RenderSettings가 없습니다.");
		return;
	}

	// --- 이 로직이 FSceneRenderer::PrepareView()에서 이동해 옴 ---

	float AspectRatio = 1.0f;
	if (Viewport->GetSizeY() > 0)
	{
		AspectRatio = (float)Viewport->GetSizeX() / (float)Viewport->GetSizeY();
	}

	ViewMatrix = InMinimalViewInfo->ViewMatrix;
	ProjectionMatrix = InMinimalViewInfo->ProjectionMatrix;
	//ViewFrustum = CreateFrustumFromCamera(*InMinimalViewInfo->RectRati;
	ViewLocation = InMinimalViewInfo->ViewLocation;
	ViewRotation = InMinimalViewInfo->ViewRotation;
	NearClip = InMinimalViewInfo->NearClip;
	FarClip = InMinimalViewInfo->FarClip;
	FieldOfView = InMinimalViewInfo->FieldOfView;
	ZoomFactor = InMinimalViewInfo->ZoomFactor;
	ProjectionMode = InMinimalViewInfo->ProjectionMode;

	ViewRect.MinX = Viewport->GetStartX();
	ViewRect.MinY = Viewport->GetStartY();
	ViewRect.MaxX = ViewRect.MinX + Viewport->GetSizeX();
	ViewRect.MaxY = ViewRect.MinY + Viewport->GetSizeY();


	ViewShaderMacros = CreateViewShaderMacros();
}

FSceneView::FSceneView(UCameraComponent* InCamera, FViewport* InViewport, URenderSettings* InRenderSettings)
	: Viewport(InViewport), RenderSettings(InRenderSettings)
{
	if (!InCamera || !Viewport || !RenderSettings)
	{
		UE_LOG("[FSceneView::FSceneView()]: CameraActor 또는 Viewport 또는 RenderSettings가 없습니다.");
		return;
	}

	// --- 이 로직이 FSceneRenderer::PrepareView()에서 이동해 옴 ---

	float AspectRatio = 1.0f;
	if (Viewport->GetSizeY() > 0)
	{
		AspectRatio = (float)Viewport->GetSizeX() / (float)Viewport->GetSizeY();
	}

	ViewMatrix = InCamera->GetViewMatrix();
	ProjectionMatrix = InCamera->GetProjectionMatrix(AspectRatio, Viewport);
	//ViewFrustum = CreateFrustumFromCamera(*InCamera, AspectRatio);
	ViewLocation = InCamera->GetWorldLocation();
	ViewRotation = InCamera->GetWorldRotation();
	NearClip = InCamera->GetNearClip();
	FarClip = InCamera->GetFarClip();
	FieldOfView = InCamera->GetFOV();
	ZoomFactor = InCamera->GetZoomFactor();

	ViewRect.MinX = Viewport->GetStartX();
	ViewRect.MinY = Viewport->GetStartY();
	ViewRect.MaxX = ViewRect.MinX + Viewport->GetSizeX();
	ViewRect.MaxY = ViewRect.MinY + Viewport->GetSizeY();

	ProjectionMode = InCamera->GetProjectionMode();

	ViewShaderMacros = CreateViewShaderMacros();
}

TArray<FShaderMacro> FSceneView::CreateViewShaderMacros()
{
	TArray<FShaderMacro> ShaderMacros;

	switch (RenderSettings->GetViewMode())
	{
	case EViewMode::VMI_Lit_Phong:
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_PHONG", "1" });
		break;
	case EViewMode::VMI_Lit_Gouraud:
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_GOURAUD", "1" });
		break;
	case EViewMode::VMI_Lit_Lambert:
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_LAMBERT", "1" });
		break;
	case EViewMode::VMI_Unlit:
		// 매크로 없음 (Unlit)
		break;
	case EViewMode::VMI_WorldNormal:
		ShaderMacros.push_back(FShaderMacro{ "VIEWMODE_WORLD_NORMAL", "1" });
		break;
	default:
		// 셰이더를 강제하지 않는 모드는 여기서 처리 가능
		break;
	}

	// 그림자 AA 설정
	if (RenderSettings->IsShowFlagEnabled(EEngineShowFlags::SF_ShadowAntiAliasing))
	{
		EShadowAATechnique Technique = RenderSettings->GetShadowAATechnique();
		if (Technique == EShadowAATechnique::PCF)
		{
			ShaderMacros.Add(FShaderMacro("SHADOW_AA_TECHNIQUE", "1")); // 1 = PCF
		}
		else if (Technique == EShadowAATechnique::VSM)
		{
			ShaderMacros.Add(FShaderMacro("SHADOW_AA_TECHNIQUE", "2")); // 2 = VSM
		}
	}
	else
	{
		ShaderMacros.Add(FShaderMacro("SHADOW_AA_TECHNIQUE", "0")); // 0 = Hard Shadow (AA 끔)
	}

	return ShaderMacros;
}
