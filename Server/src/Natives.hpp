#pragma once

#include <cstdint>
#include <cstring>
#include <sdk.hpp>

#include "CPlayer.h"
#include "CVehicleManager.hpp"
#include "HandlingEnum.h"
#include "HandlingManager.h"
#include "Hooks.hpp"
#include "extendedveh.h"
#include "CustomVehicleBindingRegistry.h"
#include <Server/Components/Pawn/pawn.hpp>
#include <Server/Components/Pawn/Impl/pawn_natives.hpp>

using namespace NativeHook;

namespace FuncHook
{
inline cell OnCreateVehicleHook(AMX* amx, cell* params, NativeHook::amx_native_fn_t orig)
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	if (compo)
	{
		ICore* core_ = compo->getCore();
		if (core_)
		{
			core_->logLn(LogLevel::Debug, "[ExtendedVeh] Hooked CreateVehicle");
		}
	}
	const int vehicleid = static_cast<int>(orig(amx, params));
	if (vehicleid != INVALID_VEHICLE_ID)
	{
		HandlingMgr::OnCreateVehicle(vehicleid);
	}
	return static_cast<cell>(vehicleid);
}

inline cell OnAddStaticVehicleHook(AMX* amx, cell* params, NativeHook::amx_native_fn_t orig)
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	if (compo)
	{
		ICore* core_ = compo->getCore();
		if (core_)
		{
			core_->logLn(LogLevel::Debug, "[ExtendedVeh] Hooked AddStaticVehicle");
		}
	}
	const int vehicleid = static_cast<int>(orig(amx, params));
	if (vehicleid != INVALID_VEHICLE_ID)
	{
		HandlingMgr::OnCreateVehicle(vehicleid);
	}
	return static_cast<cell>(vehicleid);
}

inline cell OnAddStaticVehicleExHook(AMX* amx, cell* params, NativeHook::amx_native_fn_t orig)
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	if (compo)
	{
		ICore* core_ = compo->getCore();
		if (core_)
		{
			core_->logLn(LogLevel::Debug, "[ExtendedVeh] Hooked AddStaticVehicleEx");
		}
	}
	const int vehicleid = static_cast<int>(orig(amx, params));
	if (vehicleid != INVALID_VEHICLE_ID)
	{
		HandlingMgr::OnCreateVehicle(vehicleid);
	}
	return static_cast<cell>(vehicleid);
}

inline cell OnDestroyVehicleHook(AMX* amx, cell* params, NativeHook::amx_native_fn_t orig)
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	if (compo)
	{
		ICore* core_ = compo->getCore();
		if (core_)
		{
			core_->logLn(LogLevel::Debug, "[ExtendedVeh] Hooked DestroyVehicle");
		}
	}
	return orig(amx, params);
}
}

