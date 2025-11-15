#pragma once
#include "Vector.h"
#include "fbxsdk.h"

/**
 * FFbxDataConverter
 *
 * Utility class for converting FBX data to Mundi Engine format.
 * Handles coordinate system conversion (Right-Handed to Left-Handed) via Y-Flip.
 *
 * Week10 Migration: This class centralizes all FBX→Mundi conversion logic,
 * ensuring consistent Y-axis flipping and coordinate space transformations.
 */
class FFbxDataConverter
{
public:
	/**
	 * Convert FBX position vector to Mundi FVector (with Y-Flip)
	 * Right-Handed → Left-Handed coordinate system conversion
	 */
	static FVector ConvertPos(const FbxVector4& FbxVector);

	/**
	 * Convert FBX direction vector to Mundi FVector (with Y-Flip + Normalize)
	 * Used for normals, tangents, binormals
	 */
	static FVector ConvertDir(const FbxVector4& FbxVector);

	/**
	 * Convert FBX quaternion to Mundi FQuat (with Y-Flip)
	 * Y and W components are negated for handedness conversion
	 */
	static FQuat ConvertRotation(const FbxQuaternion& FbxQuat);

	/**
	 * Convert FBX scale vector to Mundi FVector (NO Y-Flip)
	 * Scale values are always positive, no handedness conversion needed
	 */
	static FVector ConvertScale(const FbxVector4& FbxVector);

	/**
	 * Convert FBX 4x4 matrix to Mundi FMatrix (with Y-Flip)
	 *
	 * Y-Flip matrix transformation:
	 * 1. Negate entire Row 1 (Y-axis row)
	 * 2. Negate Column 1 in other rows (M[0][1], M[2][1], M[3][1])
	 *
	 * This preserves matrix multiplication semantics while flipping Y-axis.
	 */
	static FMatrix ConvertFbxMatrixWithYAxisFlip(const FbxAMatrix& FbxMatrix);

	/**
	 * Get JointPostConversionMatrix for skeletal mesh bones
	 *
	 * @param bForceFrontXAxis - If true, converts -Y Forward to +X Forward
	 *                           This is required for proper bone orientation
	 *
	 * Returns: Rotation matrix (-90, -90, 0) if bForceFrontXAxis is true,
	 *          Identity matrix otherwise
	 */
	static FbxAMatrix GetJointPostConversionMatrix(bool bForceFrontXAxis = true);
};
