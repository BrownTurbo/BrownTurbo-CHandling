// SDK
#include <plugin_sa.h>

#include <game_sa/CHandlingDataMgr.h>
#include <game_sa/CModelInfo.h>
#include <game_sa/CTxdStore.h>
#include <game_sa/CVisibilityPlugins.h>
#include <game_sa/rw/rpworld.h>
#include <shared/game/CVector.h>

#include <windows.h>
#include <iostream>

#include <sampapi/CChat.h>
#include <sampapi/CNetGame.h>
#include <sampapi/CVehiclePool.h>
#include <sampapi/sampapi.h>

#include "utils.h"

#include <RakHook/rakhook.hpp>
#include <RakHook/samp.hpp>
#include <RakNet/BitStream.h>
#include <RakNet/PacketEnumerations.h>
#include <RakNet/StringCompressor.h>

#include <d3d9.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "defs.h"
#include "handling_manager.hpp"

#include "assetdownloader.hpp"
#include "audioextender.hpp"
#include "binaryrwparser.hpp"
#include "streamingextender.hpp"

#include "ImGuiOverlay.h"

namespace fs = std::filesystem;
using namespace plugin;

class CustomVehiclesASI {
private:
	struct PendingCustomVehicle {
		CustomVehicleDef def;
		CVehicleModelInfo* modelInfo = nullptr;
		std::vector<uint8_t> txd;
		std::vector<uint8_t> dff;
		bool dffReady = false;
		bool txdReady = false;
		bool colReady = true;
	};

	std::unique_ptr<AssetDownloader> m_pipeline;
	std::queue<std::shared_ptr<PendingCustomVehicle>> m_completedQueue;
	std::mutex m_queueMutex;
	std::mutex m_pendingDefMutex;
	std::queue<std::shared_ptr<PendingCustomVehicle>> m_pendingDefQueue;
	std::atomic<bool> m_pendingClearAll { false };
	std::queue<uint32_t> m_destructionQueue;
	std::mutex m_destructionMutex;

	void CreateModelAndBeginDownloads(std::shared_ptr<PendingCustomVehicle> pending)
	{
		pending->modelInfo = StreamingExtender::CreateCustomModel(pending->def);
		if (!pending->modelInfo) {
			return; // nothing to attach a clump to later - don't bother downloading
		}

		AssetDownloadTask txdTask;
		txdTask.modelId = static_cast<int32_t>(pending->def.customModelId);
		txdTask.url = pending->def.txdUrl;
		txdTask.expectedSha256 = pending->def.txdHash;
		txdTask.localCachePath = fs::path("models") / (std::to_string(pending->def.customModelId) + "_txd.bin");
		txdTask.onComplete = [this, pending](DownloadResult res) {
			if (!res.Success())
				return;
			pending->txd = std::move(res.data);

			AssetDownloadTask dffTask;
			dffTask.modelId = static_cast<int32_t>(pending->def.customModelId);
			dffTask.url = pending->def.dffUrl;
			dffTask.expectedSha256 = pending->def.dffHash;
			dffTask.localCachePath = fs::path("models") / (std::to_string(pending->def.customModelId) + "_dff.bin");
			dffTask.onComplete = [this, pending](DownloadResult dffRes) {
				if (!dffRes.Success())
					return;
				pending->dff = BinaryRwParser::ExtractClump(dffRes.data);
				if (pending->dff.empty())
					return;

				{
					std::lock_guard<std::mutex> lock(m_queueMutex);
					m_completedQueue.push(pending);
				}
			};
			m_pipeline->Enqueue(std::move(dffTask));
		};
		m_pipeline->Enqueue(std::move(txdTask));
	}

	void FinalizeCustomVehicle(std::shared_ptr<PendingCustomVehicle> pending)
	{
		CVehicleModelInfo* newModel = pending->modelInfo;

		int txdSlot = CTxdStore::AddTxdSlot(std::format("custom_veh_{}", pending->def.customModelId).c_str());

		RwMemory txdMem { pending->txd.data(), static_cast<RwUInt32>(pending->txd.size()) };
		RwStream* txdStream = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &txdMem);

		if (!txdStream) {
			CTxdStore::RemoveTxdSlot(txdSlot);
			SendMsg(0xFF0000, std::format("[CustomVeh] Failed to open TXD stream for model {}", pending->def.customModelId).c_str());
			return;
		}

		if (!CTxdStore::LoadTxd(txdSlot, txdStream)) {
			RwStreamClose(txdStream, nullptr);
			CTxdStore::RemoveTxdSlot(txdSlot);
			SendMsg(0xFF0000, std::format("[CustomVeh] Failed to load TXD for model {}", pending->def.customModelId).c_str());
			return;
		}

		RwStreamClose(txdStream, nullptr);

		CTxdStore::AddRef(txdSlot);
		newModel->m_nTxdIndex = txdSlot;

		CTxdStore::PushCurrentTxd();
		CTxdStore::SetCurrentTxd(txdSlot);

