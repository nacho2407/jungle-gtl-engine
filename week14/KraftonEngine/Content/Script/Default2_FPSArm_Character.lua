local arms = nil

local reloadFastAnimationPath = "Content/Data/FPSArm/Test1/Test1_Armature_Armature_Arms_FPS_Anim_Reload_Fast.uasset"

function BeginPlay()
    arms = obj:GetSkeletalMesh()
end

function EndPlay()
end

function Tick(dt)
    if arms == nil then return end

    if Input.GetKeyDown(Key.R) then
        arms:PlayAnimationByPath(reloadFastAnimationPath, false)
    end
end
