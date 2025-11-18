-- CharacterSM.lua
-- Character Animation State Machine (모듈화된 버전)

-- ========================================
-- Load common utilities
-- ========================================
require(GDataDir .. "/Scripts/LuaUtils")

-- ========================================
-- Setup: States와 Transitions 정의
-- ========================================
function LuaSetup()
    print("[Lua] CharacterSM: Initializing State Machine...")

    -- 애니메이션 로드 (C++ 패턴과 동일: GDataDir 기반)
    local idleAnim = LoadAnimSequence(GDataDir .. "/Fbx/Idle.fbx")
    local walkAnim = LoadAnimSequence(GDataDir .. "/Fbx/Walking.fbx")
    local runAnim = LoadAnimSequence(GDataDir .. "/Fbx/Running.fbx")  -- TODO: Run 애니메이션 추가

    if not idleAnim then
        print("[Lua][error] CharacterSM: Failed to load Idle animation")
        return
    end

    if not walkAnim then
        print("[Lua][error] CharacterSM: Failed to load Walk animation")
        return
    end

    -- States 추가 (self = ULuaAnimStateMachine)
    -- NOTE: FName 생성자를 명시적으로 호출
    self:AddState(FName("Idle"), idleAnim, true, 1.0)
    self:AddState(FName("Walk"), walkAnim, true, 1.0)
    self:AddState(FName("Run"), runAnim, true, 1.5)  -- Run은 1.5배 빠르게

    -- Transitions 추가
    self:AddTransition(FName("Idle"), FName("Walk"), 3.0)
    self:AddTransition(FName("Walk"), FName("Run"), 3.0)
    self:AddTransition(FName("Run"), FName("Idle"), 3.0)
    self:AddTransition(FName("Walk"), FName("Idle"), 3.0)
    self:AddTransition(FName("Run"), FName("Walk"), 3.0)

    -- 초기 상태 설정
    self:SetInitialState(FName("Idle"))

    print("[Lua] CharacterSM: State Machine setup complete!")
end

-- ========================================
-- Variables wrapper 캐싱 (성능 최적화)
-- ========================================
-- NOTE: VarsWrapper는 값을 저장하지 않고, C++ TMap에 대한 "프록시"입니다.
-- __index/__newindex가 매번 C++ TMap을 직접 읽고 쓰므로,
-- wrapper를 캐싱해도 항상 최신 값을 읽습니다.
--
-- 예시:
--   vars["Speed"] 읽기 → __index 호출 → anim.Variables["Speed"] 직접 읽기
--   vars["Speed"] = 100 → __newindex 호출 → anim.Variables["Speed"] 직접 쓰기
local VarsWrapper = nil

-- ========================================
-- ProcessState: 매 Update마다 호출 (자율적 전환 로직)
-- ========================================
function LuaProcessState()
    -- ⭐ StateMachine이 자율적으로 전환 결정
    -- Outer(AnimInstance)의 Variables를 읽어서 조건 평가

    -- GetOuter()는 UObject* 반환 → Cast로 ULuaAnimInstance로 변환
    -- Variables는 ULuaAnimInstance에 정의되어 있음
    local anim = Cast(self:GetOuter(), "ULuaAnimInstance")
    if not anim or not anim.Variables then
        return
    end

    -- 첫 호출 시에만 wrapper 생성 (metatable 객체 재생성 방지)
    if not VarsWrapper then
        VarsWrapper = CreateVarsWrapper(anim.Variables)
    end

    local vars = VarsWrapper  -- 프록시를 통해 C++ TMap 접근

    -- State transition logic based on AnimInstance's Variables
    -- CreateVarsWrapper로 자동 타입 변환됨
    local speed = vars["Speed"] or 0.0

    -- 상태 전환만 로그 (매 프레임 로그 방지)
    local currentStateName = self:GetCurrentState()
    local targetState = nil

    if speed >= 100.0 then
        targetState = "Run"
    elseif speed >= 0.1 then
        targetState = "Walk"
    else
        targetState = "Idle"
    end

    self:TransitionTo(FName(targetState))

    -- TODO: Jump 상태 추가 시
    -- if vars["bIsInAir"] then  -- Auto converted to boolean
    --     self:TransitionTo("Jump")
    -- end

    -- 런타임에 추가된 변수도 자유롭게 사용 가능:
    -- if vars["bIsCombatMode"] then ... end
end

print("[Lua] CharacterSM.lua loaded successfully")
