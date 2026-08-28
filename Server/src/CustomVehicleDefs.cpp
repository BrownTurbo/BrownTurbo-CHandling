#include "BroadcastModelDefinition.h"
#include "HandlingManager.h"
#include <string>
#include "CustomVehicleRegistry.h"

bool DefineCustomVehicle(uint32_t customModelId, const std::string& dffUrl,
	const std::string& dffHash, const std::string& txdUrl,
	const std::string& txdHash, const std::string& colUrl,
	const std::string& colHash,
	/* other params... */)
{
	HandlingMgr::CustomVehicleDef def {};
	def.customModelId = customModelId;
	// copy filenames/hashes into fixed-size char[] (ensure truncation is safe)
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
		def.colHash[0] = '\0'; // empty = no custom collision
	}

	// store def into your server-side registry/persistence (vector, DB, etc.)
	if (!CustomVehicleRegistry::RegisterInternalCustomVehicle(def)) // your internal function
		return false;

	// Optionally notify connected players of new/changed model (send
	// ACTION_SET_MODEL_HANDLING or similar) so clients know they may need to
	// request assets.
	BroadcastModelDefinitionToClients(def);

	return true;
}
