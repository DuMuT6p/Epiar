/**\file			mission.cpp
 * \author			Matt Zweig (thezweig@gmail.com)
 * \date			Created: Tuesday, August 24, 2010
 * \date			Modified: Tuesday, August 24, 2010
 * \brief			
 * \details
 */

#include "includes.h"
#include "Engine/mission.h"
#include "Utilities/lua.h"
#include "Utilities/log.h"
#include "Utilities/components.h"

/**\class Mission
 * \brief A Goal for the Player to complete for rewards.
 * \details
 * Missions are partially implemented in Lua and in C++.
 * 
 * On the Lua side, a Mission is described by a MissionType which describes the
 * actions and a MissionTable which describes the mission specific information.
 * 
 * The MissionType is a structured table that contains the lua functions that
 * get run at key points in the Mission's life cycle.
 * Each Mission instance knows which MissionType table to use by the type variable.
 * Each MissionType table should have this form:
\verbatim
MissionType = {
	UID = 0, --- TODO: Use this for Mission Dependencies.
	Version = 1, --- Increment this whenever changes are made to the Mission.
	Author = "", --- The name of the first author.
	Difficulty = "EASY", --- TODO: Use this to warn Users about difficult Missions.
	Create = function() --- Call this when the Mission is created.
		--- Return a mission table.
		--- This is persistent and will be passed back to the action functions below.
		return defaultMissionTable( "Name", "Description" )
	end,
	Accept = function( missionTable ) end, --- Call this when the Mission is accepted.
	Reject = function( missionTable ) end, --- Call this when the Mission is rejected after being accepted.
	Update = function( missionTable ) --- Call this each time that the Mission should be checked.
		return nil --- Return nil when the mission isn't over yet.
		return true --- Return true when the mission has succeded.
		return false --- Return false when the mission has failed.
	end,
	Land = function( missionTable ) --- Call this each time that player lands
		return nil --- Return nil when the mission isn't over yet.
		return true --- Return true when the mission has succeded.
		return false --- Return false when the mission has failed.
	end,
	Success = function( missionTable ) end, --- Call this if the Mission is a Success.
	Failure = function( missionTable ) end, --- Call this if the Mission is a failure.
}
\endverbatim
 *
 * The MissionTable is more loosely described, but must contain a "Name" and "Description".
 * This table is generated by calling the MissionTable's Create function.
 * This table is sent as a message to each MissionTable function.
 * The MissionTable functions are free to modify this table as much as they want.
 * The Mission variable does not inspect this table except when saving the table to xml.
 * This table is stored in the current lua state and only retrieved PushMissionTable.
 *
 * The C++ Mission class mostly interfaces with the MissionTypes by sending it MissionTables.
 *
 * More information is found on the Epiar Wiki:
 * - http://epiar.net/trac/wiki/MissionCreation
 * - http://epiar.net/trac/wiki/MissionLifetime
 *
 * \see Resources/Scripts/missions.lua
 */

/**\brief Mission Constructor
 */
Mission::Mission( lua_State *_L, string _type, int _tableReference)
	:L(_L)
	,type(_type)
	,tableReference(_tableReference)
{
}

/**\brief Mission Destructor
 */
Mission::~Mission()
{
	luaL_unref(L, LUA_REGISTRYINDEX, tableReference);
}


