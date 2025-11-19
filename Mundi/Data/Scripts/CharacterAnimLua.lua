--[[
    CharacterAnimLua.lua

    Lua 기반 애니메이션 그래프 예시

    Unreal Engine AnimBlueprint의 Lua 버전:
    - NativeConstruct: 그래프 구성 (에디터의 AnimGraph 탭)
    - NativeUpdate: 변수 업데이트 (에디터의 EventGraph 탭)

    구조:

    [RootNode: BlendTwoWay]
         ├─ InputA: IdlePlayer (Sequence: Idle)
         └─ InputB: WalkPlayer (Sequence: Walk)
         Alpha: Speed → 0.0 (Idle) ~ 1.0 (Walk)

    확장 가능한 부분:
    - BlendSpace 추가 (다방향 이동)
    - StateMachine 통합 (Idle/Walk/Run 상태)
    - Layered Blend (상체/하체 분리)
]]

-- ========================================
-- 클래스 선언
-- ========================================

CharacterAnimLua = CharacterAnimLua or {}
CharacterAnimLua.__index = CharacterAnimLua

-- ========================================
-- Initialization (그래프 구성)
-- ========================================

--[[
    NativeConstruct는 UAnimInstance::InitializeAnimation() 후 호출

    이 시점에서 수행할 작업:
    1. 애니메이션 에셋 로드
    2. 노드 생성 (CreateNode)
    3. 노드 연결 (InputA, InputB 등)
    4. 루트 노드 설정 (SetRootNode)

    주의:
    - Skeleton 정보는 아직 유효함 (NativeInitializeAnimation 완료 후)
    - 노드 Initialize는 자동 호출됨 (InitializeNodeRecursive)
    - 여러 번 호출될 수 있음 (스켈레탈 메시 교체 시)
]]
function CharacterAnimLua:NativeConstruct()
    print("[CharacterAnimLua] NativeConstruct called")

    -- 1. 애니메이션 에셋 로드 (경로는 프로젝트에 맞게 수정)
    --    Unreal의 Asset Registry 역할
    self.IdleAnimation = self:FindAnimationByName("Idle")
    self.WalkAnimation = self:FindAnimationByName("Walking")

    if not self.IdleAnimation then
        print("[CharacterAnimLua] ERROR: Failed to load Idle animation")
        return
    end

    if not self.WalkAnimation then
        print("[CharacterAnimLua] ERROR: Failed to load Walk animation")
        return
    end

    print("[CharacterAnimLua] Loaded animations:")
    print("  - Idle: " .. tostring(self.IdleAnimation))
    print("  - Walk: " .. tostring(self.WalkAnimation))

    -- 2. Sequence Player 노드 생성 (Idle)
    --    Unreal의 FAnimNode_SequencePlayer 역할
    self.IdlePlayer = self:CreateNode("UAnimNode_SequencePlayer")
    if not self.IdlePlayer then
        print("[CharacterAnimLua] ERROR: Failed to create IdlePlayer node")
        return
    end

    self.IdlePlayer.Sequence = self.IdleAnimation
    self.IdlePlayer.bLooping = true
    self.IdlePlayer.PlayRate = 1.0

    print("[CharacterAnimLua] Created IdlePlayer:")
    print("  - Sequence: " .. tostring(self.IdlePlayer.Sequence))
    print("  - Looping: " .. tostring(self.IdlePlayer.bLooping))

    -- 3. Sequence Player 노드 생성 (Walk)
    self.WalkPlayer = self:CreateNode("UAnimNode_SequencePlayer")
    if not self.WalkPlayer then
        print("[CharacterAnimLua] ERROR: Failed to create WalkPlayer node")
        return
    end

    self.WalkPlayer.Sequence = self.WalkAnimation
    self.WalkPlayer.bLooping = true
    self.WalkPlayer.PlayRate = 1.0

    print("[CharacterAnimLua] Created WalkPlayer:")
    print("  - Sequence: " .. tostring(self.WalkPlayer.Sequence))

    -- 4. Blend 노드 생성
    --    Unreal의 FAnimNode_BlendListByBool 또는 FAnimNode_TwoWayBlend 역할
    self.BlendNode = self:CreateNode("UAnimNode_BlendTwoWay")
    if not self.BlendNode then
        print("[CharacterAnimLua] ERROR: Failed to create BlendNode")
        return
    end

    self.BlendNode.InputA = self.IdlePlayer
    self.BlendNode.InputB = self.WalkPlayer
    self.BlendNode.Alpha = 0.0  -- 초기값: 완전 Idle

    print("[CharacterAnimLua] Created BlendNode:")
    print("  - InputA: IdlePlayer")
    print("  - InputB: WalkPlayer")
    print("  - Alpha: " .. tostring(self.BlendNode.Alpha))

    -- 5. 루트 노드 설정 (평가 시작점)
    --    Unreal의 FAnimNode_Root 역할
    self:SetRootNode(self.BlendNode)

    print("[CharacterAnimLua] Root node set to BlendNode")
    print("[CharacterAnimLua] Graph construction complete (Total nodes: " ..
          self:GetNodeCount() .. ")")
