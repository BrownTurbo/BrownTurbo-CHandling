#pragma once

#include <cstring>
#include <unordered_map>

#include "game_sa/CHandlingDataMgr.h"
#include "game_sa/CModelInfo.h"
#include "game_sa/CStreaming.h"
#include "game_sa/CTxdStore.h"
#include "game_sa/CVehicleModelInfo.h"
#include "game_sa/CVisibilityPlugins.h"
#include "game_sa/NodeName.h"
#include "game_sa/common.h"
#include "injector/injector.hpp"
#include "plugin.h"

#include "CVisibilityPlugins.h"
#include "audioextender.hpp"
#include "defs.h"
#include "handling_manager.hpp"
#include "rpworld.h"

class StreamingExtender {
private:
	static inline std::unordered_map<uint32_t, CVehicleModelInfo*> s_customModels;
	static inline void* s_originalGetModelInfo = nullptr;
	static inline void* s_originalRequestModel = nullptr;
	static inline void* s_originalHasModelLoaded = nullptr;
	static inline void* s_originalRemoveModel = nullptr;
	static inline bool s_hooksInstalled = false;
	inline static void (*s_destructionRequeueCallback)(uint32_t) = nullptr;

	// ---- streaming hooks (unchanged from the old StreamingExtender) ----
	static CBaseModelInfo* __cdecl Hooked_GetModelInfo(int index)
	{
		if (index >= CUSTOM_MODEL_BASE_ID) {
			auto it = s_customModels.find(index);
			return (it != s_customModels.end()) ? it->second : nullptr;
		}
		return reinterpret_cast<CBaseModelInfo*(__cdecl*)(int)>(s_originalGetModelInfo)(index);
	}

	static void __cdecl Hooked_RequestModel(int index, int flags)
	{
		if (index >= CUSTOM_MODEL_BASE_ID) {
			return; // custom models are loaded manually via CreateCustomModel/FinalizeClump,
					// never through the engine's own streaming request queue
		}
		reinterpret_cast<void(__cdecl*)(int, int)>(s_originalRequestModel)(index, flags);
	}

	static bool __cdecl Hooked_HasModelLoaded(int index)
	{
		if (index >= CUSTOM_MODEL_BASE_ID) {
			auto it = s_customModels.find(index);
			return (it != s_customModels.end() && it->second->m_pRwClump != nullptr);
		}
		return reinterpret_cast<bool(__cdecl*)(int)>(s_originalHasModelLoaded)(index);
	}

	static void __cdecl Hooked_RemoveModel(int index)
	{
		if (index >= CUSTOM_MODEL_BASE_ID) {
			return; // custom models are never unloaded through the engine's own streaming path
		}
		reinterpret_cast<void(__cdecl*)(int)>(s_originalRemoveModel)(index);
	}

	// ---- construction helpers (merged in from the old ModelExtender) ----
	static RpAtomic* SetAtomicRenderCallbacks(RpAtomic* atomic, void* data)
	{
		CVisibilityPlugins::SetAtomicRenderCallback(atomic, nullptr);

		const char* nodeName = GetFrameNodeName(RpAtomicGetFrame(atomic));
		if (nodeName && (strstr(nodeName, "_dam") || strstr(nodeName, "bump"))) {
			// Apply GTA SA specific damage material flags here.
		}
		return atomic;
	}

public:
	static void InstallHooks()
	{
		if (s_hooksInstalled)
			return;

		s_originalGetModelInfo = plugin::patch::Get<void*>(0x403BA0);
		injector::MakeJMP(0x403BA0, Hooked_GetModelInfo, true);

		s_originalRequestModel = plugin::patch::Get<void*>(0x4087E0);
		injector::MakeJMP(0x4087E0, Hooked_RequestModel, true);

		s_originalHasModelLoaded = plugin::patch::Get<void*>(0x407800);
		injector::MakeJMP(0x407800, Hooked_HasModelLoaded, true);

		s_originalRemoveModel = plugin::patch::Get<void*>(0x4089A0);
		injector::MakeJMP(0x4089A0, Hooked_RemoveModel, true);
		s_hooksInstalled = true;
	}

	static void RestoreHooks()
	{
		if (!s_hooksInstalled)
			return;

		injector::MakeJMP(0x403BA0, s_originalGetModelInfo, true);
		injector::MakeJMP(0x4087E0, s_originalRequestModel, true);
		injector::MakeJMP(0x407800, s_originalHasModelLoaded, true);
		injector::MakeJMP(0x4089A0, s_originalRemoveModel, true);
		s_originalGetModelInfo = nullptr;
		s_originalRequestModel = nullptr;
		s_originalHasModelLoaded = nullptr;
		s_originalRemoveModel = nullptr;
		s_hooksInstalled = false;
	}

	static CVehicleModelInfo* CreateCustomModel(const CustomVehicleDef& def)
	{
		auto it = s_customModels.find(def.customModelId);
		if (it != s_customModels.end() && it->second != nullptr) {
			return it->second;
		}

		CBaseModelInfo* visualBase = CModelInfo::GetModelInfo(def.visualBaseModelId);
		if (!visualBase)
			return nullptr;

		auto* vBaseInfo = reinterpret_cast<CVehicleModelInfo*>(visualBase);

		CVehicleModelInfo* newModel = new CVehicleModelInfo();
		memcpy(newModel, vBaseInfo, sizeof(CVehicleModelInfo));
		newModel->m_pRwClump = nullptr;
		newModel->m_pRwObject = nullptr;
		newModel->m_pColModel = vBaseInfo->m_pColModel;

		CBaseModelInfo* handlingBase = CModelInfo::GetModelInfo(def.handlingBaseModelId);
		if (handlingBase) {
			newModel->m_nHandlingId = reinterpret_cast<CVehicleModelInfo*>(handlingBase)->m_nHandlingId;
		}

		s_customModels[def.customModelId] = newModel;
		return newModel;
	}

	static void FinalizeClump(CVehicleModelInfo* pInfo, RpClump* pClump)
	{
		if (!pInfo || !pClump)
			return;
		if (pInfo->m_pRwClump) {
			RpClumpDestroy(pInfo->m_pRwClump);
			pInfo->m_pRwClump = nullptr;
		}
		CVisibilityPlugins::SetupVehicleVariables(pClump);
		RpClumpForAllAtomics(pClump, SetAtomicRenderCallbacks, nullptr);
		pInfo->SetClump(pClump);
	}

	static void RegisterModel(uint32_t customId, CVehicleModelInfo* pInfo)
	{
		s_customModels[customId] = pInfo;
	}

	static CVehicleModelInfo* GetCustomModel(uint32_t id)
	{
		auto it = s_customModels.find(id);
		return (it != s_customModels.end()) ? it->second : nullptr;
	}

	static bool IsCustomModel(uint32_t id)
	{
		return s_customModels.contains(id);
	}

	static void DestroyCustomModel(uint32_t customId)
	{
		// Check if the model is still in use
		if (HandlingManager::GetModelUseCount(customId) > 0) {
			// Re-queue the destruction command
			if (s_destructionRequeueCallback) {
				s_destructionRequeueCallback(customId);
			}
			return;
		}

		// Proceed with destruction...
		auto it = s_customModels.find(customId);
		if (it == s_customModels.end())
			return;

		CVehicleModelInfo* pInfo = it->second;
		if (pInfo) {
			if (pInfo->m_pRwClump) {
				RpClumpDestroy(pInfo->m_pRwClump);
				pInfo->m_pRwClump = nullptr;
			}
			if (pInfo->m_nTxdIndex != -1) {
				CTxdStore::RemoveTxdSlot(pInfo->m_nTxdIndex);
				pInfo->m_nTxdIndex = -1;
			}
			delete pInfo;
		}
		s_customModels.erase(it);
		AudioExtender::UnregisterVehicleAudio(customId);
	}

	static void ClearAllCustomModels()
	{
		for (auto& [id, pInfo] : s_customModels) {
			if (pInfo) {
				if (pInfo->m_pRwClump)
					RpClumpDestroy(pInfo->m_pRwClump);
				if (pInfo->m_nTxdIndex != -1)
					CTxdStore::RemoveTxdSlot(pInfo->m_nTxdIndex);
				delete pInfo;
			}
			AudioExtender::UnregisterVehicleAudio(id);
		}
		s_customModels.clear();
	}

	static void SetDestructionCallback(void (*callback)(uint32_t))
	{
		s_destructionRequeueCallback = callback;
	}
};
