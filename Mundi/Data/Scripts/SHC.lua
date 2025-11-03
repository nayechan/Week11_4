-- Fireball이 생성되는 위치를 정해주는 스크립트입니다
-- 현재는 X,Y의 바운드를 정할 수 있고, Z는 FireballArea Actor에서 가져옵니다.
local SpawnInterval = 1
local TimeAcc = 0.0

-- Rotation: oscillate around Z from initial rotation
local RotAccum = 0.0
local RotSpeed = 1.0          -- radians per second
local RotAmplitudeDeg = 1.0  -- +/- degrees
local BaseRot = nil           -- will store Obj.Rotation at BeginPlay

function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID) 
    --Obj.Scale = Vector(10, 10, 1)
    GlobalConfig.SpawnAreaPos = Obj.Location
    BaseRot = Obj.Rotation

    FireballSpawner = SpawnPrefab("Data/Prefabs/FireballSpawnArea.prefab") 
    FireballSpawner.Location = Obj.Location
    
    -- Orient spawner to look along Owner's local +X axis
    GlobalConfig.SpawnerForwardByUUID = GlobalConfig.SpawnerForwardByUUID or {}
    do
        local rot = Obj.Rotation
        local rz = math.rad(rot.Z)
        local ry = math.rad(rot.Y)
        local rx = math.rad(rot.X)
        -- Rotation matrix R = Rz * Ry * Rx, apply to local right (1,0,0)
        local cz, sz = math.cos(rz), math.sin(rz)
        local cy, sy = math.cos(ry), math.sin(ry)
        -- First column of R (world Right for local +X)
        local rightX = cz * cy
        local rightY = sz * cy
        local rightZ = -sy
        GlobalConfig.SpawnerForwardByUUID[FireballSpawner.UUID] = Vector(rightX, rightY, rightZ)
    end
    --RightEye = SpawnPrefab("Data/Prefabs/FireballSpawnArea.prefab") 
    
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
    -- Update Owner rotation: keep X,Y from base, oscillate Z in [-45,45]
    RotAccum = RotAccum + dt
    local angleDeg = RotAmplitudeDeg * math.sin(RotSpeed * RotAccum)
    local rot = BaseRot or Obj.Rotation
    rot.Z = (BaseRot and BaseRot.Z or rot.Z) + angleDeg
    Obj.Rotation = rot

    -- Keep spawner's forward aligned to Owner's local +X each frame
    if FireballSpawner and GlobalConfig then
        GlobalConfig.SpawnerForwardByUUID = GlobalConfig.SpawnerForwardByUUID or {}

        local rz = math.rad(rot.Z)
        local ry = math.rad(rot.Y)
        local rx = math.rad(rot.X)
        local cz, sz = math.cos(rz), math.sin(rz)
        local cy, sy = math.cos(ry), math.sin(ry)
        -- First column of R (world Right for local +X)
        local rightX = cz * cy
        local rightY = sz * cy
        local rightZ = -sy
        GlobalConfig.SpawnerForwardByUUID[FireballSpawner.UUID] = Vector(rightX, rightY, rightZ)
    end

   --[[print("[Tick] ")]]--
end
