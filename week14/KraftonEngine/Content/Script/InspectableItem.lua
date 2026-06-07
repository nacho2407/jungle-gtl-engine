local DEFAULT_ITEM_ID = "Ion_core_sample"

local function normalize_actor_name(name)
    if name == nil then return nil end
    local id = string.match(name, "^Item_(.+)$")
    return id or name
end

local function resolve_item_id()
    _G.ItemActorIds = _G.ItemActorIds or {}

    if obj ~= nil and obj.UUID ~= nil then
        local explicitId = _G.ItemActorIds[obj.UUID]
        if explicitId ~= nil and explicitId ~= "" then
            return explicitId
        end
    end

    if obj ~= nil then
        local idFromName = normalize_actor_name(obj.Name)
        if idFromName ~= nil and idFromName ~= "" then
            return idFromName
        end
    end

    return DEFAULT_ITEM_ID
end

function BeginPlay()
    _G.InspectableItems = _G.InspectableItems or {}

    if obj == nil or obj.UUID == nil then
        return
    end

    _G.InspectableItems[obj.UUID] = {
        actor = obj,
        item_id = resolve_item_id()
    }
end

function EndPlay()
    if obj ~= nil and obj.UUID ~= nil and _G.InspectableItems ~= nil then
        _G.InspectableItems[obj.UUID] = nil
    end
end
