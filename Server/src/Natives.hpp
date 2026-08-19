#pragma once

#include "CPlayer.h"
#include "Hooks.hpp"
#include "HandlingEnum.h"
#include "HandlingManager.h"
#include "CVehicleManager.hpp"
#include "chandlingsvr.h"
#include <cstring>
#include <sdk.hpp>
#include <Server/Components/Pawn/pawn.hpp>
#include <Server/Components/Pawn/Impl/pawn_natives.hpp>

// Vehicle related funcs hooks
using namespace NativeHook;
    
void RegisterNativeHooks();
void RegisterNativeHooks()
{
	NativeHookManager::Instance().RegisterHookByName("CreateVehicle", [](AMX *amx, cell *params, NativeHook::amx_native_fn_t orig) -> cell
	{
		auto core_ = CHandlingCompo::getCore();
		if (core_)
		{
			core_->logLn(LogLevel::Debug, "[CHandling] Hooked CreateVehicle");
		}
		int vehicleid = orig(amx, params);
		if (vehicleid != INVALID_VEHICLE_ID)
		{
			HandlingMgr::OnCreateVehicle(vehicleid);
		}
		return static_cast<cell>(vehicleid);
	});

	NativeHookManager::Instance().RegisterHookByName("AddStaticVehicle", [](AMX *amx, cell *params, NativeHook::amx_native_fn_t orig) -> cell
	{
		auto core_ = CHandlingCompo::getCore();
		if (core_)
		{
			core_->logLn(LogLevel::Debug, "[CHandling] Hooked AddStaticVehicle");
		}
		int vehicleid = orig(amx, params);
		if (vehicleid != INVALID_VEHICLE_ID)
		{
			HandlingMgr::OnCreateVehicle(vehicleid);
		}
		return static_cast<cell>(vehicleid);
	});

	NativeHookManager::Instance().RegisterHookByName("AddStaticVehicleEx", [](AMX *amx, cell *params, NativeHook::amx_native_fn_t orig) -> cell
	{
		auto core_ = CHandlingCompo::getCore();
		if (core_)
		{
			core_->logLn(LogLevel::Debug, "[CHandling] Hooked AddStaticVehicleEx");
		}
		int vehicleid = orig(amx, params);
		if (vehicleid != INVALID_VEHICLE_ID)
		{
			HandlingMgr::OnCreateVehicle(vehicleid);
		}
		return static_cast<cell>(vehicleid);
	});

	NativeHookManager::Instance().RegisterHookByName("DestroyVehicle", [](AMX *amx, cell *params, NativeHook::amx_native_fn_t orig) -> cell
	{
		auto core_ = CHandlingCompo::getCore();
		if (core_)
		{
			core_->logLn(LogLevel::Debug, "[CHandling] Hooked DestroyVehicle");
		}
		int ret = orig(amx, params);
		return static_cast<cell>(ret);
	});
}

// Vehicle handling related funcs
SCRIPT_API(GetHandlingAttribType, cell(int attr))
{
	CHandlingAttrib handlingAttr = static_cast<CHandlingAttrib>(attr);
	CHandlingAttribType type = GetHandlingAttributeType(handlingAttr);
	return static_cast<cell>(type);
}

SCRIPT_API(IsPlayerUsingCHandling, bool(IPlayer &player))
{
	if (&player != nullptr)
	{
		int playerid = player.getID();
		return gPlayers[playerid].hasCHandling();
	}
	return false;
}

SCRIPT_API(ResetModelHandling, bool(int modelid))
{
	return HandlingMgr::ResetModelHandling(modelid);
}

SCRIPT_API(ResetVehicleHandling, bool(IVehicle &vehicle))
{
	HandlingMgr::ResetVehicleHandling(vehicle);
	return true;
}

SCRIPT_API(SetVehicleHandlingFloat, bool(int vehicleid, CHandlingAttrib attrib, float value))
{
	return HandlingMgr::SetVehicleHandling((uint16_t)vehicleid, attrib, value);
}

SCRIPT_API(SetVehicleHandlingInt, bool(int vehicleid, CHandlingAttrib attrib, int value))
{
	if (GetHandlingAttributeType(attrib) == TYPE_BYTE)
		return HandlingMgr::SetVehicleHandling((uint16_t)vehicleid, attrib, (uint8_t)value);

	return HandlingMgr::SetVehicleHandling((uint16_t)vehicleid, attrib, (unsigned int)value);
}

SCRIPT_API(SetModelHandlingFloat, bool(int modelid, CHandlingAttrib attrib, float value))
{
	return HandlingMgr::SetModelHandling((uint16_t)modelid, attrib, value);
}

SCRIPT_API(SetModelHandlingInt, bool(int modelid, CHandlingAttrib attrib, int value))
{
	if (GetHandlingAttributeType(attrib) == TYPE_BYTE)
		return HandlingMgr::SetModelHandling((uint16_t)modelid, attrib, (uint8_t)value);

	return HandlingMgr::SetModelHandling((uint16_t)modelid, attrib, (unsigned int)value);
}

SCRIPT_API(GetVehicleHandlingFloat, bool(int vehicleid, CHandlingAttrib attrib, float &value))
{
    value = 0.0f;
    return HandlingMgr::GetVehicleHandling((uint16_t)vehicleid, attrib, value);
}

SCRIPT_API(GetVehicleHandlingInt, bool(int vehicleid, CHandlingAttrib attrib, unsigned int &value))
{
	value = 0;
	bool ret = false;

	if (GetHandlingAttributeType(attrib) == TYPE_BYTE)
	{
		uint8_t byteVal = 0;
		ret = HandlingMgr::GetVehicleHandling((uint16_t)vehicleid, attrib, byteVal);
		value = byteVal;
	}
	else
	{
		ret = HandlingMgr::GetVehicleHandling((uint16_t)vehicleid, attrib, value);
	}
	return ret;
}

