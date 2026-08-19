#include "Actions.h"
#include "CPlayer.h"
#include "HandlingManager.h"
#include "chandlingsvr.h"
#include "CHandlingStore.hpp"
#include "CVehicleManager.hpp"

namespace HandlingMgr {
	void __WriteHandlingEntryToBitStream(NetworkBitStream *bs, const struct stHandlingEntry entry);
}

bool Actions::Process(CHandlingAction id, NetworkBitStream &bs, IPlayer &player)
{
	switch (id)
	{
	case ACTION_INIT:
	{
		uint32_t compat_ver;
		bs.Read(compat_ver);

		CHandlingActionPacket pkt(ACTION_INIT_RESPONSE);
		pkt.data.Write((uint32_t)CHANDLING_COMPAT_VERSION);

		int playerid = player.getID();
		if (compat_ver >= CHANDLING_COMPAT_VERSION)
		{
			pkt.data.Write(true);
			gPlayers[playerid].setHasCHandling();
			auto core_ = CHandlingCompo::getCore();
			if (core_)
			{
				core_->logLn(LogLevel::Message, "[CHandling] Player %d reports having chandling plugin", playerid);
			}
			player.sendPacket(Span<uint8_t>(pkt.data.GetData(), pkt.data.GetNumberOfBitsUsed()), 0, false);
			HandlingMgr::OnPlayerConnect(player);
			return true;
		}
		else
		{
			pkt.data.Write(false);
			player.sendPacket(Span<uint8_t>(pkt.data.GetData(), pkt.data.GetNumberOfBitsUsed()), 0, false);
			return true;
		}
		return true;
	}
	case ACTION_SET_PLAYER_HANDLING:
	{
        uint16_t playerId;
        uint8_t count;
        bs.Read(playerId);
        bs.Read(count);
        for (int i = 0; i < count; ++i) {
            CHandlingAttrib attrib;
            CHandlingAttribType type;
            bs.Read(attrib);
            bs.Read(type);
        }
        return false;
	}
	case ACTION_RESET_PLAYER_HANDLING:
	{
        uint16_t playerId;
        bs.Read(playerId);
        HandlingMgr::ResetPlayerHandling(playerId);
        return false;
	}
	case ACTION_GET_VEHICLE_HANDLING: {
        uint16_t vehicleId;
        bs.Read(vehicleId);
        auto it = HandlingMgr::vehicleHandlings.find(vehicleId);
        if (it == HandlingMgr::vehicleHandlings.end() || it->second.handlingModMap.empty()) {
            return false;
        }
        struct CHandlingActionPacket response(ACTION_SET_VEHICLE_HANDLING);
        response.data.Write(vehicleId);
        HandlingMgr::__WriteHandlingEntryToBitStream(&response.data, it->second);
        player.sendPacket(Span<uint8_t>(response.data.GetData(), response.data.GetNumberOfBitsUsed()), 0, true);
        return false;
	}
	case ACTION_GET_MODEL_HANDLING: {
        uint16_t modelId;
        bs.Read(modelId);

        if (!CVehicleMgr::IS_VALID_VEHICLE_MODEL(modelId)) {
            return false;
        }
        
        HandlingMgr::stHandlingEntry* entry = HandlingMgr::GetModelHandlingEntry(modelId);
        if (!entry || entry->handlingModMap.empty()) {
            return false;
        }

        struct CHandlingActionPacket response(ACTION_SET_MODEL_HANDLING);
        response.data.Write(modelId);
        HandlingMgr::__WriteHandlingEntryToBitStream(&response.data, *entry);
        player.sendPacket(Span<uint8_t>(response.data.GetData(), response.data.GetNumberOfBitsUsed()), 0, true);
        return false;
	}
	case ACTION_GET_PLAYER_HANDLING: {
        uint16_t playerId;
        bs.Read(playerId);
        auto it = HandlingMgr::playerHandlings.find(playerId);
        if (it == HandlingMgr::playerHandlings.end() || it->second.handlingModMap.empty()) {
            return false;
        }
        struct CHandlingActionPacket response(ACTION_SET_PLAYER_HANDLING);
        response.data.Write(playerId);
        HandlingMgr::__WriteHandlingEntryToBitStream(&response.data, it->second);
        player.sendPacket(Span<uint8_t>(response.data.GetData(), response.data.GetNumberOfBitsUsed()), 0, true);
        return false;
	}
	case ACTION_RESET_ALL:
	{
        uint16_t playerId;
        bs.Read(playerId);
        HandlingMgr::ResetAll(playerId);
        return false;
	}
	default:
		break;
	}
	return false;
}
