#pragma once
#include "CameraModifierBase.h"
#include "PostProcessing/PostProcessing.h"

class UCamMod_LetterBox : public UCameraModifierBase
{
public:
    DECLARE_CLASS(UCamMod_LetterBox, UCameraModifierBase)
    UCamMod_LetterBox() = default;
    virtual ~UCamMod_LetterBox() = default;

    // 설정값
    FLinearColor BoxColor = FLinearColor::Zero();
    float AspectRatio = 2.39f;  // 목표 콘텐츠 AR (예: 2.39). 0이면 무시
    float HeightBarSize = 0.0f;   // 0이면 AR로 자동 계산, >0이면 화면 높이 비율(0~0.5)

private:
    bool          bInitialized = false;
    float         ElapsedTime = 0.f;
    FViewportRect FullRect;

public:
    void ResetBase(const FViewportRect& NewFullRect)
    {
        FullRect     = NewFullRect;
        bInitialized = true;
        ElapsedTime  = 0.f;
    }

    virtual void TickLifetime(float DeltaTime) override
    {
        // if (Duration >= 0.f) { Elapsed += DeltaTime; if (Elapsed >= Duration) bEnabled = false; }
    }
    
    virtual void ApplyToView(float DeltaTime, FMinimalViewInfo* ViewInfo) override
    {
        if (!bEnabled || !ViewInfo) return;

        if (!bInitialized)
        {
            FullRect = ViewInfo->ViewRect; // 파이프라인에서 '수정 전' 뷰를 받을 수 있으면 그걸 쓰면 더 좋다
            ElapsedTime = 0.f;
            bInitialized = true;
        }

        // 2) 진행도(애니메이션)
        ElapsedTime += DeltaTime;
        const float t = (Duration > 0.f) ? FMath::Clamp(ElapsedTime / Duration, 0.f, 1.f) : 1.f;
        const float s = t * t * (3.f - 2.f * t); // smoothstep

        const float W = float(FullRect.Width());
        const float H = float(FullRect.Height());

        // 3) 목표 바(한쪽) 두께 계산 — 오직 상/하만
        float targetBar = 0.f;
        if (HeightBarSize > 0.f)
        {
            targetBar = FMath::Clamp(HeightBarSize, 0.f, 0.5f) * H; // 고정 비율
        }
        else if (AspectRatio > 0.f)
        {
            const float contentH = FMath::Min(H, W / AspectRatio);
            targetBar = 0.5f * FMath::Max(0.f, H - contentH);
        }

        // 4) 픽셀 스냅(반올림)
        const float oneSide = targetBar * s;
        const uint32 total  = (uint32)std::lroundf(2.f * oneSide); // 위+아래 합
        const uint32 top    = total / 2;
        const uint32 bottom = total - top;

        // 5) 좌/우 고정, 위/아래만 조정
        FViewportRect R = FullRect;
        R.MinY = FullRect.MinY + top;
        R.MaxY = FullRect.MaxY - bottom;
        ViewInfo->ViewRect = R;

        const uint32 CW = R.Width();
        const uint32 CH = R.Height();
        ViewInfo->AspectRatio = float(CW) / float(FMath::Max(1u, CH));
    }
};
