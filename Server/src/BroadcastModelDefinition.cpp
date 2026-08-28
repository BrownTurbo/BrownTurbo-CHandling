#include "BroadcastModelDefinition.h"
#include "Actions.h"
#include "HandlingManager.h"
#include "extendedveh.h"
#include <algorithm>
#include <cstring>

static constexpr int kPlayerScanLimit = 2048;

static void SendModelDefToPlayer(IPlayer& player, const HandlingMgr::CustomVehicleDef& def)
{
	CHandlingActionPacket pkt(ACTION_SET_MODEL_HANDLING);
	pkt.data.Write(def.customModelId);

	pkt.data.Write(def.dffUrl, static_cast<int>(sizeof(def.dffUrl)));
	pkt.data.Write(def.dffHash, static_cast<int>(sizeof(def.dffHash)));
	pkt.data.Write(def.txdUrl, static_cast<int>(sizeof(def.txdUrl)));
	pkt.data.Write(def.txdHash, static_cast<int>(sizeof(def.txdHash)));
	pkt.data.Write(def.colUrl, static_cast<int>(sizeof(def.colUrl)));
	pkt.data.Write(def.colHash, static_cast<int>(sizeof(def.colHash)));

	player.sendPacket(Span<uint8_t>(pkt.data.GetData(), pkt.data.GetNumberOfBitsUsed()), 0, true);
}

void BroadcastModelDefinitionToClients(const HandlingMgr::CustomVehicleDef& def)
{
	ExtendedVehCompo* compo = new ExtendedVehCompo();
	ICore* core = compo->getCore();
	if (!core) return;

	for (IPlayer* player : core->getPlayers().players()) {
		if (player)
			SendModelDefToPlayer(*player, def);
	}
}
