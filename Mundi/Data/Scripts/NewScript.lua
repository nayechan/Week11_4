function BeginPlay()
    print("[BeginPlay] ")
    Obj:PrintLocation()
end

function EndPlay()
    print("[EndPlay] ")
    Obj:PrintLocation()
end

function OnOverlap(OtherActor)
    OtherActor:PrintLocation();
end

function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
    Obj:PrintLocation()
end