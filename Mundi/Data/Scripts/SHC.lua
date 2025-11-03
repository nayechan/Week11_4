-- Fireball이 생성되는 위치를 정해주는 스크립트입니다
-- 현재는 X,Y의 바운드를 정할 수 있고, Z는 FireballArea Actor에서 가져옵니다.
local SpawnInterval = 1
local TimeAcc = 0.0

function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID) 
    --Obj.Scale = Vector(10, 10, 1)
    GlobalConfig.SpawnAreaPos = Obj.Location

    LeftEye = SpawnPrefab("Data/Prefabs/FireballSpawnArea.prefab")
    RightEye = SpawnPrefab("Data/Prefabs/FireballSpawnArea.prefab") 
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnOverlap(OtherActor)
    -- Overlap hook if needed
end

function RandomInRange(MinRange, MaxRange)
    return MinRange + (MaxRange - MinRange) * math.random()
end

function Tick(dt)
    
     
   --[[print("[Tick] ")]]--
end
