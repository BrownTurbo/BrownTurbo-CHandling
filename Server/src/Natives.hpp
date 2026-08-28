#pragma once

#include <sdk.hpp>
#include "CPlayer.h"
#include "CVehicleManager.hpp"
#include "HandlingEnum.h"
#include "HandlingManager.h"
#include "Hooks.hpp"
#include "extendedveh.h"
#include <Server/Components/Pawn/Impl/pawn_natives.hpp>
#include <Server/Components/Pawn/pawn.hpp>
#include <cstring>

// Vehicle related funcs hooks
using namespace NativeHook;

void RegisterNativeHooks();
void RegisterNativeHooks()
{
	NativeHookManager::Instance().RegisterHookByName("CreateVehicle", [](AMX* amx, cell* params, NativeHook::amx_native_fn_t orig) -> cell {
		ExtendedVehCompo* compo = ExtendedVehCompo::get();
		ICore* core_ = compo->getCore();
		if (core_) {
			core_->logLn(LogLevel::Debug, "[ExtendedVeh] Hooked CreateVehicle");
		}
		int vehicleid = orig(amx, params);
		if (vehicleid != INVALID_VEHICLE_ID) {
			HandlingMgr::OnCreateVehicle(vehicleid);
		}
		return static_cast<cell>(vehicleid);
	});

	NativeHookManager::Instance().RegisterHookByName("AddStaticVehicle", [](AMX* amx, cell* params, NativeHook::amx_native_fn_t orig) -> cell {
		ExtendedVehCompo* compo = ExtendedVehCompo::get();
		ICore* core_ = compo->getCore();
		if (core_) {
			core_->logLn(LogLevel::Debug, "[ExtendedVeh] Hooked AddStaticVehicle");
		}
		int vehicleid = orig(amx, params);
		if (vehicleid != INVALID_VEHICLE_ID) {
			HandlingMgr::OnCreateVehicle(vehicleid);
		}
		return static_cast<cell>(vehicleid);
	});

	NativeHookManager::Instance().RegisterHookByName("AddStaticVehicleEx", [](AMX* amx, cell* params, NativeHook::amx_native_fn_t orig) -> cell {
		ExtendedVehCompo* compo = ExtendedVehCompo::get();
		ICore* core_ = compo->getCore();
		if (core_) {
			core_->logLn(LogLevel::Debug, "[ExtendedVeh] Hooked AddStaticVehicleEx");
		}
		int vehicleid = orig(amx, params);
		if (vehicleid != INVALID_VEHICLE_ID) {
			HandlingMgr::OnCreateVehicle(vehicleid);
		}
		return static_cast<cell>(vehicleid);
	});

	NativeHookManager::Instance().RegisterHookByName("DestroyVehicle", [](AMX* amx, cell* params, NativeHook::amx_native_fn_t orig) -> cell {
		ExtendedVehCompo* compo = ExtendedVehCompo::get();
		ICore* core_ = compo->getCore();
		if (core_) {
			core_->logLn(LogLevel::Debug, "[ExtendedVeh] Hooked DestroyVehicle");
		}
		int ret = orig(amx, params);
		return static_cast<cell>(ret);
	});
}

// Vehicle handling related funcs
// native GetHandlingAttribType(attrib);
SCRIPT_API(GetHandlingAttribType, cell(int attr))
{
	CHandlingAttrib handlingAttr = static_cast<CHandlingAttrib>(attr);
	CHandlingAttribType type = GetHandlingAttributeType(handlingAttr);
	return static_cast<cell>(type);
}

// native IsPlayerUsingCHandling(playerid);
SCRIPT_API(IsPlayerUsingCHandling, bool(IPlayer& player))
{
	if (&player != nullptr) {
		int playerid = player.getID();
		return gPlayers[playerid].hasCHandling();
	}
	return false;
}

// native ResetModelHandling(modelid);
SCRIPT_API(ResetModelHandling, bool(int modelid))
{
	return HandlingMgr::ResetModelHandling(modelid);
}

// native ResetVehicleHandling(vehicleid);
SCRIPT_API(ResetVehicleHandling, bool(IVehicle& vehicle))
{
	HandlingMgr::ResetVehicleHandling(vehicle);
	return true;
}

// native SetVehicleHandlingFloat(vehicleid, attrib, Float:value);
SCRIPT_API(SetVehicleHandlingFloat, bool(int vehicleid, CHandlingAttrib attrib, float value))
{
	return HandlingMgr::SetVehicleHandling((uint16_t)vehicleid, attrib, value);
}

