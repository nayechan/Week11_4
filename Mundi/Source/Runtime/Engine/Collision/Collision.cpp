#include "pch.h"
#include "Collision.h"
#include "AABB.h"
#include "OBB.h"
#include "BoundingSphere.h"
#include "ShapeComponent.h"

namespace Collision
{
    bool Intersects(const FAABB& Aabb, const FOBB& Obb)
    {
        const FOBB ConvertedOBB(Aabb, FMatrix::Identity());
        return ConvertedOBB.Intersects(Obb);
    }

    bool Intersects(const FAABB& Aabb, const FBoundingSphere& Sphere)
    {
		// Real Time Rendering 4th, 22.13.2 Sphere/Box Intersection
        float Dist2 = 0.0f;

        for (int32 i = 0; i < 3; ++i)
        {
            if (Sphere.Center[i] < Aabb.Min[i])
            {
                float Delta = Sphere.Center[i] - Aabb.Min[i];
                if (Delta < -Sphere.GetRadius())
					return false;
				Dist2 += Delta * Delta;
            }
            else if (Sphere.Center[i] > Aabb.Max[i])
            {
                float Delta = Sphere.Center[i] - Aabb.Max[i];
                if (Delta > Sphere.GetRadius())
                    return false;
                Dist2 += Delta * Delta;
            }
        }
        return Dist2 <= (Sphere.Radius * Sphere.Radius);
	}

    bool Intersects(const FOBB& Obb, const FBoundingSphere& Sphere)
    {
        // Real Time Rendering 4th, 22.13.2 Sphere/Box Intersection
        float Dist2 = 0.0f;
        // OBB의 로컬 좌표계로 구 중심점 변환
        FVector LocalCenter = Obb.Center;
        for (int32 i = 0; i < 3; ++i)
        {
            FVector Axis = Obb.Axes[i];
            float Offset = FVector::Dot(Sphere.Center - Obb.Center, Axis);
            if (Offset > Obb.HalfExtent[i])
                Offset = Obb.HalfExtent[i];
            else if (Offset < -Obb.HalfExtent[i])
                Offset = -Obb.HalfExtent[i];
            LocalCenter += Axis * Offset;
        }
        for (int i = 0; i < 3; ++i)
        {
            if (LocalCenter[i] < -Obb.HalfExtent[i])
            {
                float Delta = LocalCenter[i] + Obb.HalfExtent[i];
                if (Delta < -Sphere.GetRadius())
                    return false;
                Dist2 += Delta * Delta;
            }
            else if (LocalCenter[i] > Obb.HalfExtent[i])
            {
                float Delta = LocalCenter[i] - Obb.HalfExtent[i];
                if (Delta > Sphere.GetRadius())
                    return false;
                Dist2 += Delta * Delta;
            }
        }
        return Dist2 <= (Sphere.Radius * Sphere.Radius);
	}
     
    bool OverlapSphereAndSphere(const FShape& ShapeA, const FTransform& TransformA, const FShape& ShapeB, const FTransform& TransformB)
    {
        FVector Dist = TransformA.Translation - TransformB.Translation;
        float SumRadius = ShapeA.Sphere.SphereRadius + ShapeB.Sphere.SphereRadius;

        return Dist.SizeSquared() <= SumRadius * SumRadius;
    }
    
    FVector AbsVec(const FVector& v)
    {
        return FVector(std::fabs(v.X), std::fabs(v.Y), std::fabs(v.Z));
    }

    void BuildOBB(const FShape& BoxShape, const FTransform& BoxTranform, FOBB& Out)
    {
        Out.Center = BoxTranform.Translation;

        const FMatrix R = BoxTranform.Rotation.ToMatrix();

        // Sclae + Rotation 행렬 곱하고, Normalize
        Out.Axes[0] = FVector(R.M[0][0], R.M[0][1], R.M[0][2]).GetSafeNormal();
        Out.Axes[1] = FVector(R.M[1][0], R.M[1][1], R.M[1][2]).GetSafeNormal();
        Out.Axes[2] = FVector(R.M[2][0], R.M[2][1], R.M[2][2]).GetSafeNormal();


        // half-extent에 abs 스케일 적용
        const FVector S = AbsVec(BoxTranform.Scale3D);
        const FVector HalfExtent = BoxShape.Box.BoxExtent;
        Out.HalfExtent[0] = HalfExtent.X * S.X;
        Out.HalfExtent[1] = HalfExtent.Y * S.Y;
        Out.HalfExtent[2] = HalfExtent.Z * S.Z;
    }
    