end

-- ========================================
-- Update (매 프레임 변수 업데이트)
-- ========================================

--[[
    NativeUpdate는 매 프레임 호출 (TickComponent)

    이 시점에서 수행할 작업:
    1. 캐릭터 상태 읽기 (속도, 회전 등)
    2. 블렌딩 파라미터 계산
    3. 노드 프로퍼티 업데이트 (Alpha, PlayRate 등)

    주의:
    - 포즈 생성은 하지 않음 (Evaluate에서 수행)
    - 노드 Update()는 자동 호출됨 (NativeUpdateAnimation)
    - DeltaTime은 World의 DeltaSeconds
]]
function CharacterAnimLua:NativeUpdate(DeltaTime)
    -- 1. 캐릭터 속도 가져오기
    --    Unreal의 GetOwningComponent()->GetComponentVelocity() 역할
    local velocity = self:GetOwnerVelocity()
    if not velocity then
        -- 초기화 중이거나 컴포넌트가 없는 경우
        return
    end

    local speed = velocity:Size()

    -- 2. 블렌딩 Alpha 계산
    --    속도에 따라 Idle(0.0) → Walk(1.0) 전환
    --
    --    Unreal AnimBlueprint의 로직:
    --    - 0 ~ 100 units: Idle
    --    - 100 ~ 600 units: Walk
    --    - 600+ units: Run (여기서는 미구현)
    local maxWalkSpeed = 600.0
    local targetAlpha = math.min(speed / maxWalkSpeed, 1.0)

    -- 3. 부드러운 보간 (Alpha Interpolation)
    --    갑작스러운 전환 방지
    --
    --    Unreal의 FMath::FInterpTo 역할
    local interpSpeed = 5.0  -- 보간 속도 (높을수록 빠름)
    local currentAlpha = self.BlendAlpha or 0.0
    self.BlendAlpha = currentAlpha + (targetAlpha - currentAlpha) * DeltaTime * interpSpeed

    -- 4. Blend 노드에 Alpha 적용
    if self.BlendNode then
        self.BlendNode.Alpha = self.BlendAlpha
    end

    -- 디버깅 출력 (매 초마다)
    self.DebugTimer = (self.DebugTimer or 0.0) + DeltaTime
    if self.DebugTimer >= 1.0 then
        self.DebugTimer = 0.0
        print(string.format(
            "[CharacterAnimLua] Speed: %.1f, Alpha: %.2f (Target: %.2f)",
            speed, self.BlendAlpha, targetAlpha
        ))
    end
end

-- ========================================
-- Helper Functions (보조 함수)
-- ========================================

--[[
    애니메이션 이름으로 UAnimSequence 찾기

    TODO: 실제 구현은 프로젝트에 맞게 변경
    - Asset Manager에서 검색
    - SkeletalMeshComponent에 바인딩된 애니메이션 목록
    - 하드코딩된 경로 (프로토타입용)

    @param name 애니메이션 이름 (예: "Idle", "Walk")
    @return UAnimSequence 또는 nil
]]
function CharacterAnimLua:FindAnimationByName(name)
    -- 임시 구현: 하드코딩된 경로
    --
    -- 프로덕션에서는:
    -- 1. Asset Manager 사용
    -- 2. SkeletalMesh에 포함된 애니메이션 목록 조회
    -- 3. Data Table에서 로드

    local animPaths = {
        Idle = "AnimSequence'/Game/Characters/Mannequin/Animations/Idle.Idle'",
        Walking = "AnimSequence'/Game/Characters/Mannequin/Animations/Walk.Walk'",
        Running = "AnimSequence'/Game/Characters/Mannequin/Animations/Run.Run'",
    }

    local path = animPaths[name]
    if not path then
        print("[CharacterAnimLua] WARNING: No path defined for animation '" .. name .. "'")
        return nil
    end

    -- TODO: LoadAsset(path) 구현
    -- return LoadAsset(path)

    -- 임시: 더미 객체 반환
    return { Name = name, Path = path }
