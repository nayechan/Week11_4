-- TestAnimation.lua
-- PIE 실행 시 SkeletalMeshComponent의 AnimationData를 자동으로 루핑 재생

function BeginPlay()
    print("=== TestAnimation: BeginPlay ===")

    -- 이 Actor의 SkeletalMeshComponent 찾기
    local SkeletalComp = GetComponent(Obj, "USkeletalMeshComponent")

    if SkeletalComp then
        print("Found SkeletalMeshComponent")

        -- Property Window에서 설정한 AnimationData 가져오기
        local AnimData = SkeletalComp.AnimationData

        if AnimData then
            print("AnimationData found, starting playback...")

            -- 애니메이션 재생 (루핑 true)
            SkeletalComp:PlayAnimation(AnimData, true)

            print("Animation playback started (looping)")
        else
            print("WARNING: No AnimationData selected!")
            print("  Please select an animation in Property Window:")
            print("  1. Select this Actor")
            print("  2. Find SkeletalMeshComponent")
            print("  3. Set AnimationData (e.g., ninave.fbx)")
        end
    else
        print("ERROR: No SkeletalMeshComponent found on this Actor")
    end
end

function Tick(DeltaTime)
    -- 필요시 재생 상태 모니터링 코드 추가
    -- 예: 현재 재생 시간 출력 등
end

function EndPlay()
    print("=== TestAnimation: EndPlay ===")
end