    bool Overlap_OBB_OBB(const FOBB& A, const FOBB& B)
    {
        constexpr float EPS = 1e-6f;

        auto ProjectRadius = [](const FOBB& box, const FVector& axisUnit) -> float
            {
                // box의 세 축을 axisUnit에 투영했을 때의 합
                return  box.HalfExtent[0] * FMath::Abs(FVector::Dot(box.Axes[0], axisUnit))
                    + box.HalfExtent[1] * FMath::Abs(FVector::Dot(box.Axes[1], axisUnit))
                    + box.HalfExtent[2] * FMath::Abs(FVector::Dot(box.Axes[2], axisUnit));
            };

        auto SeparatedOnAxis = [&](const FVector& axis) -> bool
            {
                // 축이 너무 짧으면(평행·중복) 건너뜀
                const float len2 = axis.SizeSquared();
                if (len2 < EPS) return false;

                const FVector n = axis / FMath::Sqrt(len2);          // 단위축
                const float dist = FMath::Abs(FVector::Dot(B.Center - A.Center, n));
                const float ra = ProjectRadius(A, n);
                const float rb = ProjectRadius(B, n);
                return dist > (ra + rb);                             // 분리되면 true
            };

        // 1) A의 세 면 법선
        if (SeparatedOnAxis(A.Axes[0])) return false;
        if (SeparatedOnAxis(A.Axes[1])) return false;
        if (SeparatedOnAxis(A.Axes[2])) return false;

        // 2) B의 세 면 법선
        if (SeparatedOnAxis(B.Axes[0])) return false;
        if (SeparatedOnAxis(B.Axes[1])) return false;
        if (SeparatedOnAxis(B.Axes[2])) return false;

        // 3) 9개 교차축 A x B
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                if (SeparatedOnAxis(FVector::Cross(A.Axes[i], B.Axes[j])))
                    return false;
            }
        }

        return true;
    }

    static inline float UniformScaleMax(const FVector& S)
    {
        return FMath::Max(FMath::Max(std::fabs(S.X), std::fabs(S.Y)), std::fabs(S.Z));
    }

    // Sphere–OBB recent-closest test
    bool Overlap_Sphere_OBB(const FVector& Center, float Radius, const FOBB& B)
    {
        const FVector d = Center - B.Center;
        const float q0 = FVector::Dot(d, B.Axes[0]);
        const float q1 = FVector::Dot(d, B.Axes[1]);
        const float q2 = FVector::Dot(d, B.Axes[2]);

        const float cx = FMath::Clamp(q0, -B.HalfExtent[0], B.HalfExtent[0]);
        const float cy = FMath::Clamp(q1, -B.HalfExtent[1], B.HalfExtent[1]);
        const float cz = FMath::Clamp(q2, -B.HalfExtent[2], B.HalfExtent[2]);

        const float dx = q0 - cx, dy = q1 - cy, dz = q2 - cz;
        return (dx * dx + dy * dy + dz * dz) <= Radius * Radius;
    }

    bool OverlapSphereAndBox(const FShape& ShapeA, const FTransform& TransformA,
        const FShape& ShapeB, const FTransform& TransformB)
    {
     
        // 구 중심과 반지름(비등방 스케일 상계 적용)
        const FVector SphereCenter = TransformA.Translation;
        const float SphereRadius = ShapeA.Sphere.SphereRadius* UniformScaleMax(AbsVec(TransformA.Scale3D));

        FOBB B{}; 
        BuildOBB(ShapeB, TransformB, B);

        return Overlap_Sphere_OBB(SphereCenter, SphereRadius, B);
    }

    bool OverlapBoxAndSphere(const FShape& ShapeA, const FTransform& TransformA,
        const FShape& ShapeB, const FTransform& TransformB)
    { 
        FOBB B{}; 
        BuildOBB(ShapeA, TransformA, B);

        // 구 중심과 반지름(비등방 스케일 상계 적용)
        const FVector SphereCenter = TransformB.Translation;
        const float SphereRadius = ShapeB.Sphere.SphereRadius * UniformScaleMax(AbsVec(TransformB.Scale3D));

        return Overlap_Sphere_OBB(SphereCenter, SphereRadius, B);
    }

    bool OverlapBoxAndBox(const FShape& ShapeA, const FTransform& TransformA, const FShape& ShapeB, const FTransform& TransformB)
    {
        // 두 입력 모두 OBB로 구성
        FOBB A{}, B{};
        BuildOBB(ShapeA, TransformA, A);
        BuildOBB(ShapeB, TransformB, B);
        return Overlap_OBB_OBB(A, B);
        return true;

    }
    // 0: Box ,1: Sphere, 2: Capsule
    OverlapFunc OverlapLUT[3][3] =
    {
        /* Box    */ { &OverlapBoxAndBox,     &OverlapBoxAndSphere,  &OverlapSphereAndBox },
        /* Sphere */ { &OverlapSphereAndBox,  &OverlapSphereAndSphere,&OverlapSphereAndSphere },
        /* Capsule*/ { &OverlapSphereAndBox,  &OverlapSphereAndSphere,&OverlapSphereAndSphere }
    };


    bool CheckOverlap(const UShapeComponent* A, const UShapeComponent* B)
    {
        FShape ShapeA, ShapeB;
        A->GetShape(ShapeA);
        B->GetShape(ShapeB);

        return OverlapLUT[(int)ShapeA.Kind][(int)ShapeB.Kind](ShapeA, A->GetWorldTransform(), ShapeB, B->GetWorldTransform());
    }


}


