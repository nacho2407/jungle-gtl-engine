local HoverDescription = {}

local function set_text(widget, id, text)
    if widget == nil or id == nil or text == nil then return end
    widget:SetText(id, text)
end

local function apply_description(widget, ids, description)
    if description == nil then return end

    set_text(widget, ids.title, description.title)
    set_text(widget, ids.body, description.body)
    set_text(widget, ids.note, description.note)
end

function HoverDescription.Bind(widget, ids, defaultDescription, entries)
    if widget == nil or ids == nil or entries == nil then return end

    apply_description(widget, ids, defaultDescription)

    for _, entry in ipairs(entries) do
        if entry.id ~= nil then
            widget:bind_event(entry.id, "mouseover", function()
                apply_description(widget, ids, entry)
            end)

            widget:bind_event(entry.id, "mouseout", function()
                apply_description(widget, ids, defaultDescription)
            end)
        end
    end
end

return HoverDescription