bool Mission::ValidateMission( lua_State *L, string type, int tableReference, int expectedVersion ){
	int i;
	int currentVersion;
	
	// The Functions that every mission type must have
	const char *requiredFunctions[] = {
		"Create",
		"Accept",
		"Reject",
		"Update",
		"Land",
		"Success",
		"Failure",
	};
	const int NUM_FUNCTIONS = sizeof(requiredFunctions) / sizeof(requiredFunctions[0]);

	// The Attributes that every mission Table must have
	const char *requiredInformation[] = {
		"Name",
		"Description",
	};
	const int NUM_INFORMATION = sizeof(requiredInformation) / sizeof(requiredInformation[0]);

	const int initialStackTop = lua_gettop(L);

	// Check that this mission Type exists
	if( Mission::GetMissionType(L, type) != 1 ) {
		lua_settop(L, initialStackTop );
		return false;
	}

	// Check that this Mission defines each of the required functions
	for( i=0; i<NUM_FUNCTIONS; ++i )
	{
		lua_pushstring(L, requiredFunctions[i] );
		lua_gettable(L, initialStackTop + 1);
		if( ! lua_isfunction(L, lua_gettop(L)) )
		{
			LogMsg(ERR, "The Mission '%s' doesn't have a %s function!", type.c_str(), requiredFunctions[i] );
			Lua::stackDump(L);
			lua_settop(L, initialStackTop );
			return false;
		}
		lua_pop(L,1);
	}

	// Check that the Mission Table exists
	lua_rawgeti(L, LUA_REGISTRYINDEX, tableReference);
	if( ! lua_istable(L, lua_gettop(L)) )
	{
		LogMsg(ERR, "There is no Mission Table at reference '%d'!", tableReference );
		Lua::stackDump(L);
		lua_settop(L, initialStackTop );
		Lua::stackDump(L);
		return false;
	}

	// Check that the Mission Table has a Name and Description
	for( i=0; i<NUM_INFORMATION; ++i )
	{
		// Check that this Mission defines each of the required functions
		lua_pushstring(L, requiredInformation[i] );
		lua_gettable(L, initialStackTop + 2);
		if( ! lua_isstring(L, lua_gettop(L)) )
		{
			LogMsg(ERR, "The Mission '%s' doesn't have a %s!", type.c_str(), requiredInformation[i] );
			Lua::stackDump(L);
			lua_settop(L, initialStackTop );
			return false;
		} else {
			LogMsg(INFO, "The Mission %s: %s", requiredInformation[i], luaL_checkstring(L, lua_gettop(L)) );
		}
		lua_pop(L,1);
	}

	// Check the Version Number
	if( expectedVersion > 0 )
	{
		lua_pushstring(L, "Version" );
		lua_gettable(L, initialStackTop + 1);

		// Check that the Version is actually a Number
		if( ! lua_isnumber(L, lua_gettop(L)) )
		{
			lua_pushvalue(L, lua_gettop(L));
			LogMsg(ERR, "This '%s' Mission has a corrupted version '%s'.", type.c_str(), lua_tostring(L, lua_gettop(L)) );
			lua_settop(L, initialStackTop );
			return false;
		}

		// Check that the version numbers match
		currentVersion = lua_tonumber(L, lua_gettop(L));
		if( expectedVersion != currentVersion )
		{
			lua_settop(L, initialStackTop );
			LogMsg(ERR, "This '%s' Mission is from version %d rather than the current version %d.", type.c_str(), currentVersion, expectedVersion);
			return false;
		}
	}

	// Discard the entire stack.
	lua_settop(L, initialStackTop );
	return true;
}

/**\brief 
 * \returns True if there is an error and the Mission should be deleted.
 */
bool Mission::Reject()
{
	LogMsg(INFO, "Rejecting Mission '%s'", GetName().c_str());
	return RunFunction( "Reject", false );
}

/**\brief 
 * \returns True if there is an error and the Mission should be deleted.
 */
bool Mission::Accept()
{
	LogMsg(INFO, "Accepting Mission '%s'", GetName().c_str());
	return RunFunction( "Accept", false );
}

/**\brief 
 * \returns True if the Mission is over (success, failure, or error) and should be deleted.
 */
bool Mission::Update()
{
	return RunFunction( "Update", true );
}

/**\brief 
 * \returns True if the Mission is over (success, failure, or error) and should be deleted.
 */
bool Mission::Land( )
{
	return RunFunction( "Land", true );
}

void Mission::PushMissionTable()
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, tableReference);
}


int Mission::GetVersion()
{
	int version;
	const int initialStackTop = lua_gettop(L);

	// Get the Mission Type Table
	if( Mission::GetMissionType(L, type) != 1 ) {
		LogMsg(ERR, "The Mission '%s' is missing.", type.c_str() );
		lua_settop(L, initialStackTop );
		return 0;
	}

	// Get the value of "Version" from the Mission Type Table
	lua_pushstring(L, "Version" );
	lua_gettable(L, initialStackTop + 1);

	// Check that the Version is actually a number.
	if( ! lua_isnumber(L, lua_gettop(L)) )
	{
		lua_pushvalue(L, lua_gettop(L));
		LogMsg(ERR, "This '%s' Mission has a corrupted version '%s'.", type.c_str(), lua_tostring(L, lua_gettop(L)) );
		lua_settop(L, initialStackTop );
		return 0;
	}

	version = lua_tonumber(L, lua_gettop(L));

	lua_settop(L, initialStackTop);
	return version;
}

/**\brief
 * \returns true when this Mission is complete
 */
