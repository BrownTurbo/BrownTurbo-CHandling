#include "CustomVehicleBindingManager.h"
#include "CollisionLoader.h"
#include "streamingextender.hpp"
#include "utils.h"

#include <game_sa/CStreaming.h>

void CustomVehicleBindingManager::Bind(uint16_t vehicleId, uint32_t customModelId)
{
	std::lock_guard lock(m_mutex);

	Binding binding;
	binding.sampVehicleId = vehicleId;
	binding.customModelId = customModelId;
	binding.gtaModelId = customModelId; // the id StreamingExtender::Hooked_GetModelInfo
										// actually redirects - this MUST equal
										// customModelId, not stay default-zero
	binding.originalModelId = -1;
	binding.modelApplied = false;

	m_bindings[vehicleId] = binding;
}

void CustomVehicleBindingManager::Unbind(uint16_t vehicleId)
{
	std::lock_guard lock(m_mutex);

	auto it = m_bindings.find(vehicleId);
	if (it == m_bindings.end())
		return;

	const Binding binding = it->second;
	uint32_t modelId = binding.customModelId;

	if (CStreaming::ms_aInfoForModel[modelId].m_nLoadState == LOADSTATE_LOADED) {
		CStreaming::RemoveModel(modelId);
	}

	std::string err_;
	ExtendedVeh::Collision::CollisionLoader::Instance().Unload(modelId, err_);
	if (!err_.empty()) {
		SendMsg(0xFFFFFF, std::format("Error: %s; vehicleId=%d modelId=%d", err_, vehicleId, modelId).c_str());
	}

	if (binding.modelApplied && binding.originalModelId >= 0) {
		if (auto* vehicle = GetGameVehicleFromPool(vehicleId))
			vehicle->SetModelIndexNoCreate(binding.originalModelId);
	}

	m_bindings.erase(it);
}

CustomVehicleBindingManager::Binding* CustomVehicleBindingManager::Find(uint16_t vehicleId)
{
	std::lock_guard lock(m_mutex);
	auto it = m_bindings.find(vehicleId);
	return it != m_bindings.end() ? &it->second : nullptr;
}

void CustomVehicleBindingManager::Process()
{
	std::lock_guard lock(m_mutex);

	for (auto& [vehicleId, binding] : m_bindings) {
		auto* model = StreamingExtender::GetCustomModel(binding.customModelId);

		if (!model)
			continue;

		if (!model->m_pRwClump)
			continue;

		auto* vehicle = GetGameVehicleFromPool(vehicleId);
		if (!vehicle)
			continue;

		if (binding.originalModelId < 0) {
			binding.originalModelId = vehicle->m_nModelIndex;
		}

		if (vehicle->m_nModelIndex != static_cast<int>(binding.gtaModelId)) {
			vehicle->SetModelIndexNoCreate(
				binding.gtaModelId);

			binding.modelApplied = true;
		}
	}
}
