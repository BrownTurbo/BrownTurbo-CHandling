#include "BroadcastModelDefinition.h"
#include "HandlingManager.h"
#include <string>
#include "CustomVehicleRegistry.h"

bool DefineCustomVehicle(uint32_t customModelId, const std::string& dffUrl,
	const std::string& dffHash, const std::string& txdUrl,
	const std::string& txdHash, const std::string& colUrl,
	const std::string& colHash)
{
	HandlingMgr::CustomVehicleDef def {};
	def.customModelId = customModelId;
	strncpy(def.dffUrl, dffUrl.c_str(), sizeof(def.dffUrl) - 1);
	strncpy(def.dffHash, dffHash.c_str(), sizeof(def.dffHash) - 1);
	strncpy(def.txdUrl, txdUrl.c_str(), sizeof(def.txdUrl) - 1);
	strncpy(def.txdHash, txdHash.c_str(), sizeof(def.txdHash) - 1);
	if (!colUrl.empty()) {
		strncpy(def.colUrl, colUrl.c_str(), sizeof(def.colUrl) - 1);
	} else {
		def.colUrl[0] = '\0';
	}
	if (!colHash.empty()) {
		strncpy(def.colHash, colHash.c_str(), sizeof(def.colHash) - 1);
	} else {
		def.colHash[0] = '\0';
	}

	if (!CustomVehicleRegistry::RegisterInternalCustomVehicle(def))
		return false;

	BroadcastModelDefinitionToClients(def);

	return true;
}
