---@class AnimNode
local AnimNode = {}

---@class AnimLib
Anim = {}

---@return number
function Anim.get_owner_speed() end

---@return boolean
function Anim.is_owner_falling() end

---@param path string
---@param section? string
---@param rate? number
---@param blendIn? number
---@param slotName? string
function Anim.play_montage(path, section, rate, blendIn, slotName) end

---@param blendOut? number
---@param slotName? string
function Anim.stop_montage(blendOut, slotName) end

---@param slotName? string
---@return boolean
function Anim.is_montage_playing(slotName) end

---@param path string
---@param rate number
---@param loop boolean
---@return AnimNode
function Anim.create_sequence_player(path, rate, loop) end

---@param name string
---@param input AnimNode
---@return AnimNode
function Anim.create_slot(name, input) end

---@param root AnimNode
function Anim.set_root_node(root) end

---@return AnimNode
function Anim.create_ref_pose() end