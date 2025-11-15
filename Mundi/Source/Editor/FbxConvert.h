#pragma once
#include "Vector.h"
#include "fbxsdk.h"

/**
 * FFbxConvert
 *
 * UE5 스타일의 FBX 데이터 변환 유틸리티 (템플릿 기반 정밀도 지원)
 * 좌표계 변환(오른손 → 왼손)을 Y-Flip 방식으로 처리
 *
 * Refactoring Phase 1: FFbxDataConverter를 대체하는 모듈화되고 UE5 호환되는 설계 패턴.
 * float와 double 정밀도 변환을 모두 지원.
 */
struct FFbxConvert
{
	/**
	 * FBX 위치 벡터를 Mundi FVector로 변환 (Y-Flip 적용)
	 * 오른손 좌표계 → 왼손 좌표계 변환
	 *
	 * 템플릿 버전은 FbxVector4 (double) 및 커스텀 정밀도 타입을 모두 지원
	 */
	template<typename TVector>
	static FVector ConvertPos(const TVector& FbxVector)
	{
		return FVector(
			static_cast<float>(FbxVector[0]),      // X 변경 없음
			-static_cast<float>(FbxVector[1]),     // Y 반전 (RH → LH)
			static_cast<float>(FbxVector[2])       // Z 변경 없음
		);
	}

	/**
	 * FBX 방향 벡터를 Mundi FVector로 변환 (Y-Flip + 정규화)
	 * 노말, 탄젠트, 바이노말에 사용
	 */
	template<typename TVector>
	static FVector ConvertDir(const TVector& FbxVector)
	{
		FVector Result = ConvertPos(FbxVector);  // Y-Flip 적용
		Result.Normalize();                      // 단위 방향 벡터로 정규화
		return Result;
	}

	/**
	 * FBX 쿼터니언을 Mundi FQuat로 변환 (Y-Flip 적용)
	 * handedness 변환을 위해 Y와 W 컴포넌트를 반전
	 */
	template<typename TQuat>
	static FQuat ConvertRotation(const TQuat& FbxQuat)
	{
		// 쿼터니언 Y-Flip: Y와 W 컴포넌트 반전
		// 오른손 좌표계에서 왼손 좌표계로 회전 변환
		return FQuat(
			static_cast<float>(FbxQuat[0]),        // X 변경 없음
			-static_cast<float>(FbxQuat[1]),       // Y 반전
			static_cast<float>(FbxQuat[2]),        // Z 변경 없음
			-static_cast<float>(FbxQuat[3])        // W 반전
		);
	}

	/**
	 * FBX 스케일 벡터를 Mundi FVector로 변환 (Y-Flip 적용 안 함)
	 * 스케일 값은 항상 양수이므로 handedness 변환 불필요
	 */
	template<typename TVector>
	static FVector ConvertScale(const TVector& FbxVector)
	{
		// 스케일은 항상 양수, handedness 변환 불필요
		return FVector(
			static_cast<float>(FbxVector[0]),
			static_cast<float>(FbxVector[1]),      // 반전 안 함
			static_cast<float>(FbxVector[2])
		);
	}

	/**
	 * FBX 4x4 행렬을 Mundi FMatrix로 변환 (Y-Flip 적용)
	 *
	 * Y-Flip 행렬 변환:
	 * 1. Row 1의 요소들을 반전 (M[1][1] 제외 - 중요!)
	 * 2. 다른 행의 Column 1을 반전 (M[0][1], M[2][1], M[3][1])
	 *
	 * UE5 패턴: M[1][1]은 스케일 보존을 위해 반전하지 않음!
	 */
	template<typename TMatrix>
	static FMatrix ConvertMatrix(const TMatrix& FbxMatrix)
	{
		FMatrix Result;

		// Step 1: 모든 행렬 요소 복사
		for (int Row = 0; Row < 4; Row++)
		{
			for (int Col = 0; Col < 4; Col++)
			{
				Result.M[Row][Col] = static_cast<float>(FbxMatrix.Get(Row, Col));
			}
		}

		// Step 2: Row 1 (Y축 행)에 Y-Flip 적용
		// UE5 패턴: M[1][1]은 유지, 나머지는 반전
		Result.M[1][0] = -Result.M[1][0];  // Y축 X 컴포넌트 (반전)
		// Result.M[1][1] 변경 없음 - 중요: 올바른 스케일을 위해 M[1][1]을 flip하지 않음!
		Result.M[1][2] = -Result.M[1][2];  // Y축 Z 컴포넌트 (반전)
		Result.M[1][3] = -Result.M[1][3];  // Translation Y (반전)

		// Step 3: 다른 행의 Column 1 반전 (Y 컴포넌트들)
		Result.M[0][1] = -Result.M[0][1];  // X축 Y 컴포넌트
		Result.M[2][1] = -Result.M[2][1];  // Z축 Y 컴포넌트
		Result.M[3][1] = -Result.M[3][1];  // W축 Y 컴포넌트

		return Result;
	}

	/**
	 * 일반적인 FBX SDK 타입에 대한 특수화된 오버로드
	 * 구현은 FbxConvert.cpp에 있음
	 */
	static FMatrix ConvertMatrix(const FbxAMatrix& FbxMatrix);
	static FVector ConvertPos(const FbxVector4& FbxVector);
	static FVector ConvertDir(const FbxVector4& FbxVector);
	static FQuat ConvertRotation(const FbxQuaternion& FbxQuat);
	static FVector ConvertScale(const FbxVector4& FbxVector);
};
