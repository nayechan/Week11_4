#pragma once
#include "CameraModifierBase.h"
#include "PostProcessing/PostProcessing.h"

class UCamMod_Vignette : public UCameraModifierBase
{

public:
    DECLARE_CLASS(UCamMod_Vignette, UCameraModifierBase)

    UCamMod_Vignette() = default;
    virtual ~UCamMod_Vignette() = default;
    FLinearColor Color = FLinearColor::Zero();
    float Radius;    // 반지름
    float Softness;  // 안쪽의 부드러운 정도
    float Intensity; // 색깔 세기
    float Roundness; // 1은 마름모, 2는 원, 큰 값으면 사각형
    virtual void ApplyToView(float DeltaTime, FMinimalViewInfo* ViewInfo) override {}; 

    virtual void CollectPostProcess(TArray<FPostProcessModifier>& Out) override
    {
        if (!bEnabled) return;

        FPostProcessModifier M;
        M.Type = EPostProcessEffectType::Vignette;
        M.Priority = Priority;
        M.bEnabled = true;
        M.Weight = Weight;
        M.SourceObject = this;

        M.Payload.Color = Color;

        M.Payload.Params0 = FVector4(Radius, Softness, Intensity, Roundness);

        Out.Add(M);
    }
};