SCRIPT_API(GetModelHandlingFloat, bool(int modelid, CHandlingAttrib attrib, float &value))
{
	value = 0.0f;
	return HandlingMgr::GetModelHandling((uint16_t)modelid, attrib, value);
}

SCRIPT_API(GetModelHandlingInt, bool(int modelid, CHandlingAttrib attrib, unsigned int &value))
{
	value = 0;
	bool ret = false;

	if (GetHandlingAttributeType(attrib) == TYPE_BYTE)
	{
		uint8_t byteVal = 0;
		ret = HandlingMgr::GetModelHandling((uint16_t)modelid, attrib, byteVal);
		value = byteVal;
	}
	else
	{
		ret = HandlingMgr::GetModelHandling((uint16_t)modelid, attrib, value);
	}
	return ret;
}

SCRIPT_API(GetDefaultHandlingFloat, bool(int modelid, CHandlingAttrib attrib, float &value))
{
	value = 0.0f;
	return HandlingMgr::GetDefaultHandling((uint16_t)modelid, attrib, value);
}

SCRIPT_API(GetDefaultHandlingInt, bool(int modelid, CHandlingAttrib attrib, unsigned int &value))
{
	bool ret = false;

	if (GetHandlingAttributeType(attrib) == TYPE_BYTE)
	{
		uint8_t byteVal = 0;
		ret = HandlingMgr::GetDefaultHandling((uint16_t)modelid, attrib, byteVal);
		value = byteVal;
	}
	else
	{
		ret = HandlingMgr::GetDefaultHandling((uint16_t)modelid, attrib, value);
	}
	return ret;
}

SCRIPT_API(SetPlayerHandlingFloat, bool(int playerid, CHandlingAttrib attrib, float value))
{
    return HandlingMgr::SetPlayerHandling((uint16_t)playerid, attrib, value);
}

SCRIPT_API(ResetAllHandlingForPlayer, bool(int playerid))
{
    return HandlingMgr::ResetAll((uint16_t)playerid);
}

SCRIPT_API(SetPlayerHandlingInt, bool(int playerid, CHandlingAttrib attrib, int value))
{
    if (GetHandlingAttributeType(attrib) == TYPE_BYTE)
        return HandlingMgr::SetPlayerHandling((uint16_t)playerid, attrib, (uint8_t)value);
    return HandlingMgr::SetPlayerHandling((uint16_t)playerid, attrib, (unsigned int)value);
}

SCRIPT_API(GetPlayerHandlingFloat, bool(int playerid, CHandlingAttrib attrib, float &value))
{
    value = 0.0f;
    return HandlingMgr::GetPlayerHandling((uint16_t)playerid, attrib, value);
}

SCRIPT_API(GetPlayerHandlingInt, bool(int playerid, CHandlingAttrib attrib, unsigned int &value))
{
	value = 0;
	bool ret = false;

	if (GetHandlingAttributeType(attrib) == TYPE_BYTE)
	{
		uint8_t byteVal = 0;
		ret = HandlingMgr::GetPlayerHandling((uint16_t)playerid, attrib, byteVal);
		value = byteVal;
	}
	else
	{
		ret = HandlingMgr::GetPlayerHandling((uint16_t)playerid, attrib, value);
	}
	return ret;
}

SCRIPT_API(ResetPlayerHandling, bool(int playerid))
{
    return HandlingMgr::ResetPlayerHandling((uint16_t)playerid);
}

SCRIPT_API(BeginCustomVehicleDef, bool(int customModelId, int visualBase, int audioBase, int handlingBase, int engineSoundId))
{
    if (customModelId < 0) return false;
    HandlingMgr::BeginCustomVehicleDef((uint32_t)customModelId, (uint32_t)visualBase, (uint32_t)audioBase, (uint32_t)handlingBase, (int16_t)engineSoundId);
    return true;
}

SCRIPT_API(SetCustomVehicleDff, bool(int customModelId, const std::string& dffUrl, const std::string& dffHash))
{
    return HandlingMgr::SetCustomVehicleDff((uint32_t)customModelId, dffUrl.c_str(), dffHash.c_str());
}

SCRIPT_API(SetCustomVehicleTxd, bool(int customModelId, const std::string& txdUrl, const std::string& txdHash))
{
    return HandlingMgr::SetCustomVehicleTxd((uint32_t)customModelId, txdUrl.c_str(), txdHash.c_str());
}

SCRIPT_API(CommitCustomVehicleDef, bool(int customModelId))
{
    return HandlingMgr::CommitCustomVehicleDef((uint32_t)customModelId);
}

SCRIPT_API(DestroyCustomVehicle, bool(int customModelId))
{
    if (!HandlingMgr::IsCustomVehicle((uint32_t)customModelId)) return false;
    HandlingMgr::SendCustomVehicleDestroyToAll((uint32_t)customModelId);
    HandlingMgr::UnregisterCustomVehicle((uint32_t)customModelId);
    return true;
}

SCRIPT_API(ResetAllHandling, bool(int playerid))
{
    return HandlingMgr::ResetAll((uint16_t)playerid);
}

SCRIPT_API(IsCustomVehicleModel, bool(int modelid))
{
    return HandlingMgr::IsCustomVehicle((uint32_t)modelid);
}

SCRIPT_API(IsVehicleCustom, bool(int vehicleid))
{
    IVehicle* veh = CHandlingCompo::GetVehicleByID(vehicleid);
    if (!veh) return false;
    return HandlingMgr::IsCustomVehicle((uint32_t)veh->getModel());
}