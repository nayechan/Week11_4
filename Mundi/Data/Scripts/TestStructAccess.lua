-- USTRUCT 리플렉션 테스트 스크립트
-- UTestComponent의 FTestTransform 구조체에 Lua에서 접근 가능한지 검증

function BeginPlay()
    print("=== USTRUCT Reflection Test ===")

    -- UTestComponent 가져오기
    local TestComp = GetComponent(Obj, "UTestComponent")
    if not TestComp then
        print("[ERROR] UTestComponent not found!")
        return
    end

    print("[TestStructAccess] UTestComponent found")

    -- 1. 초기 값 읽기 테스트
    print("\n[1] Reading initial struct values:")
    print(string.format("  Position: (%.1f, %.1f, %.1f)",
        TestComp.TestTransform.Position.X,
        TestComp.TestTransform.Position.Y,
        TestComp.TestTransform.Position.Z))
    print(string.format("  Rotation: (%.1f, %.1f, %.1f)",
        TestComp.TestTransform.Rotation.X,
        TestComp.TestTransform.Rotation.Y,
        TestComp.TestTransform.Rotation.Z))
    print(string.format("  Scale: (%.1f, %.1f, %.1f)",
        TestComp.TestTransform.Scale.X,
        TestComp.TestTransform.Scale.Y,
        TestComp.TestTransform.Scale.Z))
    print(string.format("  Alpha: %.2f", TestComp.TestTransform.Alpha))

    -- 2. 값 쓰기 테스트
    print("\n[2] Writing new struct values:")

    -- FVector는 값 타입이므로 전체를 대입해야 함 (Unreal Blueprint와 동일)
    local NewPosition = {X = 100.0, Y = 200.0, Z = 300.0}
    local NewRotation = {X = 45.0, Y = 90.0, Z = 180.0}
    local NewScale = {X = 2.0, Y = 3.0, Z = 4.0}

    TestComp.TestTransform.Position = NewPosition
    TestComp.TestTransform.Rotation = NewRotation
    TestComp.TestTransform.Scale = NewScale
    TestComp.TestTransform.Alpha = 0.5

    print("  Values written successfully")

    -- 3. 쓴 값 다시 읽기 (검증)
    print("\n[3] Verifying written values:")
    print(string.format("  Position: (%.1f, %.1f, %.1f)",
        TestComp.TestTransform.Position.X,
        TestComp.TestTransform.Position.Y,
        TestComp.TestTransform.Position.Z))
    print(string.format("  Rotation: (%.1f, %.1f, %.1f)",
        TestComp.TestTransform.Rotation.X,
        TestComp.TestTransform.Rotation.Y,
        TestComp.TestTransform.Rotation.Z))
    print(string.format("  Scale: (%.1f, %.1f, %.1f)",
        TestComp.TestTransform.Scale.X,
        TestComp.TestTransform.Scale.Y,
        TestComp.TestTransform.Scale.Z))
    print(string.format("  Alpha: %.2f", TestComp.TestTransform.Alpha))

    -- 4. C++ 메서드 호출 테스트
    print("\n[4] Calling C++ PrintTransform():")
    TestComp:PrintTransform()

    print("\n=== Test Complete ===")
end

-- Track elapsed time for animation
local ElapsedTime = 0
local RotationZ = 0

function Tick(dt)
    -- Tick에서 동적으로 값 변경 테스트
    local TestComp = GetComponent(Obj, "UTestComponent")
    if TestComp then
        ElapsedTime = ElapsedTime + dt

        -- Alpha를 사인파로 애니메이션
        TestComp.TestTransform.Alpha = (math.sin(ElapsedTime * 2.0) + 1.0) / 2.0

        -- 회전 애니메이션 (FVector는 전체 대입 필요)
        RotationZ = RotationZ + dt * 30.0
        if RotationZ > 360.0 then
            RotationZ = 0.0
        end

        TestComp.TestTransform.Rotation = {
            X = TestComp.TestTransform.Rotation.X,
            Y = TestComp.TestTransform.Rotation.Y,
            Z = RotationZ
        }
    end
end

function EndPlay()
    print("[TestStructAccess] Ending")
end
