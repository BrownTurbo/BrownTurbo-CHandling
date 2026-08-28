#include "CustomVehicleRegistry.h"

namespace {
std::unordered_map<uint32_t, HandlingMgr::CustomVehicleDef> g_registry;
std::mutex g_registryMutex;
}

bool CustomVehicleRegistry::RegisterInternalCustomVehicle(const HandlingMgr::CustomVehicleDef& def)
{
	std::lock_guard<std::mutex> lock(g_registryMutex);
	// Insert or replace
	g_registry[def.customModelId] = def;
	return true;
}

std::optional<HandlingMgr::CustomVehicleDef> CustomVehicleRegistry::GetCustomVehicleDef(uint32_t customModelId)
{
	std::lock_guard<std::mutex> lock(g_registryMutex);
	auto it = g_registry.find(customModelId);
	if (it == g_registry.end())
		return std::nullopt;
	return it->second; // copy
}

void CustomVehicleRegistry::RemoveCustomVehicle(uint32_t customModelId)
{
	std::lock_guard<std::mutex> lock(g_registryMutex);
	g_registry.erase(customModelId);
}

void CustomVehicleRegistry::ForEach(std::function<void(const HandlingMgr::CustomVehicleDef&)> cb)
{
	std::lock_guard<std::mutex> lock(g_registryMutex);
	for (auto& p : g_registry)
		cb(p.second);
}