// native SetVehicleHandlingInt(vehicleid, attrib, value);
SCRIPT_API(SetVehicleHandlingInt, bool(int vehicleid, CHandlingAttrib attrib, int value))
{
	if (GetHandlingAttributeType(attrib) == TYPE_BYTE)
		return HandlingMgr::SetVehicleHandling((uint16_t)vehicleid, attrib, (uint8_t)value);

	return HandlingMgr::SetVehicleHandling((uint16_t)vehicleid, attrib, (unsigned int)value);
}

// native SetPlayerHandlingFloat(playerid, attrib, Flost:value);
SCRIPT_API(SetModelHandlingFloat, bool(int modelid, CHandlingAttrib attrib, float value))
{
	return HandlingMgr::SetModelHandling((uint16_t)modelid, attrib, value);
}

// native SetModelHandlingInt(modelid, attrib, value);
SCRIPT_API(SetModelHandlingInt, bool(int modelid, CHandlingAttrib attrib, int value))
{
	if (GetHandlingAttributeType(attrib) == TYPE_BYTE)
		return HandlingMgr::SetModelHandling((uint16_t)modelid, attrib, (uint8_t)value);

	return HandlingMgr::SetModelHandling((uint16_t)modelid, attrib, (unsigned int)value);
}

// native GetVehicleHandlingFloat(vehicleid, attrib, &Float:value);
SCRIPT_API(GetVehicleHandlingFloat, bool(int vehicleid, CHandlingAttrib attrib, float& value))
{
	value = 0.0f;
	return HandlingMgr::GetVehicleHandling((uint16_t)vehicleid, attrib, value);
}

// native GetVehicleHandlingInt(vehicleid, attrib, &value);
SCRIPT_API(GetVehicleHandlingInt, bool(int vehicleid, CHandlingAttrib attrib, unsigned int& value))
{
	value = 0;
	bool ret = false;

	if (GetHandlingAttributeType(attrib) == TYPE_BYTE) {
		uint8_t byteVal = 0;
		ret = HandlingMgr::GetVehicleHandling((uint16_t)vehicleid, attrib, byteVal);
		value = byteVal;
	} else {
		ret = HandlingMgr::GetVehicleHandling((uint16_t)vehicleid, attrib, value);
	}
	return ret;
}

// native GetModelHandlingFloat(modelid, attrib, &Float:value);
SCRIPT_API(GetModelHandlingFloat, bool(int modelid, CHandlingAttrib attrib, float& value))
{
	value = 0.0f;
	return HandlingMgr::GetModelHandling((uint16_t)modelid, attrib, value);
}

// native GetModelHandlingInt(modelid, attrib, &value);
SCRIPT_API(GetModelHandlingInt, bool(int modelid, CHandlingAttrib attrib, unsigned int& value))
{
	value = 0;
	bool ret = false;

	if (GetHandlingAttributeType(attrib) == TYPE_BYTE) {
		uint8_t byteVal = 0;
		ret = HandlingMgr::GetModelHandling((uint16_t)modelid, attrib, byteVal);
		value = byteVal;
	} else {
		ret = HandlingMgr::GetModelHandling((uint16_t)modelid, attrib, value);
	}
	return ret;
}

// native GetDefaultHandlingFloat(modelid, attrib, &Float:value);
SCRIPT_API(GetDefaultHandlingFloat, bool(int modelid, CHandlingAttrib attrib, float& value))
{
	value = 0.0f;
	return HandlingMgr::GetDefaultHandling((uint16_t)modelid, attrib, value);
}

// native GetDefaultHandlingInt(modelid, attrib, &value);
SCRIPT_API(GetDefaultHandlingInt, bool(int modelid, CHandlingAttrib attrib, unsigned int& value))
{
	bool ret = false;

	if (GetHandlingAttributeType(attrib) == TYPE_BYTE) {
		uint8_t byteVal = 0;
		ret = HandlingMgr::GetDefaultHandling((uint16_t)modelid, attrib, byteVal);
		value = byteVal;
	} else {
		ret = HandlingMgr::GetDefaultHandling((uint16_t)modelid, attrib, value);
	}
	return ret;
}

// native SetPlayerHandlingFloat(playerid, attrib, Float:value);
SCRIPT_API(SetPlayerHandlingFloat, bool(int playerid, CHandlingAttrib attrib, float value))
{
	return HandlingMgr::SetPlayerHandling((uint16_t)playerid, attrib, value);
}

// native ResetAllHandlingForPlayer(playerid);
SCRIPT_API(ResetAllHandlingForPlayer, bool(int playerid))
{
	return HandlingMgr::ResetAll((uint16_t)playerid);
}

// native SetPlayerHandlingInt(playerid, attrib, value);
SCRIPT_API(SetPlayerHandlingInt, bool(int playerid, CHandlingAttrib attrib, int value))
{
	if (GetHandlingAttributeType(attrib) == TYPE_BYTE)
		return HandlingMgr::SetPlayerHandling((uint16_t)playerid, attrib, (uint8_t)value);
	return HandlingMgr::SetPlayerHandling((uint16_t)playerid, attrib, (unsigned int)value);
}