		RwMemory dffMem { pending->dff.data(), static_cast<RwUInt32>(pending->dff.size()) };
		RwStream* dffStream = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &dffMem);
		if (dffStream != nullptr) {
			if (RwStreamFindChunk(dffStream, rwID_CLUMP, nullptr, nullptr)) {
				RpClump* pClump = RpClumpStreamRead(dffStream);
				if (pClump) {
					StreamingExtender::FinalizeClump(newModel, pClump);
				} else {
					CTxdStore::RemoveTxdSlot(txdSlot);
					SendMsg(0xFF0000, std::format("[CustomVeh] Failed to parse DFF for model {}", pending->def.customModelId).c_str());
				}
			} else {
				SendMsg(0xFF0000, std::format("[CustomVeh] No CLUMP chunk in DFF for model {}", pending->def.customModelId).c_str());
			}
			RwStreamClose(dffStream, nullptr);
		} else {
			SendMsg(0xFF0000, std::format("[CustomVeh] Failed to open DFF stream for model {}", pending->def.customModelId).c_str());
		}

		CTxdStore::PopCurrentTxd();
	}

	bool IsCustomModelCurrentlyInUse(uint32_t customModelId)
	{
		return HandlingManager::GetModelUseCount(customModelId) > 0;
	}

public:
	CustomVehiclesASI()
	{
		plugin::Events::initRwEvent.Add([this]() {
			fs::create_directories("models");
			StreamingExtender::InstallHooks();
			m_pipeline = std::make_unique<AssetDownloader>(4);
			AudioExtender::InstallHooks();
		});

		plugin::Events::shutdownRwEvent.Add([this]() {
			if (m_pipeline) {
				m_pipeline->Shutdown();
			}
			StreamingExtender::ClearAllCustomModels();
		});

		StreamingExtender::SetDestructionCallback([](uint32_t modelId) {
			_customVehInstance.PushDestructionCommand(modelId);
		});
	}

	void ProcessCompletedDownloads()
	{
		std::queue<std::shared_ptr<PendingCustomVehicle>> localQueue;
		{
			std::lock_guard<std::mutex> lock(m_queueMutex);
			if (m_completedQueue.empty())
				return;
			localQueue.swap(m_completedQueue);
		}

		while (!localQueue.empty()) {
			FinalizeCustomVehicle(localQueue.front());
			localQueue.pop();
		}
	}

	void HandleCustomVehicleDef(const CustomVehicleDef& def)
	{
		auto pending = std::make_shared<PendingCustomVehicle>();
		pending->def = def;

		std::lock_guard<std::mutex> lock(m_pendingDefMutex);
		m_pendingDefQueue.push(pending);
	}

	void PushDestructionCommand(uint32_t modelId)
	{
		std::lock_guard<std::mutex> lock(m_destructionMutex);
		m_destructionQueue.push(modelId);
	}

	void ProcessPendingDefinitions()
	{
		std::queue<std::shared_ptr<PendingCustomVehicle>> localQueue;
		{
			std::lock_guard<std::mutex> lock(m_pendingDefMutex);
			if (m_pendingDefQueue.empty())
				return;
			localQueue.swap(m_pendingDefQueue);
		}

		while (!localQueue.empty()) {
			auto pending = localQueue.front();
			localQueue.pop();
			AudioExtender::RegisterVehicleAudio(pending->def.customModelId, pending->def.audioBaseModelId, pending->def.engineSoundId);
			CreateModelAndBeginDownloads(pending);
		}
	}

	void ProcessPendingDestructions()
	{
		std::queue<uint32_t> localQueue;
		{
			std::lock_guard<std::mutex> lock(m_destructionMutex);
			if (m_destructionQueue.empty())
				return;
			localQueue.swap(m_destructionQueue);
		}

		while (!localQueue.empty()) {
			uint32_t modelIdToDestroy = localQueue.front();
			localQueue.pop();

			if (!IsCustomModelCurrentlyInUse(modelIdToDestroy)) {
				StreamingExtender::DestroyCustomModel(modelIdToDestroy);
			} else {
				PushDestructionCommand(modelIdToDestroy);
			}
		}
	}

	void RequestClearAllCustomModels()
	{
		m_pendingClearAll.store(true, std::memory_order_relaxed);
	}

	void ProcessPendingClearAll()
	{
		if (m_pendingClearAll.exchange(false, std::memory_order_relaxed)) {
			StreamingExtender::ClearAllCustomModels();
		}
	}

	~CustomVehiclesASI() = default;
} _customVehInstance;

