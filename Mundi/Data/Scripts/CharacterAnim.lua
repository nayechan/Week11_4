-- CharacterAnim.lua (Node Graph Version)
-- Lua AnimGraphInstance example - uses node-based animation system

-- ========================================
-- Load common utilities
-- ========================================
require(GDataDir .. "/Scripts/LuaUtils")

-- ========================================
-- Script-local variables
-- ========================================
local testTime = 0.0
local Vars = nil           -- Variables wrapper
local LowerBodySMNode = nil  -- StateMachine Node (cached for access)

-- ========================================
-- Initialization (Node Graph Construction)
-- ========================================
function LuaInitializeAnimation()
    print("[Lua] CharacterAnim (Node Graph): Initializing...")

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
    -- Create StateMachine Node (Node Wrapper)
    -- ========================================
    -- self:CreateNodeByName() comes from UAnimGraphInstance (inherited)
    -- UFUNCTION(LuaBind) - uses reflection to create nodes by type name
    LowerBodySMNode = self:CreateNodeByName("UAnimNode_StateMachine")

    if not LowerBodySMNode then
        print("[Lua][error] Failed to create UAnimNode_StateMachine")
        return
    end

    print("[Lua] Created UAnimNode_StateMachine node")

    -- ========================================
    -- Create Lua StateMachine (Adaptee with Lua Script)
    -- ========================================
    -- ULuaAnimStateMachine loads external Lua script for logic
    -- NewObject with self:Get() as Outer (for GC chain)
    local sm = NewObject("ULuaAnimStateMachine", self:Get())

    if not sm then
        print("[Lua][error] Failed to create ULuaAnimStateMachine")
        return
    end

    print("[Lua] Created ULuaAnimStateMachine")

    -- Set Lua script path (CharacterSM.lua handles States/Transitions/Logic)
    sm.LuaScriptPath = GDataDir .. "/Scripts/CharacterSM.lua"

    print("[Lua] Set LuaScriptPath: " .. sm.LuaScriptPath)

    -- Initialize StateMachine
    -- This will call LuaSetup() in CharacterSM.lua
    -- which adds States, Transitions, and sets initial state
    sm:Initialize()

    print("[Lua] StateMachine initialized (LuaSetup called in CharacterSM.lua)")

    -- ========================================
    -- Connect StateMachine to Node (Adapter Pattern)
    -- ========================================
    LowerBodySMNode.StateMachine = sm

    -- ========================================
    -- Set RootNode (Evaluation Entry Point)
    -- ========================================
    -- self.RootNode is UPROPERTY in UAnimGraphInstance
    self.RootNode = LowerBodySMNode

    print("[Lua] RootNode set to LowerBodySMNode")
    print("[Lua] CharacterAnim (Node Graph): Initialization complete!")
end

-- ========================================
-- Update (called every frame)
-- ========================================
function LuaUpdateAnimation(deltaTime)
    -- Update gameplay variables only
    -- Transition logic is handled by CharacterSM.lua (LuaProcessState)
    UpdateMovementVariables(deltaTime)

    -- NOTE: Node->Update() is automatically called by UAnimGraphInstance::NativeUpdateAnimation()
    -- UAnimNode_StateMachine->Update() calls StateMachine->Update()
    -- ULuaAnimStateMachine::Update() calls ProcessState()
    -- ProcessState() calls LuaProcessState() in CharacterSM.lua
    -- → CharacterSM.lua reads Variables and calls TransitionTo()
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
    --     Vars["Speed"] = velocity:Length()
    -- end

    -- Test simulation (sine wave)
    testTime = testTime + deltaTime
    Vars["Speed"] = 200.0 * math.sin(testTime)

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
-- 4.     RootNode->Evaluate(OutPose) → UAnimNode_StateMachine::Evaluate()
-- 5.       StateMachine->GetBlendedPose(OutPose)
-- ========================================

print("[Lua] CharacterAnim.lua (Node Graph) loaded successfully")
