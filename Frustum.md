# Frustum.h 설명서

위치: TL2/Frustum.h

이 문서는 TL2/Frustum.h 및 TL2/Frustum.cpp가 제공하는 절두체(Frustum) 관련 자료구조와 가시성 판정 유틸리티를 설명합니다. 카메라 파라미터로부터 절두체의 6개 평면을 구성하고, AABB(축 정렬 바운딩 박스)가 절두체와 교차(=화면에 잠재적으로 보임)하는지 빠르게 판정하는 기능을 제공합니다.

- 관련 파일: TL2/Frustum.h, TL2/Frustum.cpp
- 의존성: Vector.h(FVector), AABoundingBoxComponent.h(FBound), CameraComponent.h(UCameraComponent)

## 핵심 개념과 수학적 규약

- 평면 방정식: dot(N, X) − D = 0
  - N: 단위 법선 벡터(Frustum.cpp에서 정규화 보장)
  - D: 원점에서 평면까지의 거리(= dot(N, P0), P0는 평면 위 임의의 점)
- 내부 판정 규약: dot(N, X) − D ≥ 0 이면 절두체의 “안쪽”으로 간주합니다.
- 좌표계/축:
  - Forward = +X, Right = +Y, Up = +Z를 가정합니다.
  - 코드 주석 기준 LH(Left-Handed)를 사용하지만, 외적(Cross)은 RH 정의를 따르며, 피연산자 순서로 법선 방향(안/밖)을 맞춥니다.

## 제공 타입

- Plane
  - Normal: FVector (단위 벡터)
  - Distance: float (원점으로부터 평면까지의 거리)
- Frustum
  - 여섯 개의 평면을 멤버로 보유: TopFace, BottomFace, RightFace, LeftFace, NearFace, FarFace

## 공개 함수

```cpp path=null start=null
// 카메라에서 절두체(6면) 생성
Frustum CreateFrustumFromCamera(const UCameraComponent& Camera, float OverrideAspect = -1.0f);

// AABB(중심/반길이)와 평면 교차 테스트의 단일 평면 판정
bool Intersects(const Plane& P, const FVector& Center, const FVector& Extents);

// 절두체와 AABB의 가시성(교차) 판정: 6면 모두 통과해야 true
bool IsAABBVisible(const Frustum& Frustum, const FBound& Bound);
```

### CreateFrustumFromCamera
- 입력
  - Camera: UCameraComponent (Near/Far clip, FOV, Aspect, 월드 변환/방향 제공)
  - OverrideAspect: 0보다 큰 값이면 이 종횡비를 강제, 기본값 -1.0f면 Camera의 AspectRatio를 사용
- 동작 요약
  - Near/Far 평면: 내부를 향하도록 Near는 +Forward, Far는 −Forward 방향 법선으로 구성
  - 측면(Left/Right/Top/Bottom) 평면: Far 평면에서의 반가로/반세로(HalfHSide/HalfVSide)를 이용해, Cross 순서를 조절하여 모두 내부(절두체 중심) 방향으로 법선을 설정
- 결과
  - 내부가 dot(N, X) − D ≥ 0로 정의된 6개 평면을 갖는 Frustum 반환

### Intersects (평면 vs AABB 단일 테스트)
- 입력: 평면 P(N, D), AABB의 Center(중심), Extents(각 축 반길이, 항상 양수)
- 계산
  - 거리: d = dot(N, Center) − D
  - 프로젝션 반경: r = |Nx|·Ex + |Ny|·Ey + |Nz|·Ez
- 판정
  - d + r ≥ 0 이면 평면을 통과(겹침) → “절두체 바깥이 아님”
  - d + r < 0 이면 해당 평면 기준으로 절두체 바깥 → 즉시 탈락

### IsAABBVisible (프러스텀 vs AABB 종합 테스트)
- 6개 평면에 대해 Intersects를 모두 만족해야 true를 반환합니다.
- 하나라도 실패하면 false를 즉시 반환합니다(빠른 배제).

## 사용 예시

```cpp path=null start=null
// 1) 카메라로부터 절두체 생성
const Frustum fr = CreateFrustumFromCamera(*Camera);

// 2) AABB 준비 (프로젝트 상황에 맞게 FBound를 구성)
FBound boxBound;
boxBound.Min = FVector(-50.f, -50.f, -50.f);
boxBound.Max = FVector( 50.f,  50.f,  50.f);

// 3) 가시성 테스트
if (IsAABBVisible(fr, boxBound)) {
    // 렌더 후보에 포함
} else {
    // 프러스텀 밖 → 스킵(컬링)
}
```

렌더링 파이프라인에서는 보통 모든 객체의 월드 AABB를 준비해 두고, 프레임마다 카메라 절두체를 만든 뒤 각 AABB에 대해 IsAABBVisible을 호출하여 컬링합니다.

## 좌표계/법선 관련 주의사항

- 카메라 축(Forward/Right/Up)은 직교 정규(orthonormal)여야 정확한 평면이 생성됩니다.
- 외적의 피연산자 순서는 법선 방향을 결정합니다. 코드에서는 내부를 향하도록 순서를 이미 맞춰두었습니다.
- FOV는 Degree 입력이며, 내부에서 라디안 변환하여 사용합니다.
- OverrideAspect를 사용할 때는 0보다 큰 값만 유효합니다.
- Near/Far 거리 설정이 비정상적(예: Near ≤ 0, Far ≤ Near)인 경우 잘못된 절두체가 만들어질 수 있습니다.

## 성능 특성

- AABB 하나에 대해 최대 6회의 Intersects(덧셈/곱셈/절댓값/내적) 연산이 수행되며 매우 가볍습니다.
- 실패 평면에서 즉시 탈락하므로 평균 연산량은 더 적습니다.
- 대량 테스트 시 SoA 구조나 SIMD 최적화(SSE/AVX)로 추가 가속이 가능합니다.

## 확장 아이디어

- 구(Sphere) 프러스텀 테스트 추가: d ≥ −r 형태의 간단한 판정으로 빠른 프리패스 가능
- OBB(임의 회전 박스) 지원: AABB보다 일반화된 테스트(Separating Axis Theorem 등) 필요
- 안전 여유(margin) 적용: 폼 팩터/스케일 변화나 TAA/Jitter 대응을 위한 평면 D 확장

## 관련 타입/함수 요약

- FVector: 벡터 연산(Dot, Cross, 정규화 등)을 제공
- FBound: AABB의 Min/Max 보관(Frustum.cpp에서 Center/Extents 계산에 사용)
- UCameraComponent: Near/Far, FOV, Aspect, Transform(Forward/Right/Up, 위치) 제공

## 빠른 체크리스트

- 카메라 파라미터가 최신인가? (줌/FOV, 윈도우 리사이즈 후 Aspect)
- 월드 AABB가 올바른가? (Min ≤ Max, 월드 변환 반영)
- Near/Far 설정이 유효한가? (Far > Near > 0)
- 결과가 뒤집혀 보이면 좌표계/법선 방향 가정을 재점검

---
이 문서는 Frustum.h/Frustum.cpp의 실제 구현과 주석(법선 방향, 좌표계, 판정 규약)을 반영하여 작성되었습니다. 프로젝트별로 좌표계나 카메라 축 정의가 다를 경우, CreateFrustumFromCamera의 평면 생성 규칙을 해당 정의에 맞게 조정해야 합니다.