bool ASIinitialized = false;
void InitializeHooks()
{
	while (GetModuleHandleA("samp.dll") == nullptr) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	while (!ASIinitialized) {
		if (rakhook::samp_addr() && rakhook::samp_version() != rakhook::samp_ver::unknown) {
			if (IsGameInitialized()) {
				if (rakhook::initialize()) {
					ASIinitialized = true;
					break;
				}
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	rakhook::on_receive_rpc += [](unsigned char& id, RakNet::BitStream* bs) -> bool {
		if (id == RPC_CUSTOM_VEHICLE_DEF) {
			CustomVehicleDef def;
			bs->Read(reinterpret_cast<char*>(&def), sizeof(CustomVehicleDef));
			_customVehInstance.HandleCustomVehicleDef(def);
			return false;
		} else if (id == RPC_DESTROY_CUSTOM_VEHICLE_MODEL) {
			uint32_t customModelId;

			if (bs->Read(customModelId)) {
				_customVehInstance.PushDestructionCommand(customModelId);
			}
			return false;
		}
		return true;
	};

	rakhook::on_receive_packet += [](auto* packet) -> bool {
		uint8_t packetId = packet->data[0];
		if (packetId == PKT_CHANDLING) {
			RakNet::BitStream bs(packet->data, packet->length, false);
			bs.IgnoreBits(8);

			CHandlingAction actionID;
			bs.Read(actionID);
			return HandlingManager::ProcessAction(actionID, &bs);
		} else if (packetId == ID_DISCONNECTION_NOTIFICATION || packetId == ID_CONNECTION_LOST || packetId == ID_CONNECTION_BANNED) {
			_customVehInstance.RequestClearAllCustomModels();
			HandlingManager::ProcessAction(ACTION_RESET_ALL, nullptr);
		}
		return true;
	};
}

std::unique_ptr<c_plugin> Plugn;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) {
	case DLL_PROCESS_ATTACH: {
		DisableThreadLibraryCalls(hModule);

		Events::initGameEvent += []() {
			static bool threadSpawned = false;
			if (!threadSpawned) {
				threadSpawned = true;
				std::thread(InitializeHooks).detach();
			}
		};
		static CVehicle* s_prevLocalVehicle = nullptr;
		Events::gameProcessEvent += []() {
			_customVehInstance.ProcessPendingDefinitions();
			_customVehInstance.ProcessCompletedDownloads();
			_customVehInstance.ProcessPendingDestructions();
			_customVehInstance.ProcessPendingClearAll();
			HandlingManager::ProcessPendingCommands();

			// Update vehicle cache and model use counts
			static std::vector<CVehicle*> previousVehicles;
			std::vector<CVehicle*> currentVehicles;
			currentVehicles.reserve(128);

			auto pool = GetVehiclesPool();
			if (!std::holds_alternative<std::nullptr_t>(pool)) {
				std::visit([&](auto&& p) {
					using T = std::decay_t<decltype(p)>;
					if constexpr (!std::is_same_v<T, std::nullptr_t>) {
						if (p) {
							for (uint16_t id = 0; id < p->m_nCount; ++id) {
								auto* sampVeh = p->Get(id);
								if (sampVeh && sampVeh->m_pGameVehicle) {
									currentVehicles.push_back(sampVeh->m_pGameVehicle);
								}
							}
						}
					}
				},
					pool);
			}

			// Detect new vehicles
			for (CVehicle* veh : currentVehicles) {
				bool found = false;
				for (CVehicle* old : previousVehicles) {
					if (old == veh) {
						found = true;
						break;
					}
				}
				if (!found) {
					uint16_t sampId = 0xFFFF;
					// Find its SAMP ID (you can scan the pool again, but we already have it)
					// Better: we can scan the pool once more, but we can also get the ID from the SAMP vehicle object.
					// Since we have the SAMP vehicle pointer from the loop above, we can store both.
					// For simplicity, we'll call GetVehicleSAMPId (which uses the cache).
					// But it's not cached yet, so it will scan the pool.
					sampId = HandlingManager::GetVehicleSAMPId(veh);
					if (sampId != 0xFFFF) {
						HandlingManager::CacheVehicleSAMPId(veh, sampId);
					}
				}
			}

			// Detect vehicles that disappeared
			for (CVehicle* old : previousVehicles) {
				bool stillExists = false;
				for (CVehicle* cur : currentVehicles) {
					if (cur == old) {
						stillExists = true;
						break;
					}
				}
				if (!stillExists) {
					HandlingManager::RemoveVehicleFromCache(old);
				}
			}

			// Update previousVehicles for next frame
			previousVehicles = std::move(currentVehicles);

			// ── Vehicle destructor cleanup ──────────────────────────────────────
			HandlingManager::OnVehicleDestructor(nullptr);

			// ── Vehicle enter / exit for local player ───────────────────────────
			auto* localPed = FindPlayerPed();
			if (!localPed) {
				s_prevLocalVehicle = nullptr;
				return;
			}
			CVehicle* curVehicle = localPed->m_pVehicle;

			if (curVehicle != s_prevLocalVehicle) {
				uint16_t localId = GetLocalPlayerId();
				if (s_prevLocalVehicle)
					HandlingManager::RemovePlayerHandling(localId, s_prevLocalVehicle);
				if (curVehicle)
					HandlingManager::ApplyPlayerHandling(localId, curVehicle);
				s_prevLocalVehicle = curVehicle;
			}
		};
		Plugn = std::make_unique<c_plugin>(hModule);
		break;
	}
	case DLL_PROCESS_DETACH: {
		rakhook::on_receive_rpc.clear();
		rakhook::on_send_rpc.clear();
		rakhook::on_receive_packet.clear();
		rakhook::on_send_packet.clear();

		if (GetModuleHandleA("samp.dll") != nullptr) {
			rakhook::destroy();
		}

		Plugn.reset();
		break;
	}
	}
	return true;
}
