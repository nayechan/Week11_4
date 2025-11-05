#pragma once
#include "CameraModifierBase.h"

enum class EShakeNoise : uint8
{
	Sine,
	Perlin,
	Mixed,   // Sine/Perlin 가중 혼합
};

class UCamMod_Shake : public UCameraModifierBase
{
public:
	DECLARE_CLASS(UCamMod_Shake, UCameraModifierBase)

	UCamMod_Shake();
	virtual ~UCamMod_Shake() = default;

	// === 최소 파라미터 ===
	float BlendIn = 0.02f;   // 인 램프
	float BlendOut = 0.10f;   // 아웃 램프

	float AmplitudeLocation = 1.5f;  // 위치 진폭(cm) - 카메라 로컬
	float AmplitudeRotationDegrees = 2.0f;  // 회전 진폭(deg)  - pitch/yaw/roll 균등
	float Frequency = 8.0f;  // 기본 주파수(Hz)

	// 노이즈 모드
	EShakeNoise NoiseMode = EShakeNoise::Perlin;
	float MixRatio = 0.5f; // Mixed일 때: 0=Sine, 1=Perlin

	// API
	void Initialize(float InDuration, float InAmpLoc, float InAmpRotDeg, float InFrequency,
		EShakeNoise InMode = EShakeNoise::Sine, float InMixRatio = 0.5f);

	virtual void ApplyToView(float DeltaTime, FMinimalViewInfo* ViewInfo) override;
	virtual void TickLifetime(float DeltaTime) override;

	// (선택) ImGui 디버그 패널
	void DrawImGui();

private:
	// 재생 상태
	float Time = 0.0f;
	float EnvelopeAlpha = 0.0f;

	// 간단 위상 (축간 상관 줄이기)
	float PhaseX = 1.1f, PhaseY = 2.3f, PhaseZ = 3.7f, PhaseR = 5.5f;

	// 엔벌로프(스무스스텝 기반)
	float EvalEnvelope01(float T) const;

	// 샘플러
	static inline float Sine2Pi(float X);
	float SampleChannel(float T, float Freq, float Phase, EShakeNoise Mode) const;

	// Perlin 1D ([-1,1])
	static float Perlin1D(float X);
	static float Fade(float T);
	static float Grad(int Hash, float X);

	// Perlin용 perm 테이블
	static const uint8 Perm[256];
};
