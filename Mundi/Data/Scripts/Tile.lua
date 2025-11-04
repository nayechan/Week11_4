function BeginPlay()
    
end

function EndPlay()
    
end

function OnBeginOverlap(OtherActor)
    if OtherActor.Tag == "fireball" then
        if GlobalConfig and GlobalConfig.RemoveTileByUUID then
            GlobalConfig.RemoveTileByUUID(Obj.UUID)
        end
    end
end

function OnEndOverlap(OtherActor)
end
function Tick(dt) 
    Obj.Location = Obj.Location + Obj.Velocity * dt * 0.01
end