function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    --local tile = SpawnPrefab("Data/Prefabs/NormalTile.prefab")
    GenerateGrid()
end

function GenerateGrid()
    math.randomseed(os.time())
    local possibleRotations = {0, 90, 180, 270}

    local tileSize = 2
    local gridWidth = 11
    local gridHeight = 11

    local offsetX = (gridWidth - 1) * tileSize / 2
    local offsetY = (gridHeight - 1) * tileSize / 2

    for y = 0, gridHeight - 1 do
        for x = 0, gridWidth - 1 do
            local tile = SpawnPrefab("Data/Prefabs/NormalTile.prefab")

            tile.Location = Vector(x * tileSize - offsetX, y * tileSize - offsetY, 0)

            -- local randomIndex = math.random(1, #possibleRotations)
            -- local randomAngleZ = possibleRotations[randomIndex]
            -- if tile.Rotation then
            --     tile.Rotation.Z = randomAngleZ
            -- end
        end
    end
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnOverlap(OtherActor)
    if OtherActor.Tag == "Fire"
        
    --[[Obj:PrintLocation()]]--
end

function Tick(dt)
    Obj.Rotation = Obj.Rotation + Obj.Velocity * dt
    -- print("Rotation" .. Obj.Rotation.X .. Obj.Rotation.Y .. Obj.Rotation.Z)
    -- Obj.Location = Obj.Location + Obj.Velocity * dt
end