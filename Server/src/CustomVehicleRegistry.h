#pragma once
#include "HandlingManager.h"
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

// Thread-safe registry of HandlingMgr::CustomVehicleDef indexed by customModelId.
namespace CustomVehicleRegistry
{
bool RegisterInternalCustomVehicle(const HandlingMgr::CustomVehicleDef& def);
std::optional<HandlingMgr::CustomVehicleDef> GetCustomVehicleDef(uint32_t customModelId);
void RemoveCustomVehicle(uint32_t customModelId);
// Iterates all registered defs; callback runs under registry lock (fast).
void ForEach(std::function<void(const HandlingMgr::CustomVehicleDef&)> cb);
}
