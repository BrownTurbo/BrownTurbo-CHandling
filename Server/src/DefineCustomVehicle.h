#include "BroadcastModelDefinition.h"
#include "CustomVehicleRegistry.h"
#include "HandlingManager.h"
#include <cstring>

bool DefineCustomVehicle(uint32_t customModelId,
	const std::string& dffUrl, const std::string& dffHash,
	const std::string& txdUrl, const std::string& txdHash,
	const std::string& colUrl, const std::string& colHash
	/*, other params... */)
{
	HandlingMgr::CustomVehicleDef def {};
	def.customModelId = customModelId;

	std::strncpy(def.dffUrl, dffUrl.c_str(), sizeof(def.dffUrl) - 1);
	def.dffUrl[sizeof(def.dffUrl) - 1] = '\0';
	std::strncpy(def.dffHash, dffHash.c_str(), sizeof(def.dffHash) - 1);
	def.dffHash[sizeof(def.dffHash) - 1] = '\0';

	std::strncpy(def.txdUrl, txdUrl.c_str(), sizeof(def.txdUrl) - 1);
	def.txdUrl[sizeof(def.txdUrl) - 1] = '\0';
	std::strncpy(def.txdHash, txdHash.c_str(), sizeof(def.txdHash) - 1);
	def.txdHash[sizeof(def.txdHash) - 1] = '\0';

	if (!colUrl.empty()) {
		std::strncpy(def.colUrl, colUrl.c_str(), sizeof(def.colUrl) - 1);
		def.colUrl[sizeof(def.colUrl) - 1] = '\0';
	} else {
		def.colUrl[0] = '\0';
	}

	if (!colHash.empty()) {
		std::strncpy(def.colHash, colHash.c_str(), sizeof(def.colHash) - 1);
		def.colHash[sizeof(def.colHash) - 1] = '\0';
	} else {
		def.colHash[0] = '\0';
	}

	if (!CustomVehicleRegistry::RegisterInternalCustomVehicle(def))
		return false;

	BroadcastModelDefinitionToClients(def);
	return true;
}
