#pragma once
#include <cmath>
#include "Vector.h"

namespace SceneRotUtil
{
    inline float DegToRad(float d) { return d * (3.14159f/ 180.0f); }
    inline float RadToDeg(float r) { return r * (180.0f / 3.141592f); }

    inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    // 각도 정규화 [-180, 180]
    inline float NormalizeDeg(float a)
    {
        while (a > 180.0) a -= 360.0;
        while (a < -180.0) a += 360.0;
        return a;
    }

    // Quaternion 정규화(+w 기준 캐논화)
    inline void NormalizeQuat(float& x, float& y, float& z, float& w)
    {
        const float n = std::sqrt(x * x + y * y + z * z + w * w);
        if (n > 1e-12) { x /= n; y /= n; z /= n; w /= n; }
        if (w < 0.0) { x = -x; y = -y; z = -z; w = -w; } // q와 -q 동치 → 표준화
    }

    // ZYX(Tait–Bryan) 순서 오일러(deg) → Quaternion
    // eulerDeg: X=Roll, Y=Pitch, Z=Yaw  [deg]
    inline FQuat QuatFromEulerZYX_Deg(const FVector& eulerDeg)
    {
        const float x = DegToRad((float)eulerDeg.X);
        const float y = DegToRad((float)eulerDeg.Y);
        const float z = DegToRad((float)eulerDeg.Z);

        const float cx = std::cosf(x * 0.5f), sx = std::sinf(x * 0.5f);
        const float cy = std::cosf(y * 0.5f), sy = std::sinf(y * 0.5f);
        const float cz = std::cosf(z * 0.5f), sz = std::sinf(z * 0.5f);

        // q = qz * qy * qx
        float qw = cz * cy * cx + sz * sy * sx;
        float qx = cz * cy * sx - sz * sy * cx;
        float qy = cz * sy * cx + sz * cy * sx;
        float qz = sz * cy * cx - cz * sy * sx;
        NormalizeQuat(qx, qy, qz, qw);
        return FQuat((float)qx, (float)qy, (float)qz, (float)qw);
    }

    // Quaternion → ZYX(Tait–Bryan) 오일러(deg)
    // 반환: X=Roll, Y=Pitch, Z=Yaw  [deg]
    inline FVector EulerZYX_Deg_FromQuat(const FQuat& qIn)
    {
        // FQuat가 (X,Y,Z,W)라 가정
        float x = qIn.X, y = qIn.Y, z = qIn.Z, w = qIn.W;
        NormalizeQuat(x, y, z, w);

        // 회전 행렬 성분 (float)
        const float r00 = 1.0f - 2.0f * (y * y + z * z);
        const float r01 = 2.0f * (x * y - z * w);
        const float r02 = 2.0f * (x * z + y * w);
        const float r10 = 2.0f * (x * y + z * w);
        const float r11 = 1.0f - 2.0f * (x * x + z * z);
        const float r12 = 2.0f * (y * z - x * w);
        const float r20 = 2.0f * (x * z - y * w);
        const float r21 = 2.0f * (y * z + x * w);
        const float r22 = 1.0f - 2.0f * (x * x + y * y);

        // ZYX 분해
        // pitch(y) = asin(-r20)
        const float sy = Clamp(-r20, -1.0, 1.0);
        float pitch = std::asin(sy);

        float roll, yaw;
        const float cy = std::cos(pitch);

        if (std::abs(cy) > 1e-6)
        {
            // 일반 경우
            roll = std::atan2(r21, r22);  // x
            yaw = std::atan2(r10, r00);  // z
        }
        else
        {
            // 짐벌락: cy ~ 0 → roll을 0으로 두고 yaw만 결정 (일관된 규칙)
            roll = 0.0;
            yaw = std::atan2(-r01, r02);
        }

        FVector out;
        out.X = (float)NormalizeDeg(RadToDeg(roll));   // Roll
        out.Y = (float)NormalizeDeg(RadToDeg(pitch));  // Pitch
        out.Z = (float)NormalizeDeg(RadToDeg(yaw));    // Yaw
        return out;
    }
}