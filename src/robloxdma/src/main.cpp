#include <globals.h>
#include <thread>
#include <chrono>
#include <sdk/sdk.h>
#include <sdk/offsets/offsets.h>
#include <game/game.h>
#include <cache/cache.h>
#include <cheats/aimbot/aimbot.h>
#include <cheats/triggerbot/triggerbot.h>
#include <settings/settings.h>
#include <settings/config.h>
#include <makcu/makcu.h>
#include <dma_helper.h>
#include "VolkDMA/dma.hh"
#include "VolkDMA/process.hh"

std::unique_ptr<DMA> g_dma;
std::unique_ptr<Process> g_process;

auto main() -> std::int32_t {
	SetConsoleTitleA("Spotify");
	logger->log<info>("waiting for roblox...");

	g_dma = std::make_unique<DMA>(true);
	if (!g_dma->handle) {
		logger->log<info>("DMA init failed");
		sleep(5000);
		return 1;
	}
	g_process = std::make_unique<Process>(*g_dma, BINARY_NAME);
	DWORD pid = g_dma->get_process_id(BINARY_NAME);
	if (pid == 0) {
		logger->log<info>("roblox not found");
		sleep(5000);
		return 1;
	}
	set_dma_and_process(g_dma.get(), g_process.get());

	logger->log<info>("found roblox with pid %lu", pid);

	{
		logger->log<info>("loading config...");
		config::load();
	}

	if (!g_process->fix_cr3(BINARY_NAME)) {
		logger->log<info>("CR3 fix failed, continuing anyway...");
	}

	base = static_cast<std::uintptr_t>(g_process->get_base_address(BINARY_NAME));
	size = static_cast<std::uintptr_t>(g_process->get_size(BINARY_NAME));

	logger->log<info>("found main module address 0x%llx", base);

	{
		roblox_window = FindWindowW(0, L"Roblox");

		logger->log<info>("[INIT] Reading FakeDataModel from offset 0x%llx...", Offsets::FakeDataModel::Pointer);
		std::uint64_t fake_datamodel = 0;
		std::uint64_t real_datamodel = 0;
		std::uint64_t visualengine = 0;
		std::uint64_t gameid = 0;
		std::uint64_t placeid = 0;
		VMMDLL_SCATTER_HANDLE init_scatter = g_process->create_scatter(0);
		if (init_scatter)
		{
			g_process->add_read_scatter(init_scatter, base + Offsets::FakeDataModel::Pointer, &fake_datamodel, sizeof(fake_datamodel));
			g_process->execute_scatter(init_scatter);
			logger->log<info>("[INIT] FakeDataModel @ 0x%llx", fake_datamodel);
			if (fake_datamodel != 0 && fake_datamodel > 0x10000)
			{
				g_process->add_read_scatter(init_scatter, fake_datamodel + Offsets::FakeDataModel::RealDataModel, &real_datamodel, sizeof(real_datamodel));
				g_process->add_read_scatter(init_scatter, base + Offsets::VisualEngine::Pointer, &visualengine, sizeof(visualengine));
				g_process->execute_scatter(init_scatter);
				logger->log<info>("[INIT] RealDataModel @ 0x%llx", real_datamodel);
				logger->log<info>("[INIT] VisualEngine @ 0x%llx", visualengine);
			}
			else
			{
				logger->log<error>("[INIT] CRITICAL: FakeDataModel is NULL or invalid!");
				logger->log<error>("[INIT] Offsets may be outdated. Roblox might have updated.");
				logger->log<error>("[INIT] Base address: 0x%llx", base);
				logger->log<error>("[INIT] Trying to continue anyway...");
			}
			game::datamodel = { real_datamodel };
			game::visualengine = { visualengine };
			logger->log<debug>("datamodel @ 0x%llx", game::datamodel.address);
			logger->log<debug>("visengine @ 0x%llx", game::visualengine.address);
			if (game::datamodel.address > 0x10000)
			{
				g_process->add_read_scatter(init_scatter, game::datamodel.address + Offsets::DataModel::GameId, &gameid, sizeof(gameid));
				g_process->add_read_scatter(init_scatter, game::datamodel.address + Offsets::DataModel::PlaceId, &placeid, sizeof(placeid));
				g_process->execute_scatter(init_scatter);
				game::gameid = gameid;
				game::placeid = placeid;
			}
			else
			{
				logger->log<error>("[INIT] Cannot read game_id/place_id - datamodel is invalid");
			}
			g_process->close_scatter(init_scatter);
		}
		else
		{
			logger->log<error>("[INIT] Failed to create scatter for init");
		}
		if (game::datamodel.address <= 0x10000)
			game::gameid = 0, game::placeid = 0;

		logger->log<debug>("game id  -> %llu", game::gameid);
		logger->log<debug>("place id -> %llu", game::placeid);

		if (game::datamodel.address > 0x10000)
		{
			if (game::is_rivals() || settings::game::force_rivals)
			{
				game::find_rivals_datamodel_children();
				if (game::players_service.address != 0 || game::workspace.address != 0)
					logger->log<info>("[RIVALS] FindFirstChildOfClass OK");
			}
			else
			{
				game::players_service = game::datamodel.find_first_child("Players");
				game::workspace = game::datamodel.find_first_child("Workspace");
			}
			if (game::players_service.address == 0 || game::workspace.address == 0)
			{
				game::find_rivals_datamodel_children();
				if (game::players_service.address != 0 || game::workspace.address != 0) {
					settings::game::force_rivals = true;
					config::save();
					logger->log<info>("[RIVALS] Normal returned 0x0, RIVALS worked - saved force_rivals");
				}
			}
			if (game::workspace.address == 0 && game::datamodel.address > 0x10000)
			{
				std::uint64_t workspace_direct = g_process->read<std::uint64_t>(game::datamodel.address + Offsets::DataModel::Workspace);
				if (workspace_direct > 0x10000)
				{
					game::workspace = rbx::instance_t(workspace_direct);
					logger->log<info>("[INIT] Workspace from DataModel direct offset");
				}
			}
			logger->log<debug>("players  -> 0x%llx", game::players_service.address);
			logger->log<debug>("workspace -> 0x%llx", game::workspace.address);
		}
		else
		{
			logger->log<error>("[INIT] Cannot find Players/Workspace - datamodel is invalid");
			logger->log<error>("[INIT] ESP will not work until offsets are updated!");
		}
	}

	{
		makcu::auto_connect();
		if (makcu::is_connected())
			logger->log<info>("MAKCU connected on %s", settings::aimbot::com_port);
		else
			logger->log<info>("MAKCU not found - connect USB 2 to this PC, click Find MAKCU in menu");
	}

	{
		if (!render->create_window())
		{
			v2::exit("failed to create overlay window.");
		}

		if (!render->create_device())
		{
			v2::exit("failed to create directx device.");
		}

		if (!render->create_imgui())
		{
			v2::exit("failed to create imgui.");
		}
	}

	{
		std::thread(cache::run).detach();
	}

	static auto last_makcu_retry = std::chrono::steady_clock::now();
	static auto last_rivals_retry = std::chrono::steady_clock::now();
	while (true)
	{
		auto now = std::chrono::steady_clock::now();
		if (!makcu::is_connected() &&
		    std::chrono::duration_cast<std::chrono::seconds>(now - last_makcu_retry).count() >= 3) {
			makcu::auto_connect();
			last_makcu_retry = now;
		}
		if (game::players_service.address == 0 && game::datamodel.address > 0x10000 &&
		    std::chrono::duration_cast<std::chrono::seconds>(now - last_rivals_retry).count() >= 5) {
			std::uint64_t placeid_now = g_process->read<std::uint64_t>(game::datamodel.address + Offsets::DataModel::PlaceId);
			game::placeid = placeid_now;
			if (game::is_rivals() || settings::game::force_rivals) {
				game::find_rivals_datamodel_children();
			}
			else {
				game::players_service = game::datamodel.find_first_child("Players");
				game::workspace = game::datamodel.find_first_child("Workspace");
			}
			if (game::players_service.address == 0 || game::workspace.address == 0) {
				game::find_rivals_datamodel_children();
				if (game::players_service.address != 0 || game::workspace.address != 0) {
					settings::game::force_rivals = true;
					config::save();
				}
			}
			if (game::workspace.address == 0) {
				std::uint64_t ws = g_process->read<std::uint64_t>(game::datamodel.address + Offsets::DataModel::Workspace);
				if (ws > 0x10000) game::workspace = rbx::instance_t(ws);
			}
			last_rivals_retry = now;
		}
		render->start_render();
		aimbot::run();
		triggerbot::run();
		render->render_visuals();
		if (render->running)
		{
			render->render_menu();
		}
		render->end_render();
	}

	config::save();
	v2::exit("graceful exit, goodbye.");
}
