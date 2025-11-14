function BeginPlay()
    print("========================================")
    print("  LuaObjectProxy Test Suite")
    print("========================================")

    -- Test 1: Primitive 타입 읽기/쓰기 (SpotLightComponent)
    print("\n[Test 1] Primitive Property Access")
    local spotlight = GetComponent(Obj, "USpotLightComponent")
    if spotlight then
        -- 읽기 테스트
        local innerAngle = spotlight.InnerConeAngle
        local outerAngle = spotlight.OuterConeAngle
        print("  ✓ Read InnerConeAngle: " .. tostring(innerAngle))
        print("  ✓ Read OuterConeAngle: " .. tostring(outerAngle))

        -- 쓰기 테스트
        spotlight.InnerConeAngle = 20.0
        spotlight.OuterConeAngle = 50.0
        print("  ✓ Write InnerConeAngle: 20.0")
        print("  ✓ Write OuterConeAngle: 50.0")

        -- 재확인
        print("  ✓ Verify InnerConeAngle: " .. tostring(spotlight.InnerConeAngle))
        print("  ✓ Verify OuterConeAngle: " .. tostring(spotlight.OuterConeAngle))
    else
        print("  ✗ SpotLightComponent not found")
    end

    -- Test 2: UObject* 재귀 프로퍼티 접근 (StaticMeshComponent → StaticMesh)
    print("\n[Test 2] UObject Recursive Property Access")
    local staticMeshComp = GetComponent(Obj, "UStaticMeshComponent")
    if staticMeshComp then
        print("  ✓ StaticMeshComponent found")

        -- UStaticMesh* 프로퍼티 접근
        local staticMesh = staticMeshComp.StaticMesh
        if staticMesh then
            print("  ✓ StaticMesh property accessed")

            -- 재귀 접근 테스트: staticMeshComp.StaticMesh.TestVertexCount
            local vertexCount = staticMesh.TestVertexCount
            local indexCount = staticMesh.TestIndexCount
            print("  ✓ Recursive access: StaticMesh.TestVertexCount = " .. tostring(vertexCount))
            print("  ✓ Recursive access: StaticMesh.TestIndexCount = " .. tostring(indexCount))
        else
            print("  - StaticMesh property is nil (no mesh assigned)")
        end
    else
        print("  ✗ StaticMeshComponent not found")
    end

    -- Test 3: nil 할당 테스트
    print("\n[Test 3] Nil Assignment")
    if staticMeshComp and staticMeshComp.StaticMesh then
        print("  ✓ Before: StaticMesh is not nil")
        staticMeshComp.StaticMesh = nil
        print("  ✓ Assigned nil to StaticMesh")
        print("  ✓ After: StaticMesh is " .. tostring(staticMeshComp.StaticMesh))
    else
        print("  - Skipped (StaticMesh already nil or component not found)")
    end

    -- Test 4: bool 프로퍼티 읽기/쓰기
    print("\n[Test 4] Boolean Property Access")
    if spotlight then
        local isActive = spotlight.bIsActive
        print("  ✓ Read bIsActive: " .. tostring(isActive))

        spotlight.bIsActive = false
        print("  ✓ Write bIsActive: false")
        print("  ✓ Verify bIsActive: " .. tostring(spotlight.bIsActive))

        -- 원래 값으로 복원
        spotlight.bIsActive = isActive
    end

    -- Test 5: 안전 검증 (존재하지 않는 프로퍼티)
    print("\n[Test 5] Invalid Property Access")
    if spotlight then
        local invalid = spotlight.NonExistentProperty
        print("  ✓ Non-existent property returns: " .. tostring(invalid))
    end

    print("\n========================================")
    print("  Test Suite Complete!")
    print("========================================\n")
end

function EndPlay()
end

function OnBeginOverlap(OtherActor)
end

function OnEndOverlap(OtherActor)
end

function Tick(dt)
    -- 매 프레임 테스트는 주석 처리
    --[[
    local spotlight = GetComponent(Obj, "USpotLightComponent")
    if spotlight then
        -- 동적으로 각도 변경 테스트
        spotlight.InnerConeAngle = 30.0 + 10.0 * math.sin(GetTime())
        spotlight.OuterConeAngle = 45.0 + 15.0 * math.cos(GetTime())
    end
    ]]--
end
