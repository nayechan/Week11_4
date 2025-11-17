-- TestAnimation.lua
-- New API: AnimInstance 기반 애니메이션 시스템
--
-- Usage:
-- 1. Property Window에서 AnimClass를 선택 (e.g., UAnimSingleNodeInstance)
-- 2. AnimClass가 설정되면 BeginPlay에서 자동 생성됨
-- 3. Lua에서 AnimInstance에 애니메이션을 설정하고 재생

function BeginPlay()
    print("=== TestAnimation: BeginPlay (New API) ===")

    local SkeletalComp = GetComponent(Obj, "USkeletalMeshComponent")

    if not SkeletalComp then
        print("ERROR: No SkeletalMeshComponent found")
        return
    end

    print("Found SkeletalMeshComponent")

    -- AnimClass에서 자동 생성된 AnimInstance 확인
    local AnimInst = SkeletalComp.AnimInstance

    if not AnimInst then
        print("WARNING: No AnimInstance found!")
        print("  Please set AnimClass in Property Window:")
        print("  1. Select this Actor")
        print("  2. Find SkeletalMeshComponent")
        print("  3. Set AnimClass to 'UAnimSingleNodeInstance'")
        return
    end

    print("AnimInstance found: " .. tostring(AnimInst))

    -- AnimSequence를 Lua에서 직접 로드하거나 설정
    -- 예: ResourceManager에서 로드
    -- local WalkAnim = LoadAnimSequence("Data/Animations/Walk.fbx")
    -- AnimInst:SetAnimationAsset(WalkAnim)
    -- AnimInst:Play(true)

    print("Animation system ready!")
    print("  Note: Set animation via AnimInstance methods")
end

function Tick(DeltaTime)
    -- 필요시 애니메이션 상태 모니터링
end

function EndPlay()
    print("=== TestAnimation: EndPlay ===")
end
