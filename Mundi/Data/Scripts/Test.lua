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
        print("   Read InnerConeAngle: " .. tostring(innerAngle))
        print("   Read OuterConeAngle: " .. tostring(outerAngle))

        -- 쓰기 테스트
        spotlight.InnerConeAngle = 20.0
        spotlight.OuterConeAngle = 50.0
        print("   Write InnerConeAngle: 20.0")
        print("   Write OuterConeAngle: 50.0")

        -- 재확인
        print("   Verify InnerConeAngle: " .. tostring(spotlight.InnerConeAngle))
        print("   Verify OuterConeAngle: " .. tostring(spotlight.OuterConeAngle))
    else
        print("  ✗ SpotLightComponent not found")
    end

    -- Test 2: UObject* 재귀 프로퍼티 접근 (StaticMeshComponent → StaticMesh)
    print("\n[Test 2] UObject Recursive Property Access")
    local staticMeshComp = GetComponent(Obj, "UStaticMeshComponent")
    if staticMeshComp then
        print("   StaticMeshComponent found")

        -- UStaticMesh* 프로퍼티 접근
        local staticMesh = staticMeshComp.StaticMesh
        if staticMesh then
            print("   StaticMesh property accessed")

            -- 재귀 접근 테스트: staticMeshComp.StaticMesh.TestVertexCount
            local vertexCount = staticMesh.TestVertexCount
            local indexCount = staticMesh.TestIndexCount
            print("   Recursive access: StaticMesh.TestVertexCount = " .. tostring(vertexCount))
            print("   Recursive access: StaticMesh.TestIndexCount = " .. tostring(indexCount))
        else
            print("  - StaticMesh property is nil (no mesh assigned)")
        end
    else
        print("  ✗ StaticMeshComponent not found")
    end

    -- Test 3: TArray<int32> 기본값 확인 및 읽기/쓰기
    print("\n[Test 3] TArray<int32> Default Values & Access")
    if staticMeshComp then
        local intArray = staticMeshComp.TestIntArray
        print("   TestIntArray accessed (type: " .. type(intArray) .. ")")

        -- 기본값 확인 (생성자에서 초기화한 값)
        print("   Initial length: " .. #intArray)
        print("   Initial values:")
        for i = 1, #intArray do
            print("     [" .. i .. "] = " .. intArray[i])
        end

        -- 배열에 값 추가/변경
        intArray[1] = 100
        intArray[6] = 600  -- 새 값 추가
        print("   Modified intArray[1] = 100")
        print("   Added intArray[6] = 600")
        print("   Updated length: " .. #intArray)
        print("   New value intArray[6] = " .. tostring(intArray[6]))
    end

    -- Test 4: TArray<float> 기본값 확인 및 읽기/쓰기
    print("\n[Test 4] TArray<float> Default Values & Access")
    if staticMeshComp then
        local floatArray = staticMeshComp.TestFloatArray
        print("   TestFloatArray accessed (type: " .. type(floatArray) .. ")")

        -- 기본값 확인
        print("   Initial length: " .. #floatArray)
        print("   Initial values:")
        for i = 1, #floatArray do
            print("     [" .. i .. "] = " .. floatArray[i])
        end

        -- 값 수정
        floatArray[1] = 1.5
        floatArray[2] = 2.7
        floatArray[3] = 3.14159
        print("   Modified values:")
        print("   floatArray[1] = " .. tostring(floatArray[1]))
        print("   floatArray[2] = " .. tostring(floatArray[2]))
        print("   floatArray[3] = " .. tostring(floatArray[3]))
        print("   Array length: " .. #floatArray)
    end

    -- Test 5: TArray<FString> 기본값 확인 및 읽기/쓰기
    print("\n[Test 5] TArray<FString> Default Values & Access")
    if staticMeshComp then
        local stringArray = staticMeshComp.TestStringArray
        print("   TestStringArray accessed (type: " .. type(stringArray) .. ")")

        -- 기본값 확인
        print("   Initial length: " .. #stringArray)
        print("   Initial values:")
        for i = 1, #stringArray do
            print("     [" .. i .. "] = " .. stringArray[i])
        end

        -- 값 수정
        stringArray[1] = "Hello"
        stringArray[2] = "Lua"
        stringArray[3] = "World"
        print("   Modified values:")
        print("   stringArray[1] = " .. stringArray[1])
        print("   stringArray[2] = " .. stringArray[2])
        print("   stringArray[3] = " .. stringArray[3])
        print("   Array length: " .. #stringArray)
    end

    -- Test 6: TArray<FVector> 기본값 확인 및 읽기/쓰기
    print("\n[Test 6] TArray<FVector> Default Values & Access")
    if staticMeshComp then
        local vectorArray = staticMeshComp.TestVectorArray
        print("   TestVectorArray accessed (type: " .. type(vectorArray) .. ")")

        -- 기본값 확인
        print("   Initial length: " .. #vectorArray)
        print("   Initial values:")
        for i = 1, #vectorArray do
            local v = vectorArray[i]
            print("     [" .. i .. "] = (" .. v.X .. ", " .. v.Y .. ", " .. v.Z .. ")")
        end

        -- 값 수정
        vectorArray[1] = Vector(1.0, 2.0, 3.0)
        vectorArray[2] = Vector(4.0, 5.0, 6.0)

        print("   Modified values:")
        local v1 = vectorArray[1]
        local v2 = vectorArray[2]
        print("   vectorArray[1] = (" .. v1.X .. ", " .. v1.Y .. ", " .. v1.Z .. ")")
        print("   vectorArray[2] = (" .. v2.X .. ", " .. v2.Y .. ", " .. v2.Z .. ")")
        print("   Array length: " .. #vectorArray)
    end

    -- Test 7: TMap<FString, int32> 기본값 확인 및 읽기/쓰기
    print("\n[Test 7] TMap<FString, int32> Default Values & Access")
    if staticMeshComp then
        local stringIntMap = staticMeshComp.TestStringIntMap
        print("   TestStringIntMap accessed (type: " .. type(stringIntMap) .. ")")

        -- 기본값 확인
        print("   Initial values:")
        print("     map['InitScore'] = " .. tostring(stringIntMap["InitScore"]))
        print("     map['InitHealth'] = " .. tostring(stringIntMap["InitHealth"]))
        print("     map['InitMana'] = " .. tostring(stringIntMap["InitMana"]))

        -- Map에 값 추가/수정
        stringIntMap["Score"] = 100
        stringIntMap["Health"] = 95
        stringIntMap["Level"] = 5
        print("   Modified values:")
        print("   Write map['Score'] = 100")
        print("   Write map['Health'] = 95")
        print("   Write map['Level'] = 5")

        -- Map 읽기 테스트
        print("   Read map['Score'] = " .. tostring(stringIntMap["Score"]))
        print("   Read map['Health'] = " .. tostring(stringIntMap["Health"]))
        print("   Read map['Level'] = " .. tostring(stringIntMap["Level"]))
    end

    -- Test 8: TMap<int32, float> 기본값 확인 및 읽기/쓰기
    print("\n[Test 8] TMap<int32, float> Default Values & Access")
    if staticMeshComp then
        local intFloatMap = staticMeshComp.TestIntFloatMap
        print("   TestIntFloatMap accessed (type: " .. type(intFloatMap) .. ")")

        -- 기본값 확인
        print("   Initial values:")
        print("     map[0] = " .. tostring(intFloatMap[0]))
        print("     map[1] = " .. tostring(intFloatMap[1]))
        print("     map[2] = " .. tostring(intFloatMap[2]))

        -- 값 수정 및 추가
        intFloatMap[1] = 1.5
        intFloatMap[2] = 2.7
        intFloatMap[10] = 3.14159
        print("   Modified values:")
        print("   map[1] = " .. tostring(intFloatMap[1]))
        print("   map[2] = " .. tostring(intFloatMap[2]))
        print("   map[10] = " .. tostring(intFloatMap[10]))
    end

    -- Test 9: TMap<FString, FVector> 기본값 확인 및 읽기/쓰기
    print("\n[Test 9] TMap<FString, FVector> Default Values & Access")
    if staticMeshComp then
        local stringVectorMap = staticMeshComp.TestStringVectorMap
        print("   TestStringVectorMap accessed (type: " .. type(stringVectorMap) .. ")")

        -- 기본값 확인
        print("   Initial values:")
        local initPos = stringVectorMap["InitPosition"]
        local initVel = stringVectorMap["InitVelocity"]
        print("     map['InitPosition'] = (" .. initPos.X .. ", " .. initPos.Y .. ", " .. initPos.Z .. ")")
        print("     map['InitVelocity'] = (" .. initVel.X .. ", " .. initVel.Y .. ", " .. initVel.Z .. ")")

        -- 값 수정
        stringVectorMap["Position"] = Vector(10.0, 20.0, 30.0)
        stringVectorMap["Velocity"] = Vector(1.0, 0.0, 0.0)

        print("   Modified values:")
        local pos = stringVectorMap["Position"]
        local vel = stringVectorMap["Velocity"]
        print("   map['Position'] = (" .. pos.X .. ", " .. pos.Y .. ", " .. pos.Z .. ")")
        print("   map['Velocity'] = (" .. vel.X .. ", " .. vel.Y .. ", " .. vel.Z .. ")")
    end

    -- Test 10: nil 할당으로 Map 키 삭제 테스트
    print("\n[Test 10] Map Key Removal (nil assignment)")
    if staticMeshComp then
        local stringIntMap = staticMeshComp.TestStringIntMap
        print("   Before removal: map['Score'] = " .. tostring(stringIntMap["Score"]))

        -- nil 할당으로 키 삭제
        stringIntMap["Score"] = nil
        print("   Assigned nil to map['Score']")
        print("   After removal: map['Score'] = " .. tostring(stringIntMap["Score"]))
    end

    -- Test 11: 배열 순회 테스트
    print("\n[Test 11] Array Iteration")
    if staticMeshComp then
        local stringArray = staticMeshComp.TestStringArray
        print("   Iterating through TestStringArray:")
        for i = 1, #stringArray do
            print("    [" .. i .. "] = " .. stringArray[i])
        end
    end

    -- Test 12: TArray<UTexture*> 테스트
    print("\n[Test 12] TArray<UTexture*> Access")
    if staticMeshComp then
        local textureArray = staticMeshComp.TestTextureArray
        print("   TestTextureArray accessed (type: " .. type(textureArray) .. ")")
        print("   Initial length: " .. #textureArray)

        -- 빈 배열이므로 nil 체크
        if #textureArray == 0 then
            print("   Array is empty (no textures loaded)")
        else
            print("   Array contains " .. #textureArray .. " textures")
            for i = 1, #textureArray do
                local tex = textureArray[i]
                print("     [" .. i .. "] = " .. tostring(tex))
            end
        end
    end

    -- Test 13: TArray<UMaterialInterface*> 테스트
    print("\n[Test 13] TArray<UMaterialInterface*> Access")
    if staticMeshComp then
        local materialArray = staticMeshComp.TestMaterialArray
        print("   TestMaterialArray accessed (type: " .. type(materialArray) .. ")")
        print("   Initial length: " .. #materialArray)

        -- 빈 배열이므로 nil 체크
        if #materialArray == 0 then
            print("   Array is empty (no materials loaded)")
        else
            print("   Array contains " .. #materialArray .. " materials")
            for i = 1, #materialArray do
                local mat = materialArray[i]
                print("     [" .. i .. "] = " .. tostring(mat))
            end
        end
    end

    -- Test 14: TMap<FString, UTexture*> 테스트
    print("\n[Test 14] TMap<FString, UTexture*> Access")
    if staticMeshComp then
        local stringTextureMap = staticMeshComp.TestStringTextureMap
        print("   TestStringTextureMap accessed (type: " .. type(stringTextureMap) .. ")")

        -- Map에 nil 체크
        local testKey = stringTextureMap["TestTexture"]
        print("   map['TestTexture'] = " .. tostring(testKey) .. " (expected nil)")

        -- TODO: 실제 텍스처를 로드하여 테스트하려면 LoadTexture 같은 함수가 필요
        print("   Note: UObject* container tests require resource loading functions")
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
