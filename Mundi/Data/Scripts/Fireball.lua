function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
	Obj.Velocity.X = 3.0
    Obj.Scale.X = 10.0
    Obj.Scale.Y = 10.0
    Obj.Scale.Z = 10.0
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
    --[[Obj:PrintLocation()]]--
    --[[print("[Tick] ")]]--
end