bool Mission::RunFunction(string functionName, bool checkCompletion)
{
	const int initialStackTop = lua_gettop(L);

	if( Mission::GetMissionType(L, type) != 1 ) {
		LogMsg(ERR, "Something bad happened?"); // TODO
		lua_settop(L, initialStackTop );
		return true;
	}

	// Get the function
	lua_pushstring(L, functionName.c_str() );
	lua_gettable(L,initialStackTop + 1);
	if( ! lua_isfunction(L,lua_gettop(L)) )
	{
		LogMsg(ERR, "The Mission Type named '%s' cannot %s!", type.c_str(), functionName.c_str() );
		lua_settop(L,initialStackTop);
		return true; // Invalid Mission, Delete it
	}

	lua_rawgeti(L, LUA_REGISTRYINDEX, tableReference);
	
	// Call the function
	if( lua_pcall(L, 1, LUA_MULTRET, 0) != 0)
	{
		LogMsg(ERR,"Failed to run %s.%s: %s\n", type.c_str(), functionName.c_str(), lua_tostring(L, -1));
		lua_settop(L,initialStackTop);
		return true; // Invalid Mission, Delete it
	}

	// When this flag is set, check if the function returned a true or false value.
	// If the return value is nil, then the Mission is not complete.
	if( checkCompletion && lua_isboolean(L, lua_gettop(L)) )
	{
		if( lua_toboolean(L, lua_gettop(L)) )
		{
			LogMsg(INFO, "The Mission '%s' is a Success", GetName().c_str() );
			RunFunction( "Success", false);
		}
		else
		{
			LogMsg(INFO, "The Mission '%s' is a Failure", GetName().c_str() );
			RunFunction( "Failure", false);
		}

		return true;
	}

	lua_settop(L, initialStackTop);
	return false;
}

/**\brief Lookup String Attributes from the Mission Table
 */
string Mission::GetStringAttribute( string attribute )
{
	int MissionTableIndex;
	string value = "";

	PushMissionTable();
	MissionTableIndex = lua_gettop(L);
	lua_pushstring( L, attribute.c_str() );
	lua_gettable(L,MissionTableIndex);
	if( lua_isstring(L, lua_gettop(L)) )
	{
		value = (string)luaL_checkstring(L, lua_gettop(L));
		printf("%s of this Mission: %s.\n", type.c_str(), value.c_str());
	} else {
		// TODO: Error!
	}
	lua_pop(L, 2); // Table and value

	return value;
}

Mission* Mission::FromXMLNode( xmlDocPtr doc, xmlNodePtr node )
{
	string type;
	int version = 0;;
	int missionTable;
	xmlNodePtr typeNode = FirstChildNamed(node,"type");
	xmlNodePtr versionNode = FirstChildNamed(node,"version");
	xmlNodePtr missionNode = FirstChildNamed(node,"value");

	type = NodeToString(doc, typeNode);
	if( versionNode != NULL ) {
		version = NodeToInt(doc, versionNode);
	}

	lua_State *L = Lua::CurrentState();

	// Turn the XML data into a Table
	Lua::ConvertFromXML(L, doc, missionNode);
	// pop the name off the top of the stack.  It should just be "missionTable" anyway.
	assert(lua_istable(L,lua_gettop(L)));
	// Gets and pops the top of the stack, which should have the the missionTable.
	missionTable = luaL_ref(L, LUA_REGISTRYINDEX); 

	assert(lua_isstring(L,lua_gettop(L)));
	lua_pop(L,1); // Pop the Name

	// Validate this Mission
	if( !Mission::ValidateMission(L, type, missionTable, version) ) {
		LogMsg(ERR, "Something important!");
		return NULL;
	}

	return new Mission(L, type, missionTable);
}

xmlNodePtr Mission::ToXMLNode()
{
	char buff[256];

	xmlNodePtr section = xmlNewNode(NULL, BAD_CAST "Mission" );

	xmlNewChild(section, NULL, BAD_CAST "type", BAD_CAST type.c_str() );

	snprintf(buff, sizeof(buff), "%d", GetVersion() );
	xmlNewChild(section, NULL, BAD_CAST "version", BAD_CAST buff );

	lua_State *L = Lua::CurrentState();

	lua_pushstring(L,"missionTable");
	PushMissionTable();

	xmlAddChild(section, Lua::ConvertToXML(L, lua_gettop(L), lua_gettop(L)-1) );

	lua_pop(L, 1); // Pop Table

	return section;
}

int Mission::GetMissionType( lua_State *L, string type )
{
	lua_getglobal(L, type.c_str() );
	if( ! lua_istable(L, lua_gettop(L)) )
	{
		LogMsg(ERR, "There is no Mission Type named '%s'.", type.c_str() );
		return 0;
	}
	return 1;
}

