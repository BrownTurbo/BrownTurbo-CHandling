#pragma once

#include <sampapi/CChat.h>
#include <sampapi/CGame.h>
#include <sampapi/CLocalPlayer.h>
#include <sampapi/CNetGame.h>
#include <sampapi/CPlayerPool.h>
#include <sampapi/CVehiclePool.h>
#include <sampapi/sampapi.h>

#include <RakHook/rakhook.hpp>
#include <RakHook/samp.hpp>

#include <cstdint>
#include <variant>

bool SendMsg(int color, const char* msg);

using PlayerPoolVariant = std::variant<
	std::nullptr_t,
	sampapi::v037r1::CPlayerPool*,
	sampapi::v037r3::CPlayerPool*,
	sampapi::v037r5::CPlayerPool*,
	sampapi::v03dl::CPlayerPool*>;

using VehiclePoolVariant = std::variant<
	std::nullptr_t,
	sampapi::v037r1::CVehiclePool*,
	sampapi::v037r3::CVehiclePool*,
	sampapi::v037r5::CVehiclePool*,
	sampapi::v03dl::CVehiclePool*>;

PlayerPoolVariant GetPlayerPoolPtr();
bool MatchPlayerId(int playerId);
CVehicle* GetGameVehicleFromPool(uint16_t sampVehicleId);
bool IsGameInitialized();
uint16_t GetLocalPlayerId();
VehiclePoolVariant GetVehiclesPool();
