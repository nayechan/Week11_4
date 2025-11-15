#include "pch.h"
#include "FbxDataConverter.h"

// ========================================
// Position Conversion (Y-Flip)
// ========================================
FVector FFbxDataConverter::ConvertPos(const FbxVector4& FbxVector)
{
	return FVector(
		static_cast<float>(FbxVector[0]),      // X unchanged
		-static_cast<float>(FbxVector[1]),     // Y flipped (RH → LH)
		static_cast<float>(FbxVector[2])       // Z unchanged
	);
}

// ========================================
// Direction Conversion (Y-Flip + Normalize)
// ========================================
FVector FFbxDataConverter::ConvertDir(const FbxVector4& FbxVector)
{
	FVector Result = ConvertPos(FbxVector);  // Apply Y-Flip
	Result.Normalize();                       // Normalize for unit direction
	return Result;
}

// ========================================
// Quaternion Conversion (Y-Flip)
// ========================================
FQuat FFbxDataConverter::ConvertRotation(const FbxQuaternion& FbxQuat)
{
	// Quaternion Y-Flip: Negate Y and W components
	// This converts rotation from Right-Handed to Left-Handed coordinate system
	return FQuat(
		static_cast<float>(FbxQuat[0]),        // X unchanged
		-static_cast<float>(FbxQuat[1]),       // Y negated
		static_cast<float>(FbxQuat[2]),        // Z unchanged
		-static_cast<float>(FbxQuat[3])        // W negated
	);
}

// ========================================
// Scale Conversion (NO Y-Flip)
// ========================================
FVector FFbxDataConverter::ConvertScale(const FbxVector4& FbxVector)
{
	// Scale is always positive, no handedness conversion needed
	return FVector(
		static_cast<float>(FbxVector[0]),
		static_cast<float>(FbxVector[1]),      // NO negation
		static_cast<float>(FbxVector[2])
	);
}

// ========================================
// Matrix Conversion (Y-Flip)
// ========================================
FMatrix FFbxDataConverter::ConvertFbxMatrixWithYAxisFlip(const FbxAMatrix& FbxMatrix)
{
	FMatrix Result;

	// Step 1: Copy all matrix elements
	for (int Row = 0; Row < 4; Row++)
	{
		for (int Col = 0; Col < 4; Col++)
		{
			Result.M[Row][Col] = static_cast<float>(FbxMatrix.Get(Row, Col));
		}
	}

	// Step 2: Apply Y-Flip to Row 1 (Y-axis row)
	// UE5 Pattern: Keep M[1][1], negate others
	Result.M[1][0] = -Result.M[1][0];  // Y-axis X component (negated)
	// Result.M[1][1] unchanged - CRITICAL: Do NOT negate M[1][1] for correct scale!
	Result.M[1][2] = -Result.M[1][2];  // Y-axis Z component (negated)
	Result.M[1][3] = -Result.M[1][3];  // Translation Y (negated)

	// Step 3: Negate Column 1 in other rows (Y components)
	Result.M[0][1] = -Result.M[0][1];  // X-axis Y component
	Result.M[2][1] = -Result.M[2][1];  // Z-axis Y component
	Result.M[3][1] = -Result.M[3][1];  // W-axis Y component

	return Result;
}

// ========================================
// JointPostConversionMatrix
// ========================================
FbxAMatrix FFbxDataConverter::GetJointPostConversionMatrix(bool bForceFrontXAxis)
{
	FbxAMatrix JointPostMatrix;
	JointPostMatrix.SetIdentity();

	if (bForceFrontXAxis)
	{
		// Convert -Y Forward to +X Forward
		// Rotation: (-90°, -90°, 0°) in Euler angles
		JointPostMatrix.SetR(FbxVector4(-90.0, -90.0, 0.0));
	}

	return JointPostMatrix;
}
