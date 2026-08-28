#pragma once

#include "injector/injector.hpp"
#include <game_sa/CAEVehicleAudioEntity.h>
#include <game_sa/CModelInfo.h>
#include <game_sa/CVehicle.h>
#include <game_sa/CVehicleModelInfo.h>
#include <plugin_sa.h>
#include <cstdint>
#include <unordered_map>

#include <mutex>

// ---------------------------------------------------------
// HARDENED VEHICLE AUDIO EXTENDER
// ---------------------------------------------------------
class AudioExtender {
private:
	static inline std::unordered_map<uint32_t, int32_t> s_customAudioMap;
	static inline std::mutex s_audioMutex;
	static inline void* s_originalInitVehicleAudio = nullptr;
	static constexpr std::ptrdiff_t kVehicleAudioIdOffset = 0x2A;

	static int16_t& GetModelAudioId(CVehicleModelInfo* pInfo)
	{
		return *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(pInfo) + kVehicleAudioIdOffset);
	}

	// Hook callback when CAEVehicleAudioEntity initializes sound for a created vehicle instance
	static void __fastcall Hooked_InitialiseVehicleAudio(CAEVehicleAudioEntity* pAudio, void* edx, CVehicle* pVehicle)
	{
		if (pVehicle) {
			uint32_t modelId = pVehicle->m_nModelIndex;
			int32_t customSoundId = -1;
			{
				std::lock_guard<std::mutex> lock(s_audioMutex);
				auto it = s_customAudioMap.find(modelId);
				if (it != s_customAudioMap.end()) {
					customSoundId = it->second;
				}
			}

			if (customSoundId != -1) {
				CVehicleModelInfo* pInfo = reinterpret_cast<CVehicleModelInfo*>(CModelInfo::GetModelInfo(modelId));
				if (pInfo) {
					int16_t& soundIdRef = GetModelAudioId(pInfo);
					int16_t originalSoundId = soundIdRef;
					soundIdRef = static_cast<int16_t>(customSoundId);

					reinterpret_cast<void(__fastcall*)(CAEVehicleAudioEntity*, void*, CVehicle*)>(s_originalInitVehicleAudio)(pAudio, edx, pVehicle);

					soundIdRef = originalSoundId;
					return;
				}
			}
		}

		// Fallback to native execution
		reinterpret_cast<void(__fastcall*)(CAEVehicleAudioEntity*, void*, CVehicle*)>(s_originalInitVehicleAudio)(pAudio, edx, pVehicle);
	}

public:
	static void InstallHooks()
	{
		// Hook CAEVehicleAudioEntity::Initialise (0x4EFA10)
		s_originalInitVehicleAudio = plugin::patch::Get<void*>(0x4EFA10);
		injector::MakeJMP(0x4EFA10, Hooked_InitialiseVehicleAudio, true);
	}

	static void RegisterVehicleAudio(uint32_t customModelId, uint32_t audioBaseModelId, int16_t engineSoundId)
	{
		std::lock_guard<std::mutex> lock(s_audioMutex);
		if (engineSoundId >= 0) {
			s_customAudioMap[customModelId] = static_cast<int32_t>(engineSoundId);
		} else if (audioBaseModelId >= 400 && audioBaseModelId <= 611) {
			CVehicleModelInfo* pBaseInfo = reinterpret_cast<CVehicleModelInfo*>(CModelInfo::GetModelInfo(audioBaseModelId));
			if (pBaseInfo) {
				s_customAudioMap[customModelId] = static_cast<int32_t>(GetModelAudioId(pBaseInfo));
			}
		}
	}

	static void RegisterVehicleAudio(uint32_t customModelId, int16_t engineSoundId)
	{
		std::lock_guard<std::mutex> lock(s_audioMutex);
		s_customAudioMap[customModelId] = static_cast<int32_t>(engineSoundId);
	}

	static int32_t GetVehicleAudio(uint32_t customModelId)
	{
		std::lock_guard<std::mutex> lock(s_audioMutex);
		auto it = s_customAudioMap.find(customModelId);
		return (it != s_customAudioMap.end()) ? it->second : -1;
	}

	static int32_t GetAudioIdForModel(uint32_t modelId)
	{
		return GetVehicleAudio(modelId);
	}

	static void UnregisterVehicleAudio(uint32_t customModelId)
	{
		std::lock_guard<std::mutex> lock(s_audioMutex);
		s_customAudioMap.erase(customModelId);
	}
};