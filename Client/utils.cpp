#include "utils.h"

bool SendMsg(int color, const char* msg) {
    rakhook::samp_ver version = rakhook::samp_version();
	bool _sent = false;
	switch (version) {
		case rakhook::samp_ver::v037r1:
		{
			SAMPAPI_EXPORT sampapi::v037r1::CChat*& p_chat = sampapi::v037r1::RefChat();
			_sent = true;
			p_chat->AddMessage(color, msg);
			break;

		}
		case rakhook::samp_ver::v037r31:
		{
			SAMPAPI_EXPORT sampapi::v037r3::CChat*& p_chat = sampapi::v037r3::RefChat();
			_sent = true;
			p_chat->AddMessage(color, msg);
			break;
		}
		case rakhook::samp_ver::v037r5:
		{
			SAMPAPI_EXPORT sampapi::v037r5::CChat*& p_chat = sampapi::v037r5::RefChat();
			_sent = true;
			p_chat->AddMessage(color, msg);
			break;
		}
		case rakhook::samp_ver::v03dlr1:
		{
			SAMPAPI_EXPORT sampapi::v03dl::CChat*& p_chat = sampapi::v03dl::RefChat();
			_sent = true;
			p_chat->AddMessage(color, msg);
			break;
		}
		default:{
			_sent = false;
			break;
		}
	}
	return _sent;
}

PlayerPoolVariant GetPlayerPoolPtr() {
    rakhook::samp_ver version = rakhook::samp_version();
    switch (version) {
        case rakhook::samp_ver::v037r1: {
            auto* pNetGame = sampapi::v037r1::RefNetGame();
            if (!pNetGame || !pNetGame->m_pPools || !pNetGame->m_pPools->m_pPlayer) return nullptr;
            return pNetGame->m_pPools->m_pPlayer;
        }
        case rakhook::samp_ver::v037r31: {
            auto* pNetGame = sampapi::v037r3::RefNetGame();
            if (!pNetGame || !pNetGame->m_pPools || !pNetGame->m_pPools->m_pPlayer) return nullptr;
            return pNetGame->m_pPools->m_pPlayer;
        }
        case rakhook::samp_ver::v037r5: {
            auto* pNetGame = sampapi::v037r5::RefNetGame();
            if (!pNetGame || !pNetGame->m_pPools || !pNetGame->m_pPools->m_pPlayer) return nullptr;
            return pNetGame->m_pPools->m_pPlayer;
        }
        case rakhook::samp_ver::v03dlr1: {
            auto* pNetGame = sampapi::v03dl::RefNetGame();
            if (!pNetGame || !pNetGame->m_pPools || !pNetGame->m_pPools->m_pPlayer) return nullptr;
            return pNetGame->m_pPools->m_pPlayer;
        }
        default:
            return nullptr;
    }
}

bool MatchPlayerId(int playerId) {
    PlayerPoolVariant poolVariant = GetPlayerPoolPtr();
	bool _matched = false;

    std::visit([playerId, &_matched](auto&& localPlayerInfo) {
        using T = std::decay_t<decltype(localPlayerInfo)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            _matched = false;
        } else {
            if (localPlayerInfo) {
                if constexpr (std::is_same_v<T, sampapi::v03dl::CPlayerPool*>)
                {
                    _matched = (localPlayerInfo->m_nLocalPlayerId == playerId);
                }
                else
                {
                    _matched = (localPlayerInfo->m_localInfo.m_nId == playerId);
                }
            }
        }
    }, poolVariant);

    return _matched;
}

CVehicle* GetGameVehicleFromPool(uint16_t sampVehicleId) {
    rakhook::samp_ver ver = rakhook::samp_version();
    switch (ver) {
        case rakhook::samp_ver::v037r1: {
            auto* pNetGame = sampapi::v037r1::RefNetGame();
            if (!pNetGame || !pNetGame->m_pPools || !pNetGame->m_pPools->m_pVehicle) return nullptr;
            auto* veh = pNetGame->m_pPools->m_pVehicle->Get(sampVehicleId);
            return veh ? veh->m_pGameVehicle : nullptr;
        }
        case rakhook::samp_ver::v037r31: {
            auto* pNetGame = sampapi::v037r3::RefNetGame();
            if (!pNetGame || !pNetGame->m_pPools || !pNetGame->m_pPools->m_pVehicle) return nullptr;
            auto* veh = pNetGame->m_pPools->m_pVehicle->Get(sampVehicleId);
            return veh ? veh->m_pGameVehicle : nullptr;
        }
        case rakhook::samp_ver::v037r5: {
            auto* pNetGame = sampapi::v037r5::RefNetGame();
            if (!pNetGame || !pNetGame->m_pPools || !pNetGame->m_pPools->m_pVehicle) return nullptr;
            auto* veh = pNetGame->m_pPools->m_pVehicle->Get(sampVehicleId);
            return veh ? veh->m_pGameVehicle : nullptr;
        }
        case rakhook::samp_ver::v03dlr1: {
            auto* pNetGame = sampapi::v03dl::RefNetGame();
            if (!pNetGame || !pNetGame->m_pPools || !pNetGame->m_pPools->m_pVehicle) return nullptr;
            auto* veh = pNetGame->m_pPools->m_pVehicle->Get(sampVehicleId);
            return veh ? veh->m_pGameVehicle : nullptr;
        }
        default: return nullptr;
    }
}

