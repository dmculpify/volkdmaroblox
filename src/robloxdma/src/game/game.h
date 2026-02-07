#pragma once
#include <sdk/sdk.h>

namespace game
{
	extern rbx::datamodel_t datamodel;
	extern rbx::visualengine_t visualengine;
	extern std::uint64_t gameid;
	extern std::uint64_t placeid;
	extern rbx::instance_t players_service;
	extern rbx::instance_t workspace;

	inline bool is_rivals() { return placeid == 17625359962ULL; }

	void find_rivals_datamodel_children();
	/** Re-read DataModel, Players, Workspace from memory. Call on entity refresh interval. */
	void refresh_datamodel();
}