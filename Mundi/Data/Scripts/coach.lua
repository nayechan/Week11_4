local maxTime = 28.0      -- 전체 색상 변화 시간 (초)
local whiteHoldTime = 8.0 -- 하얀색 유지 시간
local elapsedTime = 0.0
local meshComp = nil
GlobalConfig.CoachLevel = 1 -- (1: White, 2: Yellow, 3: Orange, 4: Red)

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

-- 색상 단계별로 데칼 소환
local function SpawnStageDecal(stage)
    local prefabPath = ""

    if stage == 2 then
        prefabPath = "Data/Prefabs/CrackDecal1.prefab"
    elseif stage == 3 then
        prefabPath = "Data/Prefabs/CrackDecal2.prefab"
    elseif stage == 4 then
        prefabPath = "Data/Prefabs/CrackDecal3.prefab"
    else
        return
    end

    local decal = SpawnPrefab(prefabPath)
    if decal then
        local decalComp = GetComponent(decal, "UDecalComponent")
        if decalComp then
            decalComp.FadeSpeed = 0
        end
 
    end
end

function BeginPlay() 
    Obj.Tag = "Damageable"

    meshComp = GetComponent(Obj, "UStaticMeshComponent")
    if meshComp then
        meshComp:SetColor(0, "DiffuseColor", Color(1.0, 1.0, 1.0)) -- 초기 하얀색
    end

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

    -- 색상 구간 진입 시 데칼 스폰
    if ratio >= 0.0 and ratio < 0.33 and GlobalConfig.CoachLevel < 2 then 
        SpawnStageDecal(2)
        GlobalConfig.CoachLevel = 2
    elseif ratio >= 0.33 and ratio < 0.66 and GlobalConfig.CoachLevel < 3 then 
        SpawnStageDecal(3)
        GlobalConfig.CoachLevel = 3
    elseif ratio >= 0.66 and GlobalConfig.CoachLevel < 4 then 
        SpawnStageDecal(4)
        GlobalConfig.CoachLevel = 4
    end
end
