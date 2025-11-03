-- GameState
-- Init     : Tile, 동상, Player Spawn (각각의 Spawner에서 Tick 검사-> State는 PlayerSpawner만 바꾼다)
-- Playing
-- End

function BeginPlay()
    GlobalConfig.GameState = "Init"
end

function EndPlay()
    GlobalConfig.GameState = "End"
end

function OnBeginOverlap(OtherActor)
end

function OnEndOverlap(OtherActor)
end

function Tick(dt)
    --print(GlobalConfig.GameState)

    if GlobalConfig.GameState == "Init" then
        SpawnPrefab("Data/Prefabs/Player.prefab")
        GlobalConfig.PlayerState = "Alive"
        GlobalConfig.GameState = "Playing"

    elseif GlobalConfig.GameState == "Playing" then
        if GlobalConfig.PlayerState == "Dead" then
            GlobalConfig.GameState = "End"
        end

    elseif GlobalConfig.GameState == "End" then
        if InputManager:IsKeyDown("R") then
            GlobalConfig.GameState = "Init" 
        end
    end
end