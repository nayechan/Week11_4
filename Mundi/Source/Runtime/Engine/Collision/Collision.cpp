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

    float UniformScaleMax(const FVector& S)
    {
        return FMath::Max(FMath::Max(std::fabs(S.X), std::fabs(S.Y)), std::fabs(S.Z));
    }
     
    // 캡슐 obb의 상단, 하단 중점을 반환 + 반지름 반환 
    void BuildCapsule(const FShape& CapsuleShape, const FTransform& TransformCapsule, FVector& OutP0, FVector& OutP1, float& OutRadius)
    {
        const float CapsuleRadius = CapsuleShape.Capsule.CapsuleRadius;
        const float CapsuleHalfHeight = CapsuleShape.Capsule.CapsuleHalfHeight;

        const FVector S = AbsVec(TransformCapsule.Scale3D);
        const float HeightScale = std::fabs(S.Z); 
        const float RadiusScale = FMath::Max(std::fabs(S.X), std::fabs(S.Y));

        const float RadiusWorld = CapsuleRadius * RadiusScale;
        const float HalfHeightWorld = FMath::Max(0.0f, CapsuleHalfHeight - CapsuleRadius) * HeightScale;

        const FVector AxisWorld = TransformCapsule.Rotation.RotateVector(FVector(0, 0, 1)).GetSafeNormal();
        const FVector Center = TransformCapsule.Translation;

        OutP0 = Center - AxisWorld * HalfHeightWorld;
        OutP1 = Center + AxisWorld * HalfHeightWorld;
        OutRadius = RadiusWorld;
    }

    // 캡슐의 가운데 OBB 반환 
    void BuildCapsuleCoreOBB(const FShape& CapsuleShape, const FTransform& Transform, FOBB& Out)
    {
        const float CapsuleRadius = CapsuleShape.Capsule.CapsuleRadius;
        const float HalfHeight= CapsuleShape.Capsule.CapsuleHalfHeight;

        const FVector S = AbsVec(Transform.Scale3D);
        const float AxisScale = std::fabs(S.Z);
        const float RadiusScale = FMath::Max(std::fabs(S.X), std::fabs(S.Y));

        const float RadiusWorld = CapsuleRadius * RadiusScale;
        const float HalfHeightLocal = FMath::Max(0.0f, HalfHeight- CapsuleRadius);
        const float HalfHeightWorld = HalfHeightLocal* AxisScale;

        Out.Center = Transform.Translation;
        const FMatrix R = Transform.Rotation.ToMatrix();
        Out.Axes[0] = FVector(R.M[0][0], R.M[0][1], R.M[0][2]).GetSafeNormal();
        Out.Axes[1] = FVector(R.M[1][0], R.M[1][1], R.M[1][2]).GetSafeNormal();
        Out.Axes[2] = FVector(R.M[2][0], R.M[2][1], R.M[2][2]).GetSafeNormal();

        Out.HalfExtent[0] = RadiusWorld;
        Out.HalfExtent[1] = RadiusWorld;
        Out.HalfExtent[2] = HalfHeightWorld;
    }
     
    // 캡슐 VS Sphere
    bool OverlapCapsuleAndSphere(const FShape& Capsule, const FTransform& TransformCapsule,
        const FShape& Sphere, const FTransform& TransformSphere)
    {
        // 캡슐 코어 obb생성
        FOBB Core{};
        BuildCapsuleCoreOBB(Capsule, TransformCapsule, Core);

        // 캡슐 상하단 중점 + Radius 생성
        FVector TopCenter, BottomCenter; float rCapsule = 0.0f;
        BuildCapsule(Capsule, TransformCapsule, BottomCenter, TopCenter, rCapsule);  

        const float SphereRadius = Sphere.Sphere.SphereRadius * UniformScaleMax(AbsVec(TransformSphere.Scale3D));
        const FVector C = TransformSphere.Translation;

        // 1) Sphere vs OBB
        if (Overlap_Sphere_OBB(C, SphereRadius, Core))
            return true;

        // 2) Sphere vs top/bottom Spheres
        auto SphereSphere = [](const FVector& Pos0, float Radius0, const FVector& Pos1, float Radius1) -> bool
        {
            const FVector d = Pos0 - Pos1; 
            const float rs = Radius0 + Radius1; 
            
            return d.SizeSquared() <= rs * rs;
        };

        if (SphereSphere(C, SphereRadius, TopCenter, rCapsule)) return true;
        if (SphereSphere(C, SphereRadius, BottomCenter, rCapsule)) return true;

        return false;
    }

    // 캡슐 VS Box 
    bool OverlapCapsuleAndBox(const FShape& Capsule, const FTransform& TransformCapsule,
        const FShape& Box, const FTransform& TransformBox)
    { 
        // 캡슐 몸통 부분
        FOBB Core{};
        BuildCapsuleCoreOBB(Capsule, TransformCapsule, Core);

        // 비교 대상 OBB 
        FOBB B{};
        BuildOBB(Box, TransformBox, B);

        // 1) 캡슐 몸통 vs OBB
        if (Overlap_OBB_OBB(Core, B))
            return true;

        // 2) Top/Bottom 반구 vs OBB 
        FVector BottomCenter, TopCenter; float CapsuleRadius = 0.0f;
        BuildCapsule(Capsule, TransformCapsule, BottomCenter, TopCenter, CapsuleRadius);
        if (Overlap_Sphere_OBB(TopCenter, CapsuleRadius, B)) return true;
        if (Overlap_Sphere_OBB(BottomCenter, CapsuleRadius, B)) return true;

        return false;
    }

    // Capsuel - Box 
    // 캡슐은 구 - OBB - 구로 구성되었다고 가정했다. 
    bool OverlapBoxAndCapsule(const FShape& Box, const FTransform& TransformBox,
        const FShape& Capsule, const FTransform& TransformCapsule)
    {
        return OverlapCapsuleAndBox(Capsule, TransformCapsule, Box, TransformBox);
    }
    
    // Capsule - Capsule
    // 캡슐은 구 - OBB - 구로 구성되었다고 가정했다. 
    bool OverlapCapsuleAndCapsule(const FShape& CapsuleA, const FTransform& TransformA,
        const FShape& CapsuleB, const FTransform& TransformB)
    {
        FOBB CoreA{}, CoreB{}; 
        BuildCapsuleCoreOBB(CapsuleA, TransformA, CoreA);
        BuildCapsuleCoreOBB(CapsuleB, TransformB, CoreB);
        
        FVector ABottom, ATop; 
        float RadiusA = 0.0f; 
        BuildCapsule(CapsuleA, TransformA, ABottom, ATop, RadiusA);
        
        FVector BBottom, BTop; 
        float RadiusB = 0.0f; 
        BuildCapsule(CapsuleB, TransformB, BBottom, BTop, RadiusB);

        auto SphereSphere = [](const FVector& Pos0, float Radius0, const FVector& Pos1, float Radius1) -> bool
            {
                const FVector d = Pos0 - Pos1;
                const float rs = Radius0 + Radius1;

                return d.SizeSquared() <= rs * rs;
            };

        // 1) Core vs Core
        if (Overlap_OBB_OBB(CoreA, CoreB)) return true;

        // 2) Core vs Sphere
        if (Overlap_Sphere_OBB(BTop, RadiusB, CoreA)) return true;
        if (Overlap_Sphere_OBB(BBottom, RadiusB, CoreA)) return true;
        if (Overlap_Sphere_OBB(ATop, RadiusA, CoreB)) return true;
        if (Overlap_Sphere_OBB(ABottom, RadiusA, CoreB)) return true;

        // 3) Sphere Sphere 
        if (SphereSphere(ATop, RadiusA, BTop, RadiusB)) return true;
        if (SphereSphere(ATop, RadiusA, BBottom, RadiusB)) return true;
        if (SphereSphere(ABottom, RadiusA, BTop, RadiusB)) return true;
        if (SphereSphere(ABottom, RadiusA, BBottom, RadiusB)) return true;

        return false;
    }

    // Capsuel - Sphere
    // 캡슐은 구 - OBB - 구로 구성되었다고 가정했다. 
    bool OverlapSphereAndCapsule(const FShape& Sphere, const FTransform& TransformSphere,
        const FShape& Capsule, const FTransform& TransformCapsule)
    {
        return OverlapCapsuleAndSphere(Capsule, TransformCapsule, Sphere, TransformSphere);
    }

    // Spherer - OBB 충돌 처리 
    bool Overlap_Sphere_OBB(const FVector& Center, float Radius, const FOBB& B)
    {
        const FVector Dist = Center - B.Center;
        const float Dist0 = FVector::Dot(Dist, B.Axes[0]);
        const float Dist1 = FVector::Dot(Dist, B.Axes[1]);
        const float Dist2 = FVector::Dot(Dist, B.Axes[2]);

        const float CenterX = FMath::Clamp(Dist0, -B.HalfExtent[0], B.HalfExtent[0]);
        const float CenterY = FMath::Clamp(Dist1, -B.HalfExtent[1], B.HalfExtent[1]);
        const float CenterZ = FMath::Clamp(Dist2, -B.HalfExtent[2], B.HalfExtent[2]);

        const float DiffX = Dist0 - CenterX;
        const float DiffY = Dist1 - CenterY;
        const float DiffZ = Dist2 - CenterZ;

        return (DiffX * DiffX + DiffY * DiffY + DiffZ * DiffZ) <= Radius * Radius;
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
        /* Box    */   { &OverlapBoxAndBox,      &OverlapBoxAndSphere,   &OverlapBoxAndCapsule },
        /* Sphere */   { &OverlapSphereAndBox,   &OverlapSphereAndSphere,&OverlapSphereAndCapsule },
        /* Capsule*/   { &OverlapCapsuleAndBox,  &OverlapCapsuleAndSphere,&OverlapCapsuleAndCapsule }
    };


    bool CheckOverlap(const UShapeComponent* A, const UShapeComponent* B)
    {
        FShape ShapeA, ShapeB;
        A->GetShape(ShapeA);
        B->GetShape(ShapeB);

        return OverlapLUT[(int)ShapeA.Kind][(int)ShapeB.Kind](ShapeA, A->GetWorldTransform(), ShapeB, B->GetWorldTransform());
    }


}


