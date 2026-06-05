local arms = nil
local weaponHudWidget = nil

local reloadFastAnimationPath = "Content/Data/FPSArm/Test1/Test1_Armature_Armature_Arms_FPS_Anim_Reload_Fast.uasset"

function BeginPlay()
    arms = obj:GetSkeletalMesh()

    weaponHudWidget = UI.CreateWidget("Content/UI/HUD/WeaponHUD.rml")
    if weaponHudWidget ~= nil then
        weaponHudWidget:AddToViewportZ(80)
    end
end

function EndPlay()
    if weaponHudWidget ~= nil and weaponHudWidget:IsInViewport() then
        weaponHudWidget:RemoveFromParent()
    end
    weaponHudWidget = nil
end

function Tick(dt)
    if arms == nil then return end

    if Input.GetKeyDown(Key.R) then
        arms:PlayAnimationByPath(reloadFastAnimationPath, false)
    end
end
