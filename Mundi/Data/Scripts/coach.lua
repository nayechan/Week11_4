local maxTime = 25.0      -- 전체 색상 변화 시간 (초)
local whiteHoldTime = 5.0 -- 하얀색 유지 시간
local elapsedTime = 0.0
local meshComp = nil
GlobalConfig.CoachLevel = 1 -- (1: White, 2: Yellow, 3: Orange, 4: Red)
local prevCoachLevel = GlobalConfig.CoachLevel

-- 시간 비율에 따라 색상 보간 (하양 → 노랑 → 주황 → 빨강)
local function LerpColorByTime(ratio)
    local color = Color(1.0, 1.0, 1.0) -- 기본: 하얀색

    if ratio < 0.33 then
        -- 0~0.33: White → Yellow
        local t = ratio / 0.33
        color = Color(1.0, 1.0, 1.0 - t)
    elseif ratio < 0.66 then
        -- 0.33~0.66: Yellow → Orange
        local t = (ratio - 0.33) / 0.33
        color = Color(1.0, 1.0 - 0.5 * t, 0.0)
    else
        -- 0.66~1.0: Orange → Red
        local t = (ratio - 0.66) / 0.34
        color = Color(1.0, 0.5 - 0.5 * t, 0.0)
    end

    return color
end 
function BeginPlay()
    Obj.Tag = "Damageable"

    meshComp = GetComponent(Obj, "UStaticMeshComponent")
    if meshComp then
        meshComp:SetColor(0, "DiffuseColor", Color(1.0, 1.0, 1.0, 1.0)) -- 초기 하얀색
    end

    -- Expose audio component globally for other scripts
    GlobalConfig.AudioComp = GetComponent(Obj, "UAudioComponent")

    currentStage = 1
end

function EndPlay()
end

function Tick(dt)
    if GlobalConfig.GameState ~= "Playing" then
        return
    end

    if not meshComp then
        return
    end

    AudioComp = GetComponent(Obj, "UAudioComponent")
    -- CoachLevel이 1로 돌아오면 시간 및 색상 초기화
    if GlobalConfig.CoachLevel == 1 and prevCoachLevel ~= 1 then 
        elapsedTime = 0.0
        meshComp:SetColor(0, "DiffuseColor", Color(1.0, 1.0, 1.0)) -- 하얀색 복귀
        
        currentStage = 1
    end

    prevCoachLevel = GlobalConfig.CoachLevel

    -- 시간 경과에 따른 색상 변화
    elapsedTime = math.min(elapsedTime + dt, maxTime)

    if elapsedTime <= whiteHoldTime then
        meshComp:SetColor(0, "DiffuseColor", Color(1.0, 1.0, 1.0))
        
        return
    end

    local activeTime = elapsedTime - whiteHoldTime
    local colorChangeDuration = maxTime - whiteHoldTime
    local ratio = math.min(activeTime / colorChangeDuration, 1.0)
    local newColor = LerpColorByTime(ratio)
    meshComp:SetColor(0, "DiffuseColor", newColor)
 
    if firstCall then
        AudioComp:PlayOneShot(0)
        firstCall = false
    end
    if ratio >= 0.0 and ratio < 0.33 and GlobalConfig.CoachLevel < 2 then
        if AudioComp ~= nil then 
            AudioComp:PlayOneShot(1);
            print("Call 1")
        end
        GlobalConfig.CoachLevel = 2
    elseif ratio >= 0.33 and ratio < 0.66 and GlobalConfig.CoachLevel < 3 then
        if AudioComp ~= nil then 
            AudioComp:PlayOneShot(2);
            print("Call 2")
        end
        GlobalConfig.CoachLevel = 3 
    elseif ratio >= 0.66 and GlobalConfig.CoachLevel < 4 then  
        if AudioComp ~= nil then 
            AudioComp:PlayOneShot(3);
            print("Call 3")
        end
        GlobalConfig.CoachLevel = 4
    end
end