end

--[[
    캐릭터 속도 벡터 가져오기

    @return FVector 또는 nil
]]
function CharacterAnimLua:GetOwnerVelocity()
    -- Unreal의 GetOwningComponent()->GetComponentVelocity() 역할
    --
    -- TODO: C++에서 바인딩 필요
    -- - UAnimInstance::GetOwningComponent()
    -- - USceneComponent::GetComponentVelocity()

    -- 임시: 더미 속도 반환 (테스트용)
    self.TestSpeed = (self.TestSpeed or 0.0) + 10.0
    if self.TestSpeed > 700.0 then
        self.TestSpeed = 0.0
    end

    return {
        X = self.TestSpeed,
        Y = 0.0,
        Z = 0.0,
        Size = function(self)
            return math.sqrt(self.X * self.X + self.Y * self.Y + self.Z * self.Z)
        end
    }
end

-- ========================================
-- Advanced Example: State Machine (확장 예시)
-- ========================================

--[[
    State Machine 기반 그래프 구성

    사용하지 않는 예시 (참고용)
    NativeConstruct에서 BlendNode 대신 이 함수 호출 시 사용

    구조:

    [RootNode: StateMachine]
         ├─ State "Idle"       → SequencePlayer (Idle)
         ├─ State "Walk"       → SequencePlayer (Walk)
         └─ State "Run"        → SequencePlayer (Run)

         Transitions:
         - Idle → Walk: Speed > 100
         - Walk → Run:  Speed > 600
         - Run  → Walk: Speed < 550
         - Walk → Idle: Speed < 50
]]
function CharacterAnimLua:ConstructStateMachineGraph()
    print("[CharacterAnimLua] Constructing State Machine graph...")

    -- 1. State Machine 노드 생성
    self.StateMachine = self:CreateNode("UAnimStateMachine")
    if not self.StateMachine then
        print("[CharacterAnimLua] ERROR: Failed to create StateMachine node")
        return
    end

    -- 2. State 추가
    self.StateMachine:AddState("Idle", self.IdleAnimation, true, 1.0)
    self.StateMachine:AddState("Walk", self.WalkAnimation, true, 1.0)
    -- self.StateMachine:AddState("Run",  self.RunAnimation,  true, 1.0)

    -- 3. Transition 규칙 추가
    self.StateMachine:AddTransition("Idle", "Walk", 0.3)  -- 0.3초 블렌딩
    self.StateMachine:AddTransition("Walk", "Idle", 0.3)
    -- self.StateMachine:AddTransition("Walk", "Run",  0.2)
    -- self.StateMachine:AddTransition("Run",  "Walk", 0.2)

    -- 4. 초기 상태 설정
    self.StateMachine:SetInitialState("Idle")

    -- 5. 루트 노드 설정
    self:SetRootNode(self.StateMachine)

    print("[CharacterAnimLua] State Machine graph complete")
end

--[[
    State Machine 업데이트 (NativeUpdate에서 호출)

    자동 전환 조건 체크:
    - Idle → Walk: Speed > 100
    - Walk → Idle: Speed < 50
]]
function CharacterAnimLua:UpdateStateMachine(DeltaTime)
    if not self.StateMachine then
        return
    end

    local velocity = self:GetOwnerVelocity()
    if not velocity then
        return
    end

    local speed = velocity:Size()
    local currentState = self.StateMachine:GetCurrentState()

    -- 전환 조건 체크
    if currentState == "Idle" and speed > 100.0 then
        self.StateMachine:TransitionTo("Walk")
        print("[CharacterAnimLua] Transition: Idle → Walk")
    elseif currentState == "Walk" and speed < 50.0 then
        self.StateMachine:TransitionTo("Idle")
        print("[CharacterAnimLua] Transition: Walk → Idle")
    end
end

-- ========================================
-- Module Return
-- ========================================

return CharacterAnimLua
