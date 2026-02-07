#include <game/game.h>
#include <globals.h>
#include <sdk/offsets/offsets.h>
#include <dma_helper.h>
#include <sdk/sdk.h>
#include <settings/settings.h>
#include "VolkDMA/process.hh"

namespace game
{
	rbx::datamodel_t datamodel;
	rbx::visualengine_t visualengine;
	std::uint64_t gameid;
	std::uint64_t placeid;
	rbx::instance_t players_service;
	rbx::instance_t workspace;

	/* RIVALS: use FindFirstChildOfClass for DataModel children (Players, Workspace) */
	void find_rivals_datamodel_children()
	{
		if (datamodel.address == 0 || datamodel.address <= 0x10000)
			return;
		/* FindFirstChildOfClass on DataModel - try described datamodel first, then raw */
		std::uint64_t described = get_process()->read<std::uint64_t>(datamodel.address + Offsets::DataModel::DescribedDataModel);
		rbx::instance_t dm_desc((described > 0x10000 && described < 0x7FFFFFFFFFFF) ? described : (datamodel.address + Offsets::DataModel::DescribedDataModel));
		rbx::instance_t dm_raw(datamodel.address);
		if (players_service.address == 0) {
			std::uint64_t p = dm_desc.find_first_child_by_class("Players");
			if (!p) p = dm_raw.find_first_child_by_class("Players");
			players_service = rbx::instance_t(p);
		}
		if (workspace.address == 0) {
			std::uint64_t w = dm_desc.find_first_child_by_class("Workspace");
			if (!w) w = dm_raw.find_first_child_by_class("Workspace");
			workspace = rbx::instance_t(w);
		}
	}

	void refresh_datamodel()
	{
		Process* proc = get_process();
		if (!proc || base == 0)
			return;
		std::uint64_t fake_dm = proc->read<std::uint64_t>(base + Offsets::FakeDataModel::Pointer);
		if (fake_dm == 0 || fake_dm <= 0x10000)
			return;
		std::uint64_t real_dm = proc->read<std::uint64_t>(fake_dm + Offsets::FakeDataModel::RealDataModel);
		if (real_dm == 0 || real_dm <= 0x10000)
			return;
		datamodel = rbx::datamodel_t(real_dm);
		std::uint64_t vis = proc->read<std::uint64_t>(base + Offsets::VisualEngine::Pointer);
		visualengine = rbx::visualengine_t(vis);
		gameid = proc->read<std::uint64_t>(datamodel.address + Offsets::DataModel::GameId);
		placeid = proc->read<std::uint64_t>(datamodel.address + Offsets::DataModel::PlaceId);
		players_service = rbx::instance_t(0);
		workspace = rbx::instance_t(0);
		if (is_rivals() || settings::game::force_rivals)
			find_rivals_datamodel_children();
		else
		{
			players_service = datamodel.find_first_child("Players");
			workspace = datamodel.find_first_child("Workspace");
		}
		if (players_service.address == 0 || workspace.address == 0)
			find_rivals_datamodel_children();
		if (workspace.address == 0)
		{
			std::uint64_t ws = proc->read<std::uint64_t>(datamodel.address + Offsets::DataModel::Workspace);
			if (ws > 0x10000)
				workspace = rbx::instance_t(ws);
		}
	}
}