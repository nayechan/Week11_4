#include "pch.h"
#include "Occlusion.h"
#include "AABoundingBoxComponent.h"
#include "Frustum.h"

// NDC Z가 [-1..1]인 프로젝션이면 아래 변환을 켜세요.
// static inline float To01(float z_ndc) { return z_ndc * 0.5f + 0.5f; }
static inline void Clamp01(float& v) { v = std::max(0.0f, std::min(1.0f, v)); }

// FBound(Min/Max) → 8코너
static inline void MakeAabbCornersMinMax(const FBound& B, FVector Corners[8])
{
	const FVector& mn = B.Min;
	const FVector& mx = B.Max;
	Corners[0] = { mn.X, mn.Y, mn.Z };
	Corners[1] = { mx.X, mn.Y, mn.Z };
	Corners[2] = { mn.X, mx.Y, mn.Z };
	Corners[3] = { mx.X, mx.Y, mn.Z };
	Corners[4] = { mn.X, mn.Y, mx.Z };
	Corners[5] = { mx.X, mn.Y, mx.Z };
	Corners[6] = { mn.X, mx.Y, mx.Z };
	Corners[7] = { mx.X, mx.Y, mx.Z };
}

bool FOcclusionCullingManagerCPU::ComputeRectAndMinZ(
	const FCandidateDrawable& D, int /*ViewW*/, int /*ViewH*/, FOcclusionRect& OutR)
{
	FVector C[8];
	MakeAabbCornersMinMax(D.Bound, C);

	float MinX = +1e9f, MinY = +1e9f, MaxX = -1e9f, MaxY = -1e9f;
	float MinZ = +1e9f, MaxZ = -1e9f;

	int used = 0;          // ★ 사용된 코너 수
	for (int i = 0; i < 8; i++)
	{
		const float p[4] = { C[i].X,C[i].Y,C[i].Z,1.0f };
		float c[4];
		MulPointRow(p, D.WorldViewProj, c);

		if (c[3] <= 0.0f)  // ★ 카메라 뒤면 제외
			continue;

		const float invW = 1.0f / c[3];
		float ndcX = c[0] * invW; // -1..1
		float ndcY = c[1] * invW; // -1..1
		float ndcZ = c[2] * invW; // D3D면 0..1, GL식이면 To01 적용

		// ndcZ = To01(ndcZ); // GL식이면 켜기

		float u = 0.5f * (ndcX + 1.0f);
		float v = 0.5f * (1.0f - ndcY);

		MinX = std::min(MinX, u); MinY = std::min(MinY, v);
		MaxX = std::max(MaxX, u); MaxY = std::max(MaxY, v);
		MinZ = std::min(MinZ, ndcZ);
		MaxZ = std::max(MaxZ, ndcZ);
		used++;
	}

	if (used == 0) return false; // ★ 전부 w<=0 → 스킵

	// 화면 밖 완전 스킵 (원래 로직 유지)
	if (MaxX < 0 || MaxY < 0 || MinX > 1 || MinY > 1)
		return false;

	Clamp01(MinX); Clamp01(MinY); Clamp01(MaxX); Clamp01(MaxY);
	Clamp01(MinZ); Clamp01(MaxZ);

	OutR.MinX = MinX; OutR.MinY = MinY; OutR.MaxX = MaxX; OutR.MaxY = MaxY;
	OutR.MinZ = MinZ; OutR.MaxZ = MaxZ;
	OutR.ActorIndex = D.ActorIndex;
	return true;
}
void FOcclusionCullingManagerCPU::BuildOccluderDepth(
	const TArray<FCandidateDrawable>& Occluders, int ViewW, int ViewH)
{
	Grid.Clear();

	const int GW = Grid.GetWidth();
	const int GH = Grid.GetHeight();

	for (const auto& D : Occluders)
	{
		FOcclusionRect R;
		if (!ComputeRectAndMinZ(D, ViewW, ViewH, R))
			continue;

		int minPX = (int)std::floor(R.MinX * GW);
		int minPY = (int)std::floor(R.MinY * GH);
		int maxPX = (int)std::ceil(R.MaxX * GW) - 1;
		int maxPY = (int)std::ceil(R.MaxY * GH) - 1;

		// 경계 틈새 방지: 1픽셀 팽창(옵션)
		const int dilate = 1;
		minPX -= dilate; minPY -= dilate;
		maxPX += dilate; maxPY += dilate;

		// ✨ 핵심: 오클루더는 '가장 먼' 깊이로 기록
		Grid.RasterizeRectDepthMax(minPX, minPY, maxPX, maxPY, R.MaxZ);
	}
}

