-- TestAnimation.lua
-- PIE 실행 시 SkeletalMeshComponent의 AnimationData를 자동으로 루핑 재생

function BeginPlay()
    print("=== TestAnimation: BeginPlay ===")

    -- 이 Actor의 SkeletalMeshComponent 찾기
    local SkeletalComp = GetComponent(Obj, "USkeletalMeshComponent")

    if SkeletalComp then
        print("Found SkeletalMeshComponent")

        -- Property Window에서 설정한 AnimationData를 자동으로 사용
        -- (UObject* 타입은 Lua에서 직접 접근 불가하므로 인자 없는 PlayAnimation 호출)
        SkeletalComp:PlayAnimationDefault(true)

        print("Animation playback started (looping)")
        print("  If no animation plays, set AnimationData in Property Window")
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
