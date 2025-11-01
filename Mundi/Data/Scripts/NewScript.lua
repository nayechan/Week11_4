function BeginPlay()
    print("[BeginPlay]")
    -- NewObj = SpawnPrefab("Data/Prefabs/AAmbientLightActor_1.prefab")
end

function EndPlay()
    print("[EndPlay]")
end

function OnOverlap(OtherActor)
    OtherActor:PrintLocation();
end

function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
    Obj:PrintLocation()
end