// native GetPlayerHandlingFloat(playerid, attrib, &Float:value);
SCRIPT_API(GetPlayerHandlingFloat, bool(int playerid, CHandlingAttrib attrib, float& value))
{
	value = 0.0f;
	return HandlingMgr::GetPlayerHandling((uint16_t)playerid, attrib, value);
}

// native GetPlayerHandlingInt(playerid, attrib, &value);
SCRIPT_API(GetPlayerHandlingInt, bool(int playerid, CHandlingAttrib attrib, unsigned int& value))
{
	value = 0;
	bool ret = false;

	if (GetHandlingAttributeType(attrib) == TYPE_BYTE) {
		uint8_t byteVal = 0;
		ret = HandlingMgr::GetPlayerHandling((uint16_t)playerid, attrib, byteVal);
		value = byteVal;
	} else {
		ret = HandlingMgr::GetPlayerHandling((uint16_t)playerid, attrib, value);
	}
	return ret;
}

// native ResetPlayerHandling(playerid);
SCRIPT_API(ResetPlayerHandling, bool(int playerid))
{
	return HandlingMgr::ResetPlayerHandling((uint16_t)playerid);
}

// native BeginCustomVehicleDef(customModelId, visualBase, audioBase, handlingBase, engineSoundId);
SCRIPT_API(BeginCustomVehicleDef, bool(int customModelId, int visualBase, int audioBase, int handlingBase, int engineSoundId))
{
	if (customModelId < 0)
		return false;
	HandlingMgr::BeginCustomVehicleDef((uint32_t)customModelId, (uint32_t)visualBase, (uint32_t)audioBase, (uint32_t)handlingBase, (int16_t)engineSoundId);
	return true;
}

// native SetCustomVehicleDff(customModelId, const dffUrl[], const dffHash[]);
SCRIPT_API(SetCustomVehicleDff, bool(int customModelId, const std::string& dffUrl, const std::string& dffHash))
{
	return HandlingMgr::SetCustomVehicleDff((uint32_t)customModelId, dffUrl.c_str(), dffHash.c_str());
}

// native SetCustomVehicleTxd(customModelId, const txdUrl[], const txdHash[]);
SCRIPT_API(SetCustomVehicleTxd, bool(int customModelId, const std::string& txdUrl, const std::string& txdHash))
{
	return HandlingMgr::SetCustomVehicleTxd((uint32_t)customModelId, txdUrl.c_str(), txdHash.c_str());
}

// native SetCustomVehicleCol(customModelId, const colUrl[], const colHash[]);
SCRIPT_API(SetCustomVehicleCol, bool(int customModelId, const std::string& colUrl, const std::string& colHash))
{
	return HandlingMgr::SetCustomVehicleCol((uint32_t)customModelId, colUrl.c_str(), colHash.c_str());
}

// native CommitCustomVehicleDef(customModelId);
SCRIPT_API(CommitCustomVehicleDef, bool(int customModelId))
{
	return HandlingMgr::CommitCustomVehicleDef((uint32_t)customModelId);
}

// native DestroyCustomVehicle(customModelId);
SCRIPT_API(DestroyCustomVehicle, bool(int customModelId))
{
	if (!HandlingMgr::IsCustomVehicle((uint32_t)customModelId))
		return false;
	HandlingMgr::SendCustomVehicleDestroyToAll((uint32_t)customModelId);
	HandlingMgr::UnregisterCustomVehicle((uint32_t)customModelId);
	return true;
}

// native ResetAllHandling(playerid);
SCRIPT_API(ResetAllHandling, bool(int playerid))
{
	return HandlingMgr::ResetAll((uint16_t)playerid);
}

// native IsCustomVehicleModel(modelid);
SCRIPT_API(IsCustomVehicleModel, bool(int modelid))
{
	return HandlingMgr::IsCustomVehicle((uint32_t)modelid);
}

// native IsVehicleCustom(vehicleid);
SCRIPT_API(IsVehicleCustom, bool(int vehicleid))
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	IVehicle* vehicle = compo->GetVehicleByID(vehicleid);
	if (!vehicle)
		return false;
	return HandlingMgr::IsCustomVehicle((uint32_t)vehicle->getModel());
}

// native BindVehicleModel(vehicleid, customModelId);
SCRIPT_API(BindVehicleModel, bool(int vehicleid, int customModelId))
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
    IVehicle* vehicle = compo->GetVehicleByID(vehicleid);
    if (!vehicle)
        return false;

    CustomVehicleBindingRegistry::Instance().Bind(static_cast<uint16_t>(vehicleid), static_cast<uint32_t>(customModelId));
    return true;
}