// 2) 후보 가시성 판정(HZB 샘플)
void FOcclusionCullingManagerCPU::TestOcclusion(const TArray<FCandidateDrawable>& Candidates, int ViewW, int ViewH, TArray<uint8_t>& OutVisibleFlags)
{
	const float eps = 2e-3f;  // 1차 바이어스
	const float eps2 = 4e-3f;  // 레벨0 재검증 바이어스(조금 더 큼)

	// --- 크기 보장 ---
	uint32_t maxId = 0;
	for (auto& c : Candidates) maxId = std::max(maxId, c.ActorIndex);

	if (OutVisibleFlags.size() <= maxId) OutVisibleFlags.resize(maxId + 1, 1);
	if (VisibleStreak.size() <= maxId) VisibleStreak.resize(maxId + 1, 0);
	if (OccludedStreak.size() <= maxId) OccludedStreak.resize(maxId + 1, 0);
	if (LastState.size() <= maxId) LastState.resize(maxId + 1, 1); // 초기=보임

	for (const auto& D : Candidates)
	{
		uint32_t id = D.ActorIndex;

		FOcclusionRect R;
		if (!ComputeRectAndMinZ(D, ViewW, ViewH, R))
		{
			// 화면 밖(또는 w<=0 코너만) → 가려짐으로 처리
			OutVisibleFlags[id] = 0;
			// streak 업데이트 (선택): occluded 누적
			OccludedStreak[id] = std::min<uint8_t>(255, OccludedStreak[id] + 1);
			VisibleStreak[id] = 0;
			LastState[id] = 0;
			continue;
		}

		const float rw = std::max(0.0f, R.MaxX - R.MinX);
		const float rh = std::max(0.0f, R.MaxY - R.MinY);
		const float pxW = rw * ViewW;
		const float pxH = rh * ViewH;

		// --- 작은 사각형 가드: 한 변이라도 2px 미만이면 컬링하지 않음 ---
		if (std::min(pxW, pxH) < 2.0f)
		{
			OutVisibleFlags[id] = 1;
			VisibleStreak[id] = std::min<uint8_t>(255, VisibleStreak[id] + 1);
			OccludedStreak[id] = 0;
			LastState[id] = 1;
			continue;
		}

		// --- 보수적 mip 선택 ---
		int mip = std::max(0, Grid.ChooseMip(rw, rh) - 1);
		if (pxW < 24.0f || pxH < 24.0f)
			mip = std::max(0, mip - 1);

		// --- MAX HZB 적응형 샘플 ---
		const float hzbMax = Grid.SampleMaxRectAdaptive(R.MinX, R.MinY, R.MaxX, R.MaxY, mip);

		bool occluded = ((hzbMax + eps) <= R.MinZ);

		// --- 레벨0 정밀 재검증(occluded일 때만) ---
		if (occluded)
		{
			if (!Grid.FullyOccludedAtLevel0(R.MinX, R.MinY, R.MaxX, R.MaxY, R.MinZ, eps2))
				occluded = false;
		}

		// --- 양방향 히스테리시스(2~3프레임 연속일 때만 상태 전환) ---
		const int thresh = 2; // 2~3 추천

		if (occluded)
		{
			OccludedStreak[id] = std::min<uint8_t>(255, OccludedStreak[id] + 1);
			VisibleStreak[id] = 0;

			// 직전이 보임이면, thresh 미만 동안은 보임 유지
			if (LastState[id] == 1 && OccludedStreak[id] < thresh)
				occluded = false;
		}
		else
		{
			VisibleStreak[id] = std::min<uint8_t>(255, VisibleStreak[id] + 1);
			OccludedStreak[id] = 0;

			// 직전이 가려짐이면, thresh 미만 동안은 가려짐 유지
			if (LastState[id] == 0 && VisibleStreak[id] < thresh)
				occluded = true;
		}

		LastState[id] = occluded ? 0 : 1;
		OutVisibleFlags[id] = occluded ? 0 : 1;
	}
}