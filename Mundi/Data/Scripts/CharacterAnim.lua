-- CharacterAnim.lua
-- Lua AnimInstance example - mimics CharacterAnimInstance.cpp

-- ========================================
-- Load common utilities
-- ========================================
require(GDataDir .. "/Scripts/LuaUtils")

-- ========================================
-- Script-local helper variables
-- ========================================
local testTime = 0.0       -- Test simulation timer
local Vars = nil           -- Variables wrapper (local to this script)
local StateMachine = nil   -- StateMachine instance (local to this script)

-- ========================================
-- Note: Gameplay variables pattern (String-based TMap)
-- ========================================
-- C++ TMap<FString, FString> Variables (저장 보장)
-- CreateVarsWrapper()로 metatable 생성 (자동 타입 변환)
--
-- 사용법:
-- self.Vars = CreateVarsWrapper(self.Variables)
-- self.Vars["Speed"] = 300.0  --> 자동으로 "300.0" 저장
-- local speed = self.Vars["Speed"]  --> 자동으로 300.0 반환
--
-- 중요: Vars는 로컬 wrapper이므로 다른 스크립트에서는 다시 생성 필요
-- ========================================

-- ========================================
-- Initialization
-- ========================================
function LuaInitializeAnimation()
    print("[Lua] CharacterAnim: Initializing...")

    -- ========================================
    -- Gameplay variables with auto type conversion
    -- ========================================
    -- self.Variables는 C++ TMap<FString, FString> (UPROPERTY)
    -- CreateVarsWrapper()로 metatable wrapper 생성 (공통 helper)

    -- DEBUG: Variables 확인
    print("[Lua] self.Variables type: " .. type(self.Variables))
    if self.Variables then
        print("[Lua] self.Variables exists!")

        -- Test direct TMap access
        local testStatus, testErr = pcall(function()
            self.Variables["TestKey"] = "TestValue"
            local val = self.Variables["TestKey"]
        end)

        if not testStatus then
            print("[Lua][error] Direct TMap access failed: " .. tostring(testErr))
        end
    else
        print("[Lua][error] self.Variables is nil!")
        return
    end

    -- DEBUG: CreateVarsWrapper 함수 확인
    print("[Lua] CreateVarsWrapper type: " .. type(CreateVarsWrapper))
    print("[Lua] _G.CreateVarsWrapper type: " .. type(_G.CreateVarsWrapper))

    if not CreateVarsWrapper then
        print("[Lua][error] CreateVarsWrapper function not found!")
        -- Try global
        if _G.CreateVarsWrapper then
            print("[Lua] Found in _G, using that")
            CreateVarsWrapper = _G.CreateVarsWrapper
        else
            return
        end
    end

    -- pcall로 에러 캐치
    print("[Lua] About to call CreateVarsWrapper...")
    local status, result = pcall(function()
        print("[Lua] Inside pcall, calling CreateVarsWrapper")
        local ret = CreateVarsWrapper(self.Variables)
        print("[Lua] CreateVarsWrapper returned, type: " .. type(ret))
        return ret
    end)

    print("[Lua] pcall status: " .. tostring(status))
    print("[Lua] pcall result type: " .. type(result))

    if not status then
        print("[Lua][error] CreateVarsWrapper call failed: " .. tostring(result))
        return
    end

    -- ⭐ self는 LuaObjectProxy라서 런타임 필드 추가 불가
    -- 로컬 변수로 저장
    Vars = result

    -- DEBUG: Vars 생성 결과 확인
    print("[Lua] Vars type: " .. type(Vars))
    if not Vars then
        print("[Lua][error] CreateVarsWrapper returned nil!")
        return
    end

    -- Initialize variables
    Vars["Speed"] = 0.0  -- Auto converted to "0.0"
    Vars["bIsInAir"] = false  -- Auto converted to "false"
    -- 런타임에 자유롭게 추가 가능:
    -- Vars["bIsCombatMode"] = false
    -- Vars["Health"] = 100.0

    -- ========================================
    -- ⭐ 모듈화된 StateMachine 생성 (ULuaAnimStateMachine 사용)
    -- ========================================

    -- Create Lua-controlled StateMachine (로컬 변수로 저장)
    -- Outer 체인: StateMachine → AnimInstance → SkeletalMeshComponent → Actor → World
    -- self:Get()으로 LuaObjectProxy에서 UObject* 추출
    StateMachine = NewObject("ULuaAnimStateMachine", self:Get())

    if not StateMachine then
        print("[Lua][error] Failed to create ULuaAnimStateMachine")
        return
    end

    -- DEBUG: 메서드 확인
    print("[Lua] StateMachine type: " .. type(StateMachine))
    print("[Lua] StateMachine.Initialize type: " .. type(StateMachine.Initialize))
    print("[Lua] StateMachine.Update type: " .. type(StateMachine.Update))

    -- Lua 스크립트 경로 설정 (C++ 패턴과 동일: GDataDir 기반)
    local scriptPath = GDataDir .. "/Scripts/CharacterSM.lua"
    print("[Lua] Setting StateMachine.LuaScriptPath to: " .. scriptPath)
    StateMachine.LuaScriptPath = scriptPath

    -- DEBUG: 설정 후 바로 읽어서 확인
    print("[Lua] Verifying StateMachine.LuaScriptPath = " .. tostring(StateMachine.LuaScriptPath))

    -- StateMachine 초기화
    -- LuaSetup()에서 GetOuter() → AnimInstance → GetWorld() → LuaManager 접근
    if StateMachine.Initialize then
        print("[Lua] Calling StateMachine:Initialize()...")
        StateMachine:Initialize()
        print("[Lua] StateMachine:Initialize() returned")
    else
        print("[Lua][error] Initialize method not found!")
    end

    print("[Lua] CharacterAnim: Initialization complete!")
end

-- ========================================
-- Update (called every frame)
-- ========================================
function LuaUpdateAnimation(deltaTime)
    -- 1. Update gameplay variables (AnimInstance owns them)
    UpdateMovementVariables()

    -- 2. Update state machine (time + transitions)
    -- StateMachine's LuaProcessState() reads AnimInstance's variables via GetOuter()
    if StateMachine then
        StateMachine:Update(deltaTime)
    end
end

-- Update movement-related variables (uses Vars wrapper)
function UpdateMovementVariables()
    if not self.OwnerComponent then
        Vars["Speed"] = 0.0  -- Auto converted to "0.0"
        return
    end

    -- Get actor velocity
    -- TODO: Actor Lua 바인딩 추가되면 실제 Velocity 사용
    -- local owner = self.OwnerComponent:GetOwner()
    -- if owner then
    --     local velocity = owner:GetVelocity()
    --     Vars["Speed"] = velocity:Length()
    -- else
    --     Vars["Speed"] = 0.0
    -- end

    -- 테스트용 시뮬레이션 (sin wave로 Speed 변화)
    testTime = testTime + 0.001
    Vars["Speed"] = 250.0 * math.sin(testTime) - 100.0  -- Auto converted

    -- TODO: 점프/낙하 감지
    Vars["bIsInAir"] = false  -- Auto converted to "false"
end

-- ========================================
-- Note: Transition logic is in CharacterSM.lua (LuaProcessState)
-- AnimInstance owns Variables (C++ TMap) and Vars wrapper (Lua metatable)
-- StateMachine reads anim.Vars["Speed"] via GetOuter() and decides transitions
-- String-based storage + metatable으로 유연성과 저장 보장
-- ========================================

-- ========================================
-- Pose Evaluation
-- ========================================
function LuaGetAnimationPose(outPose)
    if not StateMachine then
        print("[Lua][warning] StateMachine is nil, returning empty pose")
        return
    end

    -- Get blended pose from state machine
    StateMachine:GetBlendedPose(outPose)
end

-- ========================================
-- Optional: Debug info
-- ========================================
function GetCurrentStateName()
    if self.StateMachine then
        return self.StateMachine:GetCurrentState()
    end
    return "None"
end

function IsTransitioning()
    if self.StateMachine then
        return self.StateMachine:IsTransitioning()
    end
    return false
end

print("[Lua] CharacterAnim.lua loaded successfully")