inline void RegisterNativeHooks()
{
	auto& hooks = NativeHookManager::Instance();

	hooks.RegisterHookByName("CreateVehicle", &FuncHook::OnCreateVehicleHook);
	hooks.RegisterHookByName("AddStaticVehicle", &FuncHook::OnAddStaticVehicleHook);
	hooks.RegisterHookByName("AddStaticVehicleEx", &FuncHook::OnAddStaticVehicleExHook);
	hooks.RegisterHookByName("DestroyVehicle", &FuncHook::OnDestroyVehicleHook);
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
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	if (!compo)
		return false;
	ICore* core_ = compo->getCore();
	if (!core_)
		return false;
	const auto& players_ = core_->getPlayers().players();
	int playerid = player.getID();
	for (auto it = players_.begin(); it != players_.end(); ++it)
	{
		IPlayer* player_ = *it;
		if (player_->getID() == playerid)
		{
			return gPlayers[playerid].hasCHandling();
		}
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
	return HandlingMgr::SetVehicleHandling(static_cast<std::uint16_t>(vehicleid), attrib, value);
}

// native SetVehicleHandlingInt(vehicleid, attrib, value);
SCRIPT_API(SetVehicleHandlingInt, bool(int vehicleid, CHandlingAttrib attrib, int value))
{
	if (GetHandlingAttributeType(attrib) == TYPE_BYTE)
		return HandlingMgr::SetVehicleHandling(static_cast<std::uint16_t>(vehicleid), attrib, (uint8_t)value);

	return HandlingMgr::SetVehicleHandling(static_cast<std::uint16_t>(vehicleid), attrib, (unsigned int)value);
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
	return HandlingMgr::GetVehicleHandling(static_cast<std::uint16_t>(vehicleid), attrib, value);
}

// native GetVehicleHandlingInt(vehicleid, attrib, &value);
SCRIPT_API(GetVehicleHandlingInt, bool(int vehicleid, CHandlingAttrib attrib, unsigned int& value))
{
	value = 0;
	bool ret = false;

	if (GetHandlingAttributeType(attrib) == TYPE_BYTE)
	{
		uint8_t byteVal = 0;
		ret = HandlingMgr::GetVehicleHandling(static_cast<std::uint16_t>(vehicleid), attrib, byteVal);
		value = byteVal;
	}
	else
	{
		ret = HandlingMgr::GetVehicleHandling(static_cast<std::uint16_t>(vehicleid), attrib, value);
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

// native SetPlayerHandlingFloat(playerid, attrib, Float:value);
SCRIPT_API(SetPlayerHandlingFloat, bool(int playerid, CHandlingAttrib attrib, float value))
{
	return HandlingMgr::SetPlayerHandling(static_cast<std::uint16_t>(playerid), attrib, value);
}

// native ResetAllHandlingForPlayer(playerid);
SCRIPT_API(ResetAllHandlingForPlayer, bool(int playerid))
{
	return HandlingMgr::ResetAll(static_cast<std::uint16_t>(playerid));
}

// native SetPlayerHandlingInt(playerid, attrib, value);
SCRIPT_API(SetPlayerHandlingInt, bool(int playerid, CHandlingAttrib attrib, int value))
{
	if (GetHandlingAttributeType(attrib) == TYPE_BYTE)
		return HandlingMgr::SetPlayerHandling(static_cast<std::uint16_t>(playerid), attrib, (uint8_t)value);
	return HandlingMgr::SetPlayerHandling(static_cast<std::uint16_t>(playerid), attrib, (unsigned int)value);
}

// native GetPlayerHandlingFloat(playerid, attrib, &Float:value);
SCRIPT_API(GetPlayerHandlingFloat, bool(int playerid, CHandlingAttrib attrib, float& value))
{
	value = 0.0f;
	return HandlingMgr::GetPlayerHandling(static_cast<std::uint16_t>(playerid), attrib, value);
}

// native GetPlayerHandlingInt(playerid, attrib, &value);
SCRIPT_API(GetPlayerHandlingInt, bool(int playerid, CHandlingAttrib attrib, unsigned int& value))
{
	value = 0;
	bool ret = false;

	if (GetHandlingAttributeType(attrib) == TYPE_BYTE)
	{
		uint8_t byteVal = 0;
		ret = HandlingMgr::GetPlayerHandling(static_cast<std::uint16_t>(playerid), attrib, byteVal);
		value = byteVal;
	}
	else
	{
		ret = HandlingMgr::GetPlayerHandling(static_cast<std::uint16_t>(playerid), attrib, value);
	}
	return ret;
}

// native ResetPlayerHandling(playerid);
SCRIPT_API(ResetPlayerHandling, bool(int playerid))
{
	return HandlingMgr::ResetPlayerHandling(static_cast<std::uint16_t>(playerid));
}

// native BeginCustomVehicleDef(customModelId, visualBase, audioBase, handlingBase, engineSoundId);
SCRIPT_API(BeginCustomVehicleDef, bool(int customModelId, int visualBase, int audioBase, int handlingBase, int engineSoundId))
{
	if (customModelId < 0)
		return false;
	HandlingMgr::BeginCustomVehicleDef(static_cast<std::uint32_t>(customModelId), static_cast<std::uint32_t>(visualBase), static_cast<std::uint32_t>(audioBase), static_cast<std::uint32_t>(handlingBase), static_cast<std::int16_t>(engineSoundId));
	return true;
}

// native SetCustomVehicleDff(customModelId, const dffUrl[], const dffHash[]);
SCRIPT_API(SetCustomVehicleDff, bool(int customModelId, const std::string& dffUrl, const std::string& dffHash))
{
	if (dffUrl.empty() || dffHash.empty())
	{
		return false;
	}
	return HandlingMgr::SetCustomVehicleDff(static_cast<std::uint32_t>(customModelId), dffUrl.c_str(), dffHash.c_str());
}

// native SetCustomVehicleTxd(customModelId, const txdUrl[], const txdHash[]);
SCRIPT_API(SetCustomVehicleTxd, bool(int customModelId, const std::string& txdUrl, const std::string& txdHash))
{
	if (txdUrl.empty() || txdHash.empty())
	{
		return false;
	}
	return HandlingMgr::SetCustomVehicleTxd(static_cast<std::uint32_t>(customModelId), txdUrl.c_str(), txdHash.c_str());
}

// native SetCustomVehicleCol(customModelId, const colUrl[], const colHash[]);
SCRIPT_API(SetCustomVehicleCol, bool(int customModelId, const std::string& colUrl, const std::string& colHash))
{
	if (colUrl.empty() || colHash.empty())
	{
		return false;
	}
	return HandlingMgr::SetCustomVehicleCol(static_cast<std::uint32_t>(customModelId), colUrl.c_str(), colHash.c_str());
}

// native CommitCustomVehicleDef(customModelId);
SCRIPT_API(CommitCustomVehicleDef, bool(int customModelId))
{
	return HandlingMgr::CommitCustomVehicleDef(static_cast<std::uint32_t>(customModelId));
}

// native DestroyCustomVehicle(customModelId);
SCRIPT_API(DestroyCustomVehicle, bool(int customModelId))
{
	if (!HandlingMgr::IsCustomVehicle(static_cast<std::uint32_t>(customModelId)))
		return false;
	HandlingMgr::SendCustomVehicleDestroyToAll(static_cast<std::uint32_t>(customModelId));
	HandlingMgr::UnregisterCustomVehicle(static_cast<std::uint32_t>(customModelId));
	return true;
}

// native ResetAllHandling(playerid);
SCRIPT_API(ResetAllHandling, bool(int playerid))
{
	return HandlingMgr::ResetAll(static_cast<std::uint16_t>(playerid));
}

// native IsCustomVehicleModel(modelid);
SCRIPT_API(IsCustomVehicleModel, bool(int modelid))
{
	return HandlingMgr::IsCustomVehicle(static_cast<std::uint32_t>(modelid));
}

// native IsVehicleCustom(vehicleid);
SCRIPT_API(IsVehicleCustom, bool(int vehicleid))
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	if (!compo)
		return false;
	IVehicle* vehicle = compo->GetVehicleByID(vehicleid);
	if (!vehicle)
		return false;
	return HandlingMgr::IsCustomVehicle(static_cast<std::uint32_t>(vehicle->getModel()));
}

// native BindVehicleModel(vehicleid, customModelId);
SCRIPT_API(BindVehicleModel, bool(int vehicleid, int customModelId))
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	if (!compo)
		return false;
	IVehicle* vehicle = compo->GetVehicleByID(vehicleid);
	if (!vehicle)
		return false;
	CustomVehicleBindingRegistry::Instance().Bind(static_cast<uint16_t>(vehicleid), static_cast<uint32_t>(customModelId));
	return true;
}
