#include "pch.h"
#include "FbxConvert.h"

// ========================================
// 일반적인 FBX 타입에 대한 특수화된 오버로드
// ========================================
// 이러한 구현은 가장 일반적인 사용 사례에 대해
// 템플릿이 아닌 진입점을 제공하여 컴파일 시간을 개선하고
// 하위 호환성을 유지합니다.
// ========================================

/**
 * ConvertMatrix - FbxAMatrix 특수화
 *
 * UE5 패턴: M[1][1] 보존이 중요합니다!
 * 가장 일반적으로 사용되는 행렬 변환입니다.
 */
FMatrix FFbxConvert::ConvertMatrix(const FbxAMatrix& FbxMatrix)
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
	// UE5 패턴: M[1][1]은 유지, 나머지는 flip
	Result.M[1][0] = -Result.M[1][0];  // Y축 X 컴포넌트 (flip)
	// Result.M[1][1] 변경 없음 - 중요: 올바른 스케일을 위해 M[1][1]을 flip하지 않음!
	Result.M[1][2] = -Result.M[1][2];  // Y축 Z 컴포넌트 (flip)
	Result.M[1][3] = -Result.M[1][3];  // Translation Y (flip)

	// Step 3: 다른 행의 Column 1 flip (Y 컴포넌트들)
	Result.M[0][1] = -Result.M[0][1];  // X축 Y 컴포넌트
	Result.M[2][1] = -Result.M[2][1];  // Z축 Y 컴포넌트
	Result.M[3][1] = -Result.M[3][1];  // W축 Y 컴포넌트

	return Result;
}

/**
 * ConvertPos - FbxVector4 특수화
 */
FVector FFbxConvert::ConvertPos(const FbxVector4& FbxVector)
{
	return FVector(
		static_cast<float>(FbxVector[0]),      // X 변경 없음
		-static_cast<float>(FbxVector[1]),     // Y 반전 (RH → LH)
		static_cast<float>(FbxVector[2])       // Z 변경 없음
	);
}

/**
 * ConvertDir - FbxVector4 특수화
 */
FVector FFbxConvert::ConvertDir(const FbxVector4& FbxVector)
{
	FVector Result = ConvertPos(FbxVector);  // Y-Flip 적용
	Result.Normalize();                      // 단위 방향 벡터로 정규화
	return Result;
}

/**
 * ConvertRotation - FbxQuaternion 특수화
 */
FQuat FFbxConvert::ConvertRotation(const FbxQuaternion& FbxQuat)
{
	// 쿼터니언 Y-Flip: Y와 W 컴포넌트 flip
	// 오른손 좌표계에서 왼손 좌표계로 회전 변환
	return FQuat(
		static_cast<float>(FbxQuat[0]),        // X 변경 없음
		-static_cast<float>(FbxQuat[1]),       // Y flip
		static_cast<float>(FbxQuat[2]),        // Z 변경 없음
		-static_cast<float>(FbxQuat[3])        // W flip
	);
}

/**
 * ConvertScale - FbxVector4 특수화
 */
FVector FFbxConvert::ConvertScale(const FbxVector4& FbxVector)
{
	// 스케일은 항상 양수, handedness 변환 불필요
	return FVector(
		static_cast<float>(FbxVector[0]),
		static_cast<float>(FbxVector[1]),      // flip 안 함
		static_cast<float>(FbxVector[2])
	);
}

/**
 * ConvertScene
 *
 * FBX Scene의 좌표계 변환 및 JointOrientationMatrix 설정 (UE5 Pattern)
 *
 * UE5 Reference:
 * - FbxConvert.cpp Line 61-111
 * - FbxAPI.cpp Line 151 (호출 부분)
 *
 * 이 함수는 두 가지 작업을 수행:
 * 1. FBX Scene 좌표계를 Z-Up, Right-Handed로 변환
 * 2. JointOrientationMatrix 설정 (BindPose 계산 시 사용됨)
 *
 * JointOrientationMatrix 용도:
 * - GlobalBindPoseMatrix = TransformLinkMatrix * JointOrientationMatrix
 * - 애니메이션: NodeTransform = NodeTransform * JointOrientationMatrix
 */
void FFbxConvert::ConvertScene(FbxScene* Scene, bool bForceFrontXAxis, FbxAMatrix& JointOrientationMatrix)
{
	if (!Scene)
	{
		return;
	}

	// Step 1: JointOrientationMatrix 초기화 (항상 먼저 수행)
	JointOrientationMatrix.SetIdentity();

	// Step 2: 좌표계 변환 (UE5 Pattern)
	// UE는 Z-Up, Front는 -Y(기본) 또는 +X(bForceFrontXAxis), Right-Handed 중간 단계
	FbxAxisSystem::EFrontVector FrontVector = (FbxAxisSystem::EFrontVector)(
		bForceFrontXAxis ? FbxAxisSystem::eParityEven : -FbxAxisSystem::eParityOdd
		);

	FbxAxisSystem UnrealImportAxis(
		FbxAxisSystem::eZAxis,        // Up: Z
		FrontVector,                  // Front: -Y (기본) 또는 +X (bForceFrontXAxis)
		FbxAxisSystem::eRightHanded   // Right-Handed 중간 단계
	);

	FbxAxisSystem SourceSetup = Scene->GetGlobalSettings().GetAxisSystem();

	if (SourceSetup != UnrealImportAxis)
	{
		// UE5 패턴: 변환 전 모든 FBX 루트 제거
		FbxRootNodeUtility::RemoveAllFbxRoots(Scene);

		// ConvertScene: Scene Graph만 변환 (정점 데이터는 변환하지 않음)
		UnrealImportAxis.ConvertScene(Scene);

		// 변환 후 evaluator 리셋
		Scene->GetAnimationEvaluator()->Reset();

		// Step 3: JointOrientationMatrix 설정 (좌표계 변환이 실제로 발생한 경우만)
		if (bForceFrontXAxis)
		{
			// +X Forward: -Y Forward를 +X Forward로 변환하는 회전 행렬
			// UE5 Pattern: SetR(-90°, -90°, 0°)
			JointOrientationMatrix.SetR(FbxVector4(-90.0, -90.0, 0.0));
		}
		// else: -Y Forward는 Identity (이미 초기화됨)
	}
	// else: 좌표계 변환 불필요, JointOrientationMatrix는 Identity 유지
}
