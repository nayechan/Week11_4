-- GameState
-- Init     : Tile, 동상, Player Spawn (각각의 Spawner에서 Tick 검사-> State는 PlayerSpawner만 바꾼다)
-- Playing
-- End

function BeginPlay()
    GlobalConfig.GameState = "Init"
    GlobalConfig.UIManager = SpawnPrefab("Data/Prefabs/UIManager.prefab")
    InputManager:SetCursorVisible(true)
end

function EndPlay()
    InputManager:SetCursorVisible(true)
    GlobalConfig.GameState = "End"
end

function OnBeginOverlap(OtherActor)
end

function OnEndOverlap(OtherActor)
end

CurVisibilty = true
function Tick(dt)
    if GlobalConfig.GameState == "Init" then
        InputManager:SetCursorVisible(true)
        GlobalConfig.GameState = "Start"
        
    elseif GlobalConfig.GameState == "Start" then
        if InputManager:IsKeyDown("R") then
            SpawnPrefab("Data/Prefabs/Player.prefab")
            GlobalConfig.PlayerState = "Alive"
            GlobalConfig.GameState = "Playing" 
            InputManager:SetCursorVisible(false)
        end
        
    elseif GlobalConfig.GameState == "Playing" then
        if InputManager:IsKeyPressed("C") then
            SetCursorVisible(not CurVisibilty)
        end
        if GlobalConfig.PlayerState == "Dead" then
            SetCursorVisible(true)
            GlobalConfig.GameState = "End"
            StartCoroutine(WaitAndInit)
        end

    elseif GlobalConfig.GameState == "End" then
        InputManager:SetCursorVisible(true)
    end
end

function WaitAndInit()
    coroutine.yield("wait_time", 1.0)
    GlobalConfig.GameState = "Init"
end

function SetCursorVisible(Show)
    InputManager:SetCursorVisible(Show)
    CurVisibilty = Show
end
