#include <globals.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <sdk/sdk.h>
#include <sdk/offsets/offsets.h>
#include <game/game.h>
#include <cache/cache.h>
#include <settings/settings.h>
#include <settings/config.h>
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

	if (!g_process->fix_cr3(BINARY_NAME)) {
		logger->log<info>("CR3 fix failed, continuing anyway...");
	}

	base = static_cast<std::uintptr_t>(g_process->get_base_address(BINARY_NAME));
	size = static_cast<std::uintptr_t>(g_process->get_size(BINARY_NAME));

	logger->log<info>("found main module address 0x%llx", base);

	{
		roblox_window = FindWindowW(0, L"Roblox");

		logger->log<info>("[INIT] Reading FakeDataModel from offset 0x%llx...", Offsets::FakeDataModel::Pointer);
		std::uint64_t fake_datamodel = g_process->read<std::uint64_t>(base + Offsets::FakeDataModel::Pointer);
		std::uint64_t real_datamodel = 0;
		std::uint64_t visualengine = 0;
		
		logger->log<info>("[INIT] FakeDataModel @ 0x%llx", fake_datamodel);
		
		if (fake_datamodel != 0 && fake_datamodel > 0x10000)
		{
			real_datamodel = g_process->read<std::uint64_t>(fake_datamodel + Offsets::FakeDataModel::RealDataModel);
			visualengine = g_process->read<std::uint64_t>(base + Offsets::VisualEngine::Pointer);
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
			game::gameid = g_process->read<std::uint64_t>(game::datamodel.address + Offsets::DataModel::GameId);
			game::placeid = g_process->read<std::uint64_t>(game::datamodel.address + Offsets::DataModel::PlaceId);
		}
		else
		{
			logger->log<error>("[INIT] Cannot read game_id/place_id - datamodel is invalid");
			game::gameid = 0;
			game::placeid = 0;
		}

		logger->log<debug>("game id  -> %llu", game::gameid);
		logger->log<debug>("place id -> %llu", game::placeid);

		if (game::datamodel.address > 0x10000)
		{
			game::players_service = game::datamodel.find_first_child("Players");
			logger->log<debug>("players  -> 0x%llx", game::players_service.address);
			
			game::workspace = game::datamodel.find_first_child("Workspace");
			logger->log<debug>("workspace -> 0x%llx", game::workspace.address);
		}
		else
		{
			logger->log<error>("[INIT] Cannot find Players/Workspace - datamodel is invalid");
			logger->log<error>("[INIT] ESP will not work until offsets are updated!");
		}
	}

	{
		logger->log<info>("loading config...");
		config::load();
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

	while (true)
	{
		render->start_render();
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
