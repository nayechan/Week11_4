function BeginPlay()
    print("[BeginPlay]")
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