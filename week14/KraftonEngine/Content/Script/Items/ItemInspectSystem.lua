local ItemDatabase = require("Items/ItemDatabase")

local INSPECT_RANGE = 5.0
local PANEL_PATH = "Content/UI/InGame/ItemInspectPanel.rml"
local PANEL_Z_ORDER = 95

local M = {}

local widget = nil
local focusedActor = nil
local focusedItemId = nil
local activeActor = nil
local activeItemId = nil
local panelOpen = false

local function get_item_data(itemId)
    if itemId == nil then return nil end
    return ItemDatabase.items[itemId]
end

local function set_display(id, display)
    if widget == nil then return end
    widget:SetProperty(id, "display", display)
end

local function set_actor_outline(actor, enabled)
    if actor == nil or type(actor.SetOutline) ~= "function" then return end
    actor:SetOutline(enabled)
end

local function clear_focus()
    if focusedActor ~= nil then
        set_actor_outline(focusedActor, false)
    end
    focusedActor = nil
    focusedItemId = nil
    if not panelOpen then
        set_display("item-interact-hint", "none")
    end
end

local function set_focus(actor, itemId)
    if focusedActor == actor and focusedItemId == itemId then
        return
    end

    if focusedActor ~= nil then
        set_actor_outline(focusedActor, false)
    end

    focusedActor = actor
    focusedItemId = itemId

    if focusedActor ~= nil then
        set_actor_outline(focusedActor, true)
    end

    if not panelOpen then
        set_display("item-interact-hint", focusedActor ~= nil and "block" or "none")
    end
end

local function ensure_widget()
    if widget ~= nil then return end

    widget = UI.CreateWidget(PANEL_PATH)
    if widget == nil then return end
    widget:SetWantsMouse(false)
    widget:AddToViewportZ(PANEL_Z_ORDER)
    set_display("item-interact-hint", "none")
    set_display("item-inspect-card", "none")
end

local function show_panel(itemId)
    ensure_widget()
    if widget == nil then return end

    local data = get_item_data(itemId)
    if data == nil then return end

    activeActor = focusedActor
    activeItemId = itemId
    widget:SetText("item-inspect-title", data.name or itemId)
    widget:SetText("item-inspect-subtitle", data.subtitle or "")
    widget:SetText("item-inspect-description", data.description or "")
    widget:SetText("item-inspect-footer", data.pickup == true and "E PICK UP" or "E CLOSE")
    widget:SetAttribute("item-inspect-image", "src", data.image or ItemDatabase.default_image or "../Item/IonCoreSample.png")

    if ScoreManager ~= nil and ScoreManager.AddItemInspected ~= nil then
        ScoreManager.AddItemInspected(1)
    end

    panelOpen = true
    set_display("item-interact-hint", "none")
    set_display("item-inspect-card", "block")
end

local function close_panel()
    panelOpen = false
    activeActor = nil
    activeItemId = nil
    set_display("item-inspect-card", "none")
    set_display("item-interact-hint", focusedActor ~= nil and "block" or "none")
end

local function pickup_active_item()
    local actor = activeActor
    local itemId = activeItemId

    close_panel()
    if actor ~= nil and actor.UUID ~= nil and _G.InspectableItems ~= nil then
        _G.InspectableItems[actor.UUID] = nil
    end
    if itemId ~= nil then
        _G.PickedUpItems = _G.PickedUpItems or {}
        _G.PickedUpItems[itemId] = true
    end
    if actor ~= nil and type(actor.SetOutline) == "function" then
        actor:SetOutline(false)
    end
    if actor ~= nil and type(actor.Destroy) == "function" then
        actor:Destroy()
    end
    focusedActor = nil
    focusedItemId = nil
    set_display("item-interact-hint", "none")
end

local function is_interact_pressed()
    return Input.GetKeyDown(Key.E)
        or (Key.GamepadX ~= nil and Input.GetKeyDown(Key.GamepadX))
end

local function get_registry_entry(actor)
    if actor == nil or actor.UUID == nil then return nil end
    if _G.InspectableItems == nil then return nil end
    return _G.InspectableItems[actor.UUID]
end

local function trace_interactable(camPos, camFwd, owner)
    if World.RaycastWorldStatic ~= nil then
        local hit = World.RaycastWorldStatic(camPos, camFwd, INSPECT_RANGE, owner)
        if hit ~= nil
            and hit.HitActor ~= nil
            and hit.HitActor ~= owner
            and get_registry_entry(hit.HitActor) ~= nil then
            return hit
        end
    end

    return World.RaycastPrimitive(camPos, camFwd, INSPECT_RANGE, owner)
end

local function update_focus(camera, owner)
    if camera == nil then
        clear_focus()
        return
    end

    local camPos = camera:GetWorldLocation()
    local camFwd = camera.Forward
    local hit = trace_interactable(camPos, camFwd, owner)
    if hit == nil or hit.HitActor == nil or hit.HitActor == owner then
        clear_focus()
        return
    end

    local entry = get_registry_entry(hit.HitActor)
    if entry == nil or get_item_data(entry.item_id) == nil then
        clear_focus()
        return
    end

    set_focus(hit.HitActor, entry.item_id)
end

function M.Initialize()
    ensure_widget()
end

function M.Shutdown()
    clear_focus()
    panelOpen = false
    activeActor = nil
    activeItemId = nil

    if widget ~= nil and widget:IsInViewport() then
        widget:RemoveFromParent()
    end
    widget = nil
end

function M.Tick(camera, owner)
    ensure_widget()
    if widget == nil then return false end

    update_focus(camera, owner)

    if is_interact_pressed() then
        if panelOpen then
            local data = get_item_data(activeItemId)
            if data ~= nil and data.pickup == true then
                pickup_active_item()
            else
                close_panel()
            end
            return true
        elseif focusedItemId ~= nil then
            show_panel(focusedItemId)
            return true
        end
    end

    return false
end

function M.IsOpen()
    return panelOpen
end

return M
