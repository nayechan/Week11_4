-- CharacterAnimBlendSpace.lua (BlendSpace Version)
-- Lua AnimGraphInstance example - uses BlendSpace for smooth Locomotion

-- ========================================
-- Load common utilities
-- ========================================
require(GDataDir .. "/Scripts/LuaUtils")

-- ========================================
-- Script-local variables
-- ========================================
local testTime = 0.0
local Vars = nil              -- Variables wrapper
local BlendSpaceNode = nil    -- BlendSpace Node (cached for access)

-- ========================================
-- Initialization (Node Graph Construction)
-- ========================================
function LuaInitializeAnimation()
    print("[Lua] CharacterAnimBlendSpace: Initializing...")

    -- ========================================
    -- Variables Setup (Gameplay Variables)
    -- ========================================
    if not self.Variables then
        print("[Lua][error] self.Variables is nil!")
        return
    end

    Vars = CreateVarsWrapper(self.Variables)
    if not Vars then
        print("[Lua][error] CreateVarsWrapper returned nil!")
        return
    end

    -- Initialize gameplay variables
    Vars["Speed"] = 0.0
    Vars["bIsInAir"] = false

    -- ========================================
    -- Load Animations
    -- ========================================
    local idleAnim = LoadAnimSequence(GDataDir .. "/Fbx/Idle.fbx")
    local walkAnim = LoadAnimSequence(GDataDir .. "/Fbx/Walking.fbx")
    local runAnim = LoadAnimSequence(GDataDir .. "/Fbx/Running.fbx")

    if not idleAnim then
        print("[Lua][error] Failed to load Idle animation")
        return
    end

    if not walkAnim then
        print("[Lua][error] Failed to load Walk animation")
        return
    end

    if not runAnim then
        print("[Lua][error] Failed to load Run animation")
        return
    end

    print("[Lua] Animations loaded: Idle, Walk, Run")

    -- ========================================
    -- Create BlendSpace Node
    -- ========================================
    -- self:CreateNodeByName() comes from UAnimGraphInstance (inherited)
    -- UFUNCTION(LuaBind) - uses reflection to create nodes by type name
    BlendSpaceNode = self:CreateNodeByName("UAnimNode_BlendSpace1D")

    if not BlendSpaceNode then
        print("[Lua][error] Failed to create UAnimNode_BlendSpace1D")
        return
    end

    print("[Lua] Created UAnimNode_BlendSpace1D node")

    -- ========================================
    -- Configure BlendSpace Samples
    -- ========================================
    -- AddSample(SampleValue, Animation)
    -- SampleValue 오름차순으로 자동 정렬됨
    BlendSpaceNode:AddSample(0.0, idleAnim)   -- Speed = 0.0 → Idle
    BlendSpaceNode:AddSample(5.0, walkAnim)   -- Speed = 5.0 → Walk
    BlendSpaceNode:AddSample(10.0, runAnim)   -- Speed = 10.0 → Run

    print("[Lua] BlendSpace samples added: Idle(0.0), Walk(5.0), Run(10.0)")

    -- Configure BlendSpace properties
    BlendSpaceNode.bLooping = true
    BlendSpaceNode.PlayRate = 1.0
    BlendSpaceNode.CurrentParameter = 0.0  -- 초기값 (Idle)
    BlendSpaceNode.bUsePhaseSync = true    -- Phase Synchronization (Unreal 스타일)

    print("[Lua] BlendSpace configured: bLooping=true, PlayRate=1.0, bUsePhaseSync=true")

    -- ========================================
    -- Set RootNode (Evaluation Entry Point)
    -- ========================================
    -- self.RootNode is UPROPERTY in UAnimGraphInstance
    self.RootNode = BlendSpaceNode

    print("[Lua] RootNode set to BlendSpaceNode")
    print("[Lua] CharacterAnimBlendSpace: Initialization complete!")
    print("[Lua] Expected behavior:")
    print("[Lua]   - Speed 0.0 -> Idle (100%%)")
    print("[Lua]   - Speed 3.0 -> Idle (40%%) + Walk (60%%)")
    print("[Lua]   - Speed 5.0 -> Walk (100%%)")
    print("[Lua]   - Speed 7.5 -> Walk (50%%) + Run (50%%)")
    print("[Lua]   - Speed 10.0 -> Run (100%%)")
end

-- ========================================
-- Update (called every frame)
-- ========================================
function LuaUpdateAnimation(deltaTime)
    -- Update gameplay variables
    UpdateMovementVariables(deltaTime)

    -- Update BlendSpace parameter
    -- CurrentParameter drives the blending
    if BlendSpaceNode then
        local speed = Vars["Speed"] or 0.0
        BlendSpaceNode.CurrentParameter = speed
    end

    -- NOTE: Node->Update() is automatically called by UAnimGraphInstance::NativeUpdateAnimation()
    -- UAnimNode_BlendSpace1D->Update() progresses all sample times
    -- Then Evaluate() blends based on CurrentParameter
end

-- ========================================
-- Movement Variables Update
-- ========================================
function UpdateMovementVariables(deltaTime)
    if not self.OwnerComponent then
        Vars["Speed"] = 0.0
        return
    end

    -- TODO: Replace with actual velocity from Actor
    -- local owner = self.OwnerComponent:GetOwner()
    -- if owner then
    --     local velocity = owner:GetVelocity()
    --     local speed2D = math.sqrt(velocity.X * velocity.X + velocity.Y * velocity.Y)
    --     Vars["Speed"] = speed2D / 50.0  -- 속도를 0-10 범위로 매핑 (50cm/s = 1 unit)
    -- end

    -- Test simulation (sine wave)
    -- Sine: -1.0 ~ +1.0 → 절댓값 → 0.0 ~ 1.0 → *10 → 0.0 ~ 10.0
    testTime = testTime + deltaTime / 10.0
    local sineValue = math.sin(testTime)
    Vars["Speed"] = math.abs(sineValue) * 10.0

    -- 속도 범위: 0.0 (Idle) ~ 10.0 (Run)
    -- BlendSpace가 자동으로 중간값 블렌딩:
    -- - 3.0 → Idle(40%) + Walk(60%)
    -- - 7.5 → Walk(50%) + Run(50%)

    Vars["bIsInAir"] = false
end

-- ========================================
-- ⭐ NOTE: LuaGetAnimationPose() is NOT defined!
-- ========================================
-- By omitting this function, ULuaAnimInstance::GetAnimationPose()
-- will call Super::GetAnimationPose() (UAnimGraphInstance)
-- → RootNode->Evaluate() is automatically called!
--
-- Node Graph Evaluation Flow:
-- 1. ULuaAnimInstance::GetAnimationPose()
-- 2.   if (LuaGetPoseFunc.valid()) → NOT VALID, skip
-- 3.   Super::GetAnimationPose() → UAnimGraphInstance::GetAnimationPose()
-- 4.     RootNode->Evaluate(OutPose) → UAnimNode_BlendSpace1D::Evaluate()
-- 5.       FindBlendSamples(CurrentParameter) → 두 샘플 찾기
-- 6.       BlendTwoPosesTogether(PoseA, PoseB, Alpha, OutPose)
-- ========================================

print("[Lua] CharacterAnimBlendSpace.lua loaded successfully")
