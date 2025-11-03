-- Apple.lua

local LifeTime = 5.0 -- 사과 생존 시간 (초)

local function NormalizeCopy(V)
    local Out = Vector(V.X, V.Y, V.Z)
    Out:Normalize()
    return Out
end

function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    Obj.Tag = "apple"
    Obj.Velocity = Vector(1, 0, 0)
    Obj.bIsActive = true

    StartCoroutine(function()
        coroutine.yield("wait_time", LifeTime)
        DeleteObject(Obj)
        print("[Apple] Lifetime expired, deleted.")
    end)
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnBeginOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
    if OtherActor.Tag == "fireball" then
        -- print("[Apple] Hit Fireball! Resetting it.")
        local fireVel = OtherActor.Velocity
        if fireVel then
            -- 기본적으로 반대 방향으로 튕기기
            local reflected = Vector(-fireVel.X, -fireVel.Y, -fireVel.Z)

            -- 위쪽(+Z축 방향으로)으로 약간 더해줌
            local upwardBoost = 5.0  -- 위로 튀는 강도 (조정 가능)
            reflected.Z = reflected.Z + upwardBoost

            -- 감속 계수 적용 (속도가 너무 빠르면)
            reflected = reflected * 0.8

            OtherActor.Velocity = reflected
        end

        -- if GlobalConfig.ResetFireballs then
        --     GlobalConfig.ResetFireballs(OtherActor)
        -- end
    end
end

function OnEndOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function Tick(dt)

    local rc = GetComponent(Obj, "URotatingMovementComponent")
    if rc and Obj.Velocity then
        -- 사과는 로컬 y축 기준으로 회전,
        -- 플레이어가 바라보는 방향(즉, 투사체 진행방향)을 기준으로 회전축을 설정해줌
        local dir = NormalizeCopy(Obj.Velocity)
        local UpVector = Vector(0, 0, 1)
        local Axis = FVector.Cross(UpVector, dir)
        Axis:Normalize()

        rc.RotationRate = Axis * 500.0
    end

    Obj.Location = Obj.Location + Obj.Velocity * dt
    if not Obj.bIsActive then
        return 
    end
    --[[Obj:PrintLocation()]]--
    --[[print("[Tick] ")]]--
end