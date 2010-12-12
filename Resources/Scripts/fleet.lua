-- Fleet class to guide AI behavior (e.g. imitate leader's travel route, accept orders as a group,
-- don't attack fellow escorts, etc.). The idea is that this should eventually be a cleaner way
-- of controlling AIs than tampering directly with AIData from outside.
--
-- Note: This is distinct from the Alliances in that Alliances are story-based, while Fleets are
-- simply practical tools to control how AIs behave. A mission with Alliance-independent AIs might
-- still want to put them in a Fleet.
--
-- Last updated: Dec 12, 2010
--
-- Current status: Partially working
--
-- To-do: 
--   - Make ai.lua pull values from Fleets rather than having them pushed into AIData
--   - Do garbage collection for stale fleets
--   - Make IDs get more promptly de-listed upon destruction, and watch out for consequences of sprite ID re-use
--   - More

Fleet = { }

--
-- constructor
--

function Fleet.create(_name)
	local self = {
		name = _name,
		members = { },
		leader = -1,
		state = nil,

		add = Fleet.add,
		remove = Fleet.remove,
		size = Fleet.size,
		hasBoth = Fleet.hasBoth,
		target = Fleet.target,
		hunt = Fleet.hunt,
		gateTravel = Fleet.gateTravel,
		formation = Fleet.formation,
		checkSprite = Fleet.checkSprite,
		isLeader = Fleet.isLeader,
		getLeader = Fleet.getLeader,
		getLeaderRoute = Fleet.getLeaderRoute
	}
	Fleets.list[_name] = self
	return self
end

--
-- instance functions
--

function Fleet.add(self, id, leader)
	if id == nil then return end
	-- keep track of which ships are in this fleet
	self.members[id] = true
	-- but also which fleet the new ship is a member of (for simplicity, only allow one fleet per ship)
	Fleets.shipFleet[id] = self.name
	-- each fleet may optionally have a leader, which is treated differently
	if leader then self.leader = id end
	return self
end

function Fleet.remove(self, id)
	self.members[id] = nil
	Fleets.shipFleet[id] = nil
	return self
end

function Fleet.size(self)
	return #self.members
end

function Fleet.hasBoth(self, first, second)
	return ( self.members[first] ~= nil and self.members[second] ~= nil )
end

function Fleet.target(self, t)
	print ("fleet " .. self.name .. " wants to target sprite " .. t)
	for n,id in pairs(self.members) do
		local sprite = self:checkSprite(id)
		if sprite ~= nil and AIData[id] ~= nil then
			AIData[id].target = t
		end
	end
	return self
end

function Fleet.hunt(self, t)
	--print ("fleet " .. self.name .. " wants to hunt sprite " .. t)
	--if t ~= nil then self:target(t) end
	for id,yes in pairs(self.members) do
		local sprite = self:checkSprite(id)
		if sprite ~= nil and AIData[id] ~= nil and Fleets:fleetmates(id, t) == false then
			setHuntHostile(id, t)
		end
	end
	self.state = "Hunting"
	return self
end

function Fleet.gateTravel(self, dest, route)
	for id,yes in pairs(self.members) do
		local sprite = self:checkSprite(id)
		if sprite ~= nil and AIData[id] ~= nil then
			AIData[id].destinationName = dest
			if self:isLeader(id) or self:getLeader() == -1 or route == nil then
				AIData[id].nextState = "ComputingRoute"
			elseif route ~= nil then
				AIData[id].Autopilot = APInit("AI", id)
				AIData[id].Autopilot.GateRoute = copy_table(route)
				AIData[id].nextState = "GateTravelling"
			else
				AIData[id].accompany = self:getLeader()
				-- "Waiting" is a lot like "Accompanying" except that it will automatically transition into "GateTravelling"
				AIData[id].nextState = "Waiting"
			end
		end
	end
	return self
end

function Fleet.formation(self)
	for id,yes in pairs(self.members) do
		local sprite = self:checkSprite(id)
		if sprite ~= nil and AIData[id] ~= nil then
			AIData[id].hostile = nil
			AIData[id].target = -1
			AIData[id].destination = -1
			AIData[id].destinationName = nil
			AIData[id].Autopilot = nil
			AIData[id].nextState = "default"
		end
	end
end

function Fleet.checkSprite(self, id)
	local sprite = Epiar.getSprite(id)
	if sprite == nil then
		self:remove(id)
	end
	return sprite
end

function Fleet.isLeader(self, id)
	return (self.leader == id)
end

function Fleet.getLeader(self, id)
	return self.leader
end

function Fleet.getLeaderRoute(self)
	if self:getLeader() == PLAYER:GetID() and Autopilot.spcr == nil then
		return Autopilot.GateRoute
	elseif AIData[self:getLeader()].Autopilot.spcr == nil then
		return AIData[self:getLeader()].Autopilot.GateRoute
	else
		return nil
	end
end

--
-- manager functions
--

Fleets = {
	list = { },
	shipFleet = { },

	-- usage: if( Fleets:fleetmates( id1, id2 ) ) then ... end
	fleetmates = function(self, first, second)
		for name, fleet in pairs(self.list) do
			if fleet:hasBoth(first, second) then return true end
		end
		return false
	end,

	getShipFleet = function(self, s)
		return Fleets:get(self.shipFleet[s])
	end,

	-- Fleets:join( first, second )
	--    If first is already a member of a fleet, add second to that one;
	--    Otherwise, create one, make first the leader, then add second to that one.
	-- Fleets:join( single )
	--    If single is already a member of a fleet, do nothing;
	--    Otherwise, create one and make single the leader.
	join = function(self, first, second)
		local f = Fleets:getShipFleet(first)
		if f == nil then
			f = Fleet.create( first .. " fleet " .. os.time() )
			f:add(first, true)
		end

		if second ~= nil then
			setAccompany(second, first)
			f:add(second)
		end
		return f
	end,

	unjoin = function(self, first, second)
		local f = Fleets:getShipFleet(first)
		if Fleets:getShipFleet(first) ~= nil then
			setAccompany(second, -1)
			f:remove(second)
		end
		return f
	end,

	get = function(self, f)
		return self.list[f]
	end,

	-- Don't use this function unless you want to refer to an individual fleet by a specific name.
	createOrGet = function(self, name)
		local f = self:get(name)
		if f == nil then
			return Fleet.create(name)
		else
			return f
		end
	end,

	cleanup = function(self)
		-- remove any empty fleets
	end
}


--
-- player functions
--

-- this function is currently for testing purposes only
function givePlayerEscorts()
	local X, Y
	if HUD.getTarget() < 0 then
		X, Y = PLAYER:GetPosition()
	else
		X, Y = Epiar.getSprite( HUD.getTarget() ):GetPosition()
	end
	local esc1 = Ship.new("escort 1",X - 150, Y, "Fleet Guard", "Ion Engines","Escort","Independent")
	local esc2 = Ship.new("escort 2",X + 150, Y, "Fleet Guard", "Ion Engines","Escort","Independent")
	Fleets:join(PLAYER:GetID(), esc1:GetID())
	Fleets:join(PLAYER:GetID(), esc2:GetID())
end

function playerFleetHunt()
	local f = Fleets:getShipFleet( PLAYER:GetID() )
	if f ~= nil then
		f:hunt( HUD.getTarget() )
	end
end

function playerFleetFormation()
	local f = Fleets:getShipFleet( PLAYER:GetID() )
	if f ~= nil then
		f:formation()
	end
end

--
-- utility functions
--

function copy_table(a)
	local b = { }
	for k,v in pairs(a) do
		b[k] = v
	end
	return b
end
