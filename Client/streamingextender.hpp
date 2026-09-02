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
#include "safetyhook/safetyhook.hpp"
#include "plugin.h"

#include "CVisibilityPlugins.h"
#include "audioextender.hpp"
#include "defs.h"
#include "handling_manager.hpp"
#include "rpworld.h"

class StreamingExtender {
private:
	static inline std::unordered_map<uint32_t, CVehicleModelInfo*> s_customModels;
	using GetModelInfoFn = CBaseModelInfo* (__cdecl*)(int);
	using RequestModelFn = void (__cdecl*)(int, int);
	using RemoveModelFn = void (__cdecl*)(int);
	static inline safetyhook::InlineHook s_getModelInfoHook;
	static inline safetyhook::InlineHook s_requestModelHook;
	static inline safetyhook::InlineHook s_removeModelHook;
	static inline bool s_hooksInstalled = false;
	inline static void (*s_destructionRequeueCallback)(uint32_t) = nullptr;
	static CBaseModelInfo* __cdecl Hooked_GetModelInfo(int index)
	{
		if (index >= CUSTOM_MODEL_BASE_ID) {
			auto it = s_customModels.find(index);
			return (it != s_customModels.end()) ? it->second : nullptr;
		}
		return s_getModelInfoHook.original<GetModelInfoFn>()(index);
	}

	static void __cdecl Hooked_RequestModel(int index, int flags)
	{
		if (index >= CUSTOM_MODEL_BASE_ID) {
			return; // custom models are loaded manually via CreateCustomModel/FinalizeClump,
					// never through the engine's own streaming request queue
		}
		s_requestModelHook.original<RequestModelFn>()(index, flags);
	}

	static void __cdecl Hooked_RemoveModel(int index)
	{
		if (index >= CUSTOM_MODEL_BASE_ID) {
			return; // custom models are never unloaded through the engine's own streaming path
		}
		s_removeModelHook.original<RemoveModelFn>()(index);
	}

public:
	static void InstallHooks()
	{
		if (s_hooksInstalled)
			return;

		s_getModelInfoHook = safetyhook::create_inline(
			reinterpret_cast<void*>(0x403BA0), Hooked_GetModelInfo);
		s_requestModelHook = safetyhook::create_inline(
			reinterpret_cast<void*>(0x4087E0), Hooked_RequestModel);
		s_removeModelHook = safetyhook::create_inline(
			reinterpret_cast<void*>(0x4089A0), Hooked_RemoveModel);

		if (!s_getModelInfoHook || !s_requestModelHook ||
			!s_removeModelHook) {
			s_removeModelHook.reset();
			s_requestModelHook.reset();
			s_getModelInfoHook.reset();
			return;
		}

		s_hooksInstalled = true;
	}

	static void RestoreHooks()
	{
		if (!s_hooksInstalled)
			return;

		s_removeModelHook.reset();
		s_requestModelHook.reset();
		s_getModelInfoHook.reset();
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
		pInfo->SetClump(pClump);
		pInfo->SetAtomicRenderCallbacks();
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