bool IsGameInitialized()
{
    rakhook::samp_ver version = rakhook::samp_version();
	bool _initialized = false;
	switch (version) {
		case rakhook::samp_ver::v037r1:
		{
			_initialized = (sampapi::v037r1::RefGame() != nullptr);
			break;
		}
		case rakhook::samp_ver::v037r31:
		{
			_initialized = (sampapi::v037r3::RefGame() != nullptr);
			break;
		}
		case rakhook::samp_ver::v037r5:
		{
			_initialized = (sampapi::v037r5::RefGame() != nullptr);
			break;
		}
		case rakhook::samp_ver::v03dlr1:
		{
			_initialized = (sampapi::v03dl::RefGame() != nullptr);
			break;
		}
		default:{
			_initialized = false;
			break;
		}
	}
	return _initialized;
}

uint16_t GetLocalPlayerId()
{
    rakhook::samp_ver version = rakhook::samp_version();
	uint16_t localId = 0xFFFF;
	switch (version) {
		case rakhook::samp_ver::v037r1:
		{
			SAMPAPI_EXPORT sampapi::v037r1::CNetGame* pNetGame = sampapi::v037r1::RefNetGame();
			if (pNetGame && pNetGame->m_pPools && pNetGame->m_pPools->m_pPlayer) {
			    localId = pNetGame->m_pPools->m_pPlayer->m_localInfo.m_nId;
		    }
			break;
		}
		case rakhook::samp_ver::v037r31:
		{
			SAMPAPI_EXPORT sampapi::v037r3::CNetGame* pNetGame = sampapi::v037r3::RefNetGame();
			if (pNetGame && pNetGame->m_pPools && pNetGame->m_pPools->m_pPlayer) {
			    localId = pNetGame->m_pPools->m_pPlayer->m_localInfo.m_nId;
		    }
			break;
		}
		case rakhook::samp_ver::v037r5:
		{
			SAMPAPI_EXPORT sampapi::v037r5::CNetGame* pNetGame = sampapi::v037r5::RefNetGame();
			if (pNetGame && pNetGame->m_pPools && pNetGame->m_pPools->m_pPlayer) {
			    localId = pNetGame->m_pPools->m_pPlayer->m_localInfo.m_nId;
		    }
			break;
		}
		case rakhook::samp_ver::v03dlr1:
		{
			SAMPAPI_EXPORT sampapi::v03dl::CNetGame* pNetGame = sampapi::v03dl::RefNetGame();
			if (pNetGame && pNetGame->m_pPools && pNetGame->m_pPools->m_pPlayer) {
			    localId = pNetGame->m_pPools->m_pPlayer->m_nLocalPlayerId;
		    }
			break;
		}
		default:{
			localId = 0xFFFF;
			break;
		}
	}
	return localId;
}

VehiclePoolVariant GetVehiclesPool()
{
    rakhook::samp_ver version = rakhook::samp_version();
	VehiclePoolVariant vehPool = nullptr;
	switch (version) {
		case rakhook::samp_ver::v037r1:
		{
			SAMPAPI_EXPORT sampapi::v037r1::CNetGame* pNetGame = sampapi::v037r1::RefNetGame();
			if (pNetGame && pNetGame->m_pPools && pNetGame->m_pPools->m_pVehicle) {
			    vehPool = pNetGame->m_pPools->m_pVehicle;
		    }
			break;
		}
		case rakhook::samp_ver::v037r31:
		{
			SAMPAPI_EXPORT sampapi::v037r3::CNetGame* pNetGame = sampapi::v037r3::RefNetGame();
			if (pNetGame && pNetGame->m_pPools && pNetGame->m_pPools->m_pVehicle) {
			    vehPool = pNetGame->m_pPools->m_pVehicle;
		    }
			break;
		}
		case rakhook::samp_ver::v037r5:
		{
			SAMPAPI_EXPORT sampapi::v037r5::CNetGame* pNetGame = sampapi::v037r5::RefNetGame();
			if (pNetGame && pNetGame->m_pPools && pNetGame->m_pPools->m_pVehicle) {
			    vehPool = pNetGame->m_pPools->m_pVehicle;
		    }
			break;
		}
		case rakhook::samp_ver::v03dlr1:
		{
			SAMPAPI_EXPORT sampapi::v03dl::CNetGame* pNetGame = sampapi::v03dl::RefNetGame();
			if (pNetGame && pNetGame->m_pPools && pNetGame->m_pPools->m_pVehicle) {
			    vehPool = pNetGame->m_pPools->m_pVehicle;
		    }
			break;
		}
		default:{
			vehPool = nullptr;
			break;
		}
	}
	return vehPool;
}