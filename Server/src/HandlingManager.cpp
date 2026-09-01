#include "HandlingManager.h"
#include "Actions.h"
#include "CPlayer.h"
#include "CVehicleManager.hpp"
#include "HandlingDefault.h"
#include "PacketEnum.h"
#include "extendedveh.h"

#include <cstring>
#include <unordered_map>
#include <unordered_set>

#define CHECK_TYPE(attribute, type)                                                                                \
	if (GetHandlingAttributeType(attrib) != type)                                                                  \
	{                                                                                                              \
		core_->logLn(LogLevel::Error, "[ExtendedVeh] Invalid type (%d) specified for attribute %d", type, attrib); \
		return false;                                                                                              \
	}

namespace HandlingMgr
{
std::array<stHandlingEntry, CVehicleMgr::BASE_MAX_VEHICLE_MODELS> gBaseModelHandlings;
std::unordered_map<uint32_t, stHandlingEntry> gCustomModelHandlings;

std::unordered_map<uint16_t, struct stVehicleHandlingEntry> vehicleHandlings;
std::unordered_map<uint16_t, struct stHandlingEntry> playerHandlings; // key = playerid
std::unordered_map<uint32_t, CustomVehicleDef> customVehicleDefs; // key = modelId
std::unordered_set<uint32_t> customVehicleModels;
std::unordered_map<uint32_t, CustomVehicleDef> stagedCustomVehicleDefs;

std::unordered_set<uint16_t> usOutgoingVehicleMods;
std::unordered_set<uint32_t> usOutgoingModelMods;

stHandlingEntry* GetModelHandlingEntry(uint32_t modelid)
{
	if (CVehicleMgr::IsBaseVehicleModel(modelid))
	{
		return &gBaseModelHandlings[CVehicleMgr::GetBaseModelIndex(modelid)];
	}
	auto it = gCustomModelHandlings.find(modelid);
	if (it != gCustomModelHandlings.end())
	{
		return &it->second;
	}
	if (CVehicleMgr::IS_VALID_VEHICLE_MODEL(modelid))
	{
		stHandlingEntry entry;
		HandlingDefault::copyDefaultModelHandling(modelid, &entry.handlingData);
		auto [insertedIt, _] = gCustomModelHandlings.emplace(modelid, std::move(entry));
		return &insertedIt->second;
	}
	return nullptr;
}

/*
 *  INTERNAL FUNCTIONS
 */
void __WriteHandlingEntryToBitStream(NetworkBitStream* bs, const struct stHandlingEntry entry)
{
	bs->Write((uint8_t)entry.handlingModMap.size());

	for (auto const& i : entry.handlingModMap)
	{
		bs->Write((uint8_t)i.first); // attribute
		bs->Write((uint8_t)i.second.type);
		switch (i.second.type)
		{
		case TYPE_BYTE:
			bs->Write(i.second.bval);
			break;
		case TYPE_UINT:
		case TYPE_FLAG:
			bs->Write(i.second.uival);
			break;
		case TYPE_FLOAT:
			bs->Write(i.second.fval);
			break;
		case TYPE_NONE:
			break;
		}
	}
}

void __addMod(struct stHandlingEntry* handling, CHandlingAttrib attribute, const struct stHandlingMod mod)
{
	if (handling->handlingModMap.count(attribute))
		handling->handlingModMap.at(attribute) = mod;
	else
		handling->handlingModMap.emplace(attribute, mod);

	void* offs = GetHandlingAttribPtr(&handling->handlingData, attribute);
	if (!offs)
		return;

	/* write the value to the handling data so we can Get it later on */
	switch (mod.type)
	{
	case TYPE_FLOAT:
		*(float*)offs = mod.fval;
		break;
	case TYPE_UINT:
	case TYPE_FLAG:
		*(unsigned int*)offs = mod.uival;
		break;
	case TYPE_BYTE:
		*(uint8_t*)offs = mod.bval;
		break;
	case TYPE_NONE:
		break;
	}
}

bool __AddModelHandlingMod(uint16_t modelid, CHandlingAttrib attribute, const struct stHandlingMod mod)
{
	if (!CVehicleMgr::IS_VALID_VEHICLE_MODEL(modelid))
		return false;

	stHandlingEntry* entry = GetModelHandlingEntry(modelid);
	if (!entry)
		return false;

	__addMod(entry, attribute, mod);

	usOutgoingModelMods.emplace(modelid);
	return true;
}

bool __AddVehicleHandlingMod(uint16_t vehicleid, CHandlingAttrib attribute, const struct stHandlingMod mod)
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	if (!compo->IsValidVehicle(vehicleid))
		return false;

	auto& vEntry = vehicleHandlings[vehicleid];
	// copy the handling of the model & apply the changed value
	if (vEntry.usesModelHandling)
	{
		vEntry.usesModelHandling = false;
		if (vEntry.modelHandling)
		{
			memcpy(&vEntry.handlingData, &vEntry.modelHandling->handlingData, sizeof(struct tHandlingData));
		}
	}
	__addMod(&vEntry, attribute, mod);

	usOutgoingVehicleMods.emplace(vehicleid);
	return true;
}

/* -------------------------------------------------------------------------------------------------------------------- */

/* We use ProcessTick to broadcast queued modifications all at once instead of spamming with packets */
void ProcessTick()
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core_ = compo->getCore();
	while (!usOutgoingVehicleMods.empty())
	{
		const auto it = usOutgoingVehicleMods.begin();
		uint16_t vehicleid = *it;
		usOutgoingVehicleMods.erase(it);

		auto vIt = vehicleHandlings.find(vehicleid);
		if (!compo->IsValidVehicle(vehicleid) || vIt == vehicleHandlings.end() || vIt->second.usesModelHandling)
		{
			continue;
		}
		struct CHandlingActionPacket p(ACTION_SET_VEHICLE_HANDLING);
		p.data.Write(vehicleid);
		__WriteHandlingEntryToBitStream(&p.data, vIt->second);

		for (IPlayer* player : core_->getPlayers().players())
		{
			player->sendPacket(Span<uint8_t>(p.data.GetData(), p.data.GetNumberOfBytesUsed()), 0, true);
		}
	}

	while (!usOutgoingModelMods.empty())
	{
		const auto it = usOutgoingModelMods.begin();
		uint32_t modelid = *it;
		usOutgoingModelMods.erase(it);

		stHandlingEntry* mEntry = GetModelHandlingEntry(modelid);
		if (!CVehicleMgr::IS_VALID_VEHICLE_MODEL(modelid) || !mEntry || mEntry->handlingModMap.empty())
		{
			continue;
		}

		struct CHandlingActionPacket p(ACTION_SET_MODEL_HANDLING);
		p.data.Write((uint16_t)modelid);
		__WriteHandlingEntryToBitStream(&p.data, *mEntry);

		for (IPlayer* player : core_->getPlayers().players())
		{
			player->sendPacket(Span<uint8_t>(p.data.GetData(), p.data.GetNumberOfBytesUsed()), 0, true);
		}
	}
}

// call right after HandlingDefault::Initialize()
void InitializeModelHandlings()
{
	for (uint16_t i = 0; i < CVehicleMgr::BASE_MAX_VEHICLE_MODELS; i++)
	{
		HandlingDefault::copyDefaultModelHandling(i + 400, &gBaseModelHandlings[i].handlingData);
		gBaseModelHandlings[i].handlingModMap.clear();
	}
	gCustomModelHandlings.clear();
}

void OnCreateVehicle(int vehicleid)
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	IVehicle* pVeh = compo->GetVehicleByID(vehicleid);
	if (pVeh)
	{
		ResetVehicleHandling(*pVeh, false);
	}
}

void OnPlayerConnect(IPlayer& player)
{
	int playerid = player.getID();
	if (!gPlayers[playerid].hasCHandling())
		return;

	for (uint16_t model = 0; model < CVehicleMgr::BASE_MAX_VEHICLE_MODELS; model++)
	{
		if (!gBaseModelHandlings[model].handlingModMap.empty())
		{
			struct CHandlingActionPacket p(ACTION_SET_MODEL_HANDLING);
			p.data.Write((uint16_t)(model + 400));
			__WriteHandlingEntryToBitStream(&p.data, gBaseModelHandlings[model]);
			player.sendPacket(Span<uint8_t>(p.data.GetData(), p.data.GetNumberOfBytesUsed()), 0, false);
		}
	}

	for (const auto& [customModel, entry] : gCustomModelHandlings)
	{
		if (!entry.handlingModMap.empty())
		{
			struct CHandlingActionPacket p(ACTION_SET_MODEL_HANDLING);
			p.data.Write((uint16_t)customModel);
			__WriteHandlingEntryToBitStream(&p.data, entry);
			player.sendPacket(Span<uint8_t>(p.data.GetData(), p.data.GetNumberOfBytesUsed()), 0, false);
		}
	}

	for (const auto& kv : customVehicleDefs)
	{
		SendCustomVehicleDefToPlayer(player, kv.first);
	}
}

void OnPlayerDisconnect(IPlayer& player, PeerDisconnectReason reason)
{
	HandlingMgr::ResetPlayerHandling(player.getID());
}

void OnVehicleStreamIn(IVehicle& vehicle, IPlayer& player)
{
	int vehicleid = vehicle.getID();
	int modelid = vehicle.getModel();
	int forplayerid = player.getID();

	if (IsCustomVehicle(modelid))
	{
		SendCustomVehicleDefToPlayer(player, modelid);
	}

	auto it = vehicleHandlings.find(vehicleid);
	if (it == vehicleHandlings.end() || it->second.handlingModMap.empty() || !gPlayers[forplayerid].hasCHandling())
		return;

	struct CHandlingActionPacket p(ACTION_SET_VEHICLE_HANDLING);
	p.data.Write((uint16_t)vehicleid);
	__WriteHandlingEntryToBitStream(&p.data, it->second);
	player.sendPacket(Span<uint8_t>(p.data.GetData(), p.data.GetNumberOfBytesUsed()), 0, false);
}

/*
 * Resets handling of specified vehicle model to its original default one
 */
bool ResetModelHandling(int modelid)
{
	if (!CVehicleMgr::IS_VALID_VEHICLE_MODEL(modelid))
		return false;

	stHandlingEntry* mEntry = GetModelHandlingEntry(modelid);
	if (!mEntry)
		return false;

	mEntry->handlingModMap.clear();
	HandlingDefault::copyDefaultModelHandling((uint16_t)modelid, &mEntry->handlingData);

	struct CHandlingActionPacket p(ACTION_RESET_MODEL);
	p.data.Write((uint16_t)modelid);

	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core_ = compo->getCore();
	for (IPlayer* player : core_->getPlayers().players())
	{
		player->sendPacket(Span<uint8_t>(p.data.GetData(), p.data.GetNumberOfBytesUsed()), 0, true);
	}
	return true;
}

/*
 * Resets the handling of specified vehicle to its model handling
 */
void ResetVehicleHandling(IVehicle& vehicle, bool sendToPlayers)
{
	int vehicleid = vehicle.getID();
	int modelid = vehicle.getModel();

	auto& vEntry = vehicleHandlings[vehicleid];
	vEntry.handlingModMap.clear();
	vEntry.modelHandling = GetModelHandlingEntry(modelid);
	vEntry.usesModelHandling = true;

	if (sendToPlayers)
	{
		struct CHandlingActionPacket p(ACTION_RESET_VEHICLE);
		p.data.Write((uint16_t)vehicleid);

		ExtendedVehCompo* compo = ExtendedVehCompo::get();
		ICore* core_ = compo->getCore();
		for (IPlayer* player : core_->getPlayers().players())
		{
			player->sendPacket(Span<uint8_t>(p.data.GetData(), p.data.GetNumberOfBytesUsed()), 0, true);
		}
	}
}

/* SET HANDLING FUNCTIONS */

bool SetVehicleHandling(uint16_t vehicleid, CHandlingAttrib attrib, float value)
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core_ = compo->getCore();
	if (!compo->IsValidVehicle(vehicleid) || !CanSetHandlingAttrib(attrib))
		return false;
	CHECK_TYPE(attrib, TYPE_FLOAT)

	if (!IsValidHandlingValue(attrib, value))
		return false;

	struct stHandlingMod mod;
	mod.fval = value;
	mod.type = TYPE_FLOAT;

	return __AddVehicleHandlingMod(vehicleid, attrib, mod);
}

bool SetVehicleHandling(uint16_t vehicleid, CHandlingAttrib attrib, unsigned int value)
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core_ = compo->getCore();
	if (!compo->IsValidVehicle(vehicleid) || !CanSetHandlingAttrib(attrib))
		return false;

	CHandlingAttribType type = GetHandlingAttributeType(attrib);
	if (!(type == TYPE_UINT || type == TYPE_FLAG))
		return false;

	struct stHandlingMod mod;
	mod.uival = value;
	mod.type = TYPE_UINT;
	return __AddVehicleHandlingMod(vehicleid, attrib, mod);
}

bool SetVehicleHandling(uint16_t vehicleid, CHandlingAttrib attrib, uint8_t value)
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core_ = compo->getCore();
	if (!compo->IsValidVehicle(vehicleid) || !CanSetHandlingAttrib(attrib))
		return false;
	CHECK_TYPE(attrib, TYPE_BYTE)

	if (!IsValidHandlingValue(attrib, value))
		return false;

	struct stHandlingMod mod;
	mod.bval = value;
	mod.type = TYPE_BYTE;
	return __AddVehicleHandlingMod(vehicleid, attrib, mod);
}

bool SetModelHandling(uint16_t modelid, CHandlingAttrib attrib, float value)
{
	if (!CVehicleMgr::IS_VALID_VEHICLE_MODEL(modelid) || !CanSetHandlingAttrib(attrib))
		return false;

	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core_ = compo->getCore();
	CHECK_TYPE(attrib, TYPE_FLOAT)

	if (!IsValidHandlingValue(attrib, value))
		return false;

	struct stHandlingMod mod;
	mod.fval = value;
	mod.type = TYPE_FLOAT;
	return __AddModelHandlingMod(modelid, attrib, mod);
}

bool SetModelHandling(uint16_t modelid, CHandlingAttrib attrib, unsigned int value)
{
	if (!CVehicleMgr::IS_VALID_VEHICLE_MODEL(modelid) || !CanSetHandlingAttrib(attrib))
		return false;
	CHandlingAttribType type = GetHandlingAttributeType(attrib);
	if (!(type == TYPE_UINT || type == TYPE_FLAG))
		return false;

	struct stHandlingMod mod;
	mod.uival = value;
	mod.type = TYPE_UINT;
	return __AddModelHandlingMod(modelid, attrib, mod);
}

bool SetModelHandling(uint16_t modelid, CHandlingAttrib attrib, uint8_t value)
{
	if (!CVehicleMgr::IS_VALID_VEHICLE_MODEL(modelid) || !CanSetHandlingAttrib(attrib))
		return false;

	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core_ = compo->getCore();
	CHECK_TYPE(attrib, TYPE_BYTE)

	if (!IsValidHandlingValue(attrib, value))
		return false;

	struct stHandlingMod mod;
	mod.bval = value;
	mod.type = TYPE_BYTE;
	return __AddModelHandlingMod(modelid, attrib, mod);
}

/* GET */

bool GetVehicleHandling(uint16_t vehicleid, CHandlingAttrib attrib, float& ret)
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core_ = compo->getCore();
	if (!compo->IsValidVehicle(vehicleid))
		return false;
	CHECK_TYPE(attrib, TYPE_FLOAT)

	auto it = vehicleHandlings.find(vehicleid);
	if (it == vehicleHandlings.end())
		return false;

	struct tHandlingData* hData = it->second.usesModelHandling
		? (it->second.modelHandling ? &it->second.modelHandling->handlingData : nullptr)
		: &it->second.handlingData;

	if (!hData)
		return false;
	void* ptr = GetHandlingAttribPtr(hData, attrib);
	if (!ptr)
		return false;
	ret = *(float*)ptr;
	return true;
}

bool GetVehicleHandling(uint16_t vehicleid, CHandlingAttrib attrib, unsigned int& ret)
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	if (!compo->IsValidVehicle(vehicleid))
		return false;
	CHandlingAttribType type = GetHandlingAttributeType(attrib);
	if (!(type == TYPE_UINT || type == TYPE_FLAG))
		return false;

	auto it = vehicleHandlings.find(vehicleid);
	if (it == vehicleHandlings.end())
		return false;

	struct tHandlingData* hData = it->second.usesModelHandling
		? (it->second.modelHandling ? &it->second.modelHandling->handlingData : nullptr)
		: &it->second.handlingData;

	if (!hData)
		return false;
	void* ptr = GetHandlingAttribPtr(hData, attrib);
	if (!ptr)
		return false;
	ret = *(unsigned int*)ptr;
	return true;
}

bool GetVehicleHandling(uint16_t vehicleid, CHandlingAttrib attrib, uint8_t& ret)
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core_ = compo->getCore();
	if (!compo->IsValidVehicle(vehicleid))
		return false;
	CHECK_TYPE(attrib, TYPE_BYTE)

	auto it = vehicleHandlings.find(vehicleid);
	if (it == vehicleHandlings.end())
		return false;

	struct tHandlingData* hData = it->second.usesModelHandling
		? (it->second.modelHandling ? &it->second.modelHandling->handlingData : nullptr)
		: &it->second.handlingData;

	if (!hData)
		return false;
	void* ptr = GetHandlingAttribPtr(hData, attrib);
	if (!ptr)
		return false;
	ret = *(uint8_t*)ptr;
	return true;
}

bool GetModelHandling(uint16_t modelid, CHandlingAttrib attrib, float& ret)
{
	if (!CVehicleMgr::IS_VALID_VEHICLE_MODEL(modelid))
		return false;
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core_ = compo->getCore();
	CHECK_TYPE(attrib, TYPE_FLOAT)

	stHandlingEntry* mEntry = GetModelHandlingEntry(modelid);
	if (!mEntry)
		return false;

	void* ptr = GetHandlingAttribPtr(&mEntry->handlingData, attrib);
	if (!ptr)
		return false;
	ret = *(float*)ptr;
	return true;
}

bool GetModelHandling(uint16_t modelid, CHandlingAttrib attrib, unsigned int& ret)
{
	if (!CVehicleMgr::IS_VALID_VEHICLE_MODEL(modelid))
		return false;

	CHandlingAttribType type = GetHandlingAttributeType(attrib);
	if (!(type == TYPE_UINT || type == TYPE_FLAG))
		return false;

	stHandlingEntry* mEntry = GetModelHandlingEntry(modelid);
	if (!mEntry)
		return false;

	void* ptr = GetHandlingAttribPtr(&mEntry->handlingData, attrib);
	if (!ptr)
		return false;
	ret = *(unsigned int*)ptr;
	return true;
}

bool GetModelHandling(uint16_t modelid, CHandlingAttrib attrib, uint8_t& ret)
{
	if (!CVehicleMgr::IS_VALID_VEHICLE_MODEL(modelid))
		return false;
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core_ = compo->getCore();
	CHECK_TYPE(attrib, TYPE_BYTE)

	stHandlingEntry* mEntry = GetModelHandlingEntry(modelid);
	if (!mEntry)
		return false;

	void* ptr = GetHandlingAttribPtr(&mEntry->handlingData, attrib);
	if (!ptr)
		return false;
	ret = *(uint8_t*)ptr;
	return true;
}

bool GetDefaultHandling(uint16_t modelid, CHandlingAttrib attrib, float& ret)
{
	if (!CVehicleMgr::IS_VALID_VEHICLE_MODEL(modelid))
		return false;
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core_ = compo->getCore();
	CHECK_TYPE(attrib, TYPE_FLOAT)

	struct tHandlingData* pHandl = HandlingDefault::getDefaultModelHandling(modelid);
	if (pHandl == nullptr)
		return false;
	void* ptr = GetHandlingAttribPtr(pHandl, attrib);
	if (!ptr)
		return false;
	ret = *(float*)ptr;
	return true;
}

bool GetDefaultHandling(uint16_t modelid, CHandlingAttrib attrib, unsigned int& ret)
{
	if (!CVehicleMgr::IS_VALID_VEHICLE_MODEL(modelid))
		return false;

	CHandlingAttribType type = GetHandlingAttributeType(attrib);
	if (!(type == TYPE_UINT || type == TYPE_FLAG))
		return false;

	struct tHandlingData* pHandl = HandlingDefault::getDefaultModelHandling(modelid);
	if (pHandl == nullptr)
		return false;
	void* ptr = GetHandlingAttribPtr(pHandl, attrib);
	if (!ptr)
		return false;
	ret = *(unsigned int*)ptr;
	return true;
}

bool GetDefaultHandling(uint16_t modelid, CHandlingAttrib attrib, uint8_t& ret)
{
	if (!CVehicleMgr::IS_VALID_VEHICLE_MODEL(modelid))
		return false;
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core_ = compo->getCore();
	CHECK_TYPE(attrib, TYPE_BYTE)

	struct tHandlingData* pHandl = HandlingDefault::getDefaultModelHandling(modelid);
	if (pHandl == nullptr)
		return false;
	void* ptr = GetHandlingAttribPtr(pHandl, attrib);
	if (!ptr)
		return false;
	ret = *(uint8_t*)ptr;
	return true;
}

bool SetPlayerHandling(uint16_t playerid, CHandlingAttrib attrib, float value)
{
	if (!IS_VALID_PLAYERID(playerid) || !CanSetHandlingAttrib(attrib))
		return false;
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core_ = compo->getCore();
	CHECK_TYPE(attrib, TYPE_FLOAT);

	if (!IsValidHandlingValue(attrib, value))
		return false;

	if (playerHandlings.find(playerid) == playerHandlings.end())
	{
		playerHandlings[playerid].handlingData = gBaseModelHandlings[0].handlingData;
		playerHandlings[playerid].handlingModMap.clear();
	}

	struct stHandlingMod mod;
	mod.fval = value;
	mod.type = TYPE_FLOAT;
	__addMod(&playerHandlings[playerid], attrib, mod);

	if (compo)
	{
		struct CHandlingActionPacket p(ACTION_SET_PLAYER_HANDLING);
		p.data.Write(playerid);
		__WriteHandlingEntryToBitStream(&p.data, playerHandlings[playerid]);
		IPlayer* player = compo->GetPlayerByID(playerid);
		if (player)
		{
			player->sendPacket(Span<uint8_t>(p.data.GetData(), p.data.GetNumberOfBytesUsed()), 0, true);
		}
	}
	return true;
}

bool SetPlayerHandling(uint16_t playerid, CHandlingAttrib attrib, unsigned int value)
{
	if (!IS_VALID_PLAYERID(playerid) || !CanSetHandlingAttrib(attrib))
		return false;
	CHandlingAttribType type = GetHandlingAttributeType(attrib);
	if (!(type == TYPE_UINT || type == TYPE_FLAG))
		return false;

	if (playerHandlings.find(playerid) == playerHandlings.end())
	{
		playerHandlings[playerid].handlingData = gBaseModelHandlings[0].handlingData;
		playerHandlings[playerid].handlingModMap.clear();
	}

	struct stHandlingMod mod;
	mod.uival = value;
	mod.type = TYPE_UINT;
	__addMod(&playerHandlings[playerid], attrib, mod);

	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	if (compo)
	{
		struct CHandlingActionPacket p(ACTION_SET_PLAYER_HANDLING);
		p.data.Write(playerid);
		__WriteHandlingEntryToBitStream(&p.data, playerHandlings[playerid]);
		IPlayer* player = compo->GetPlayerByID(playerid);
		if (player)
		{
			player->sendPacket(Span<uint8_t>(p.data.GetData(), p.data.GetNumberOfBytesUsed()), 0, true);
		}
	}
	return true;
}

bool SetPlayerHandling(uint16_t playerid, CHandlingAttrib attrib, uint8_t value)
{
	if (!IS_VALID_PLAYERID(playerid) || !CanSetHandlingAttrib(attrib))
		return false;
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core_ = compo->getCore();
	CHECK_TYPE(attrib, TYPE_BYTE);
	if (!IsValidHandlingValue(attrib, value))
		return false;

	if (playerHandlings.find(playerid) == playerHandlings.end())
	{
		playerHandlings[playerid].handlingData = gBaseModelHandlings[0].handlingData;
		playerHandlings[playerid].handlingModMap.clear();
	}

	struct stHandlingMod mod;
	mod.bval = value;
	mod.type = TYPE_BYTE;
	__addMod(&playerHandlings[playerid], attrib, mod);

	if (compo)
	{
		struct CHandlingActionPacket p(ACTION_SET_PLAYER_HANDLING);
		p.data.Write(playerid);
		__WriteHandlingEntryToBitStream(&p.data, playerHandlings[playerid]);
		IPlayer* player = compo->GetPlayerByID(playerid);
		if (player)
		{
			player->sendPacket(Span<uint8_t>(p.data.GetData(), p.data.GetNumberOfBytesUsed()), 0, true);
		}
	}
	return true;
}

bool ResetPlayerHandling(uint16_t playerid)
{
	auto it = playerHandlings.find(playerid);
	if (it == playerHandlings.end())
		return false;
	playerHandlings.erase(it);

	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	if (compo)
	{
		struct CHandlingActionPacket p(ACTION_RESET_PLAYER_HANDLING);
		p.data.Write(playerid);
		IPlayer* player = compo->GetPlayerByID(playerid);
		if (player)
		{
			player->sendPacket(Span<uint8_t>(p.data.GetData(), p.data.GetNumberOfBytesUsed()), 0, true);
		}
	}
	return true;
}

bool GetPlayerHandling(uint16_t playerid, CHandlingAttrib attrib, float& ret)
{
	auto it = playerHandlings.find(playerid);
	if (it == playerHandlings.end())
		return false;
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core_ = compo->getCore();
	CHECK_TYPE(attrib, TYPE_FLOAT);
	void* ptr = GetHandlingAttribPtr(&it->second.handlingData, attrib);
	if (!ptr)
		return false;
	ret = *(float*)ptr;
	return true;
}

bool GetPlayerHandling(uint16_t playerid, CHandlingAttrib attrib, unsigned int& ret)
{
	auto it = playerHandlings.find(playerid);
	if (it == playerHandlings.end())
		return false;
	CHandlingAttribType type = GetHandlingAttributeType(attrib);
	if (!(type == TYPE_UINT || type == TYPE_FLAG))
		return false;
	void* ptr = GetHandlingAttribPtr(&it->second.handlingData, attrib);
	if (!ptr)
		return false;
	ret = *(unsigned int*)ptr;
	return true;
}

bool GetPlayerHandling(uint16_t playerid, CHandlingAttrib attrib, uint8_t& ret)
{
	auto it = playerHandlings.find(playerid);
	if (it == playerHandlings.end())
		return false;
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core_ = compo->getCore();
	CHECK_TYPE(attrib, TYPE_BYTE);
	void* ptr = GetHandlingAttribPtr(&it->second.handlingData, attrib);
	if (!ptr)
		return false;
	ret = *(uint8_t*)ptr;
	return true;
}

bool ResetAll(uint16_t playerid)
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	IPlayer* player = compo->GetPlayerByID(playerid);
	if (!player)
		return false;

	for (auto& kv : vehicleHandlings)
	{
		if (kv.second.handlingModMap.empty())
			continue;
		struct CHandlingActionPacket p(ACTION_RESET_VEHICLE);
		p.data.Write(kv.first);
		player->sendPacket(Span<uint8_t>(p.data.GetData(), p.data.GetNumberOfBytesUsed()), 0, true);
	}

	for (size_t i = 0; i < gBaseModelHandlings.size(); ++i)
	{
		if (gBaseModelHandlings[i].handlingModMap.empty())
			continue;
		struct CHandlingActionPacket p(ACTION_RESET_MODEL);
		p.data.Write((uint16_t)(i + 400));
		player->sendPacket(Span<uint8_t>(p.data.GetData(), p.data.GetNumberOfBytesUsed()), 0, true);
	}

	for (const auto& [customModel, entry] : gCustomModelHandlings)
	{
		if (entry.handlingModMap.empty())
			continue;
		struct CHandlingActionPacket p(ACTION_RESET_MODEL);
		p.data.Write((uint16_t)customModel);
		player->sendPacket(Span<uint8_t>(p.data.GetData(), p.data.GetNumberOfBytesUsed()), 0, true);
	}

	ResetPlayerHandling(playerid);
	return true;
}

void RegisterCustomVehicle(uint32_t customModelId, uint32_t visualBase, uint32_t audioBase, uint32_t handlingBase,
	int16_t engineSoundId,
	const char* dffUrl, const char* dffHash,
	const char* txdUrl, const char* txdHash)
{
	CustomVehicleDef def;
	def.customModelId = customModelId;
	def.visualBaseModelId = visualBase;
	def.audioBaseModelId = audioBase;
	def.handlingBaseModelId = handlingBase;
	def.engineSoundId = engineSoundId;
	std::strncpy(def.dffUrl, dffUrl ? dffUrl : "", sizeof(def.dffUrl) - 1);
	def.dffUrl[sizeof(def.dffUrl) - 1] = '\0';
	std::strncpy(def.dffHash, dffHash ? dffHash : "", sizeof(def.dffHash) - 1);
	def.dffHash[sizeof(def.dffHash) - 1] = '\0';
	std::strncpy(def.txdUrl, txdUrl ? txdUrl : "", sizeof(def.txdUrl) - 1);
	def.txdUrl[sizeof(def.txdUrl) - 1] = '\0';
	std::strncpy(def.txdHash, txdHash ? txdHash : "", sizeof(def.txdHash) - 1);
	def.txdHash[sizeof(def.txdHash) - 1] = '\0';

	customVehicleDefs[customModelId] = def;
	customVehicleModels.insert(customModelId);
	CVehicleMgr::VehicleRegistry::Get().RegisterCustomModel(customModelId);

	stHandlingEntry& entry = gCustomModelHandlings[customModelId];
	HandlingDefault::copyDefaultModelHandling(handlingBase, &entry.handlingData);
	entry.handlingModMap.clear();
}

void UnregisterCustomVehicle(uint32_t customModelId)
{
	customVehicleDefs.erase(customModelId);
	customVehicleModels.erase(customModelId);
	gCustomModelHandlings.erase(customModelId);
	CVehicleMgr::VehicleRegistry::Get().UnregisterCustomModel(customModelId);
	SendCustomVehicleDestroyToAll(customModelId);
}

void BeginCustomVehicleDef(uint32_t customModelId, uint32_t visualBase, uint32_t audioBase, uint32_t handlingBase, int16_t engineSoundId)
{
	CustomVehicleDef def {};
	def.customModelId = customModelId;
	def.visualBaseModelId = visualBase;
	def.audioBaseModelId = audioBase;
	def.handlingBaseModelId = handlingBase;
	def.engineSoundId = engineSoundId;
	stagedCustomVehicleDefs[customModelId] = def;
}

bool SetCustomVehicleDff(uint32_t customModelId, const char* dffUrl, const char* dffHash)
{
	auto it = stagedCustomVehicleDefs.find(customModelId);
	if (it == stagedCustomVehicleDefs.end())
		return false;

	std::strncpy(it->second.dffUrl, dffUrl ? dffUrl : "", sizeof(it->second.dffUrl) - 1);
	it->second.dffUrl[sizeof(it->second.dffUrl) - 1] = '\0';
	std::strncpy(it->second.dffHash, dffHash ? dffHash : "", sizeof(it->second.dffHash) - 1);
	it->second.dffHash[sizeof(it->second.dffHash) - 1] = '\0';
	return true;
}

bool SetCustomVehicleTxd(uint32_t customModelId, const char* txdUrl, const char* txdHash)
{
	auto it = stagedCustomVehicleDefs.find(customModelId);
	if (it == stagedCustomVehicleDefs.end())
		return false;

	std::strncpy(it->second.txdUrl, txdUrl ? txdUrl : "", sizeof(it->second.txdUrl) - 1);
	it->second.txdUrl[sizeof(it->second.txdUrl) - 1] = '\0';
	std::strncpy(it->second.txdHash, txdHash ? txdHash : "", sizeof(it->second.txdHash) - 1);
	it->second.txdHash[sizeof(it->second.txdHash) - 1] = '\0';
	return true;
}

bool SetCustomVehicleCol(uint32_t customModelId, const char* colUrl, const char* colHash)
{
	auto it = stagedCustomVehicleDefs.find(customModelId);
	if (it == stagedCustomVehicleDefs.end())
		return false;

	std::strncpy(it->second.colUrl, colUrl ? colUrl : "", sizeof(it->second.colUrl) - 1);
	it->second.colUrl[sizeof(it->second.colUrl) - 1] = '\0';
	std::strncpy(it->second.colHash, colHash ? colHash : "", sizeof(it->second.colHash) - 1);
	it->second.colHash[sizeof(it->second.colHash) - 1] = '\0';
	return true;
}

bool CommitCustomVehicleDef(uint32_t customModelId)
{
	auto it = stagedCustomVehicleDefs.find(customModelId);
	if (it == stagedCustomVehicleDefs.end())
		return false;

	customVehicleDefs[customModelId] = it->second;
	stagedCustomVehicleDefs.erase(it);

	SendCustomVehicleDefToAll(customModelId);
	return true;
}

bool IsCustomVehicle(uint32_t modelId)
{
	return customVehicleModels.find(modelId) != customVehicleModels.end();
}

void SendCustomVehicleDefToPlayer(IPlayer& player, uint32_t modelId)
{
	auto it = customVehicleDefs.find(modelId);
	if (it == customVehicleDefs.end())
		return;

	NetworkBitStream bs;
	const auto& def = it->second;
	bs.Write(def.customModelId);
	bs.Write(def.visualBaseModelId);
	bs.Write(def.audioBaseModelId);
	bs.Write(def.handlingBaseModelId);
	bs.Write(def.engineSoundId);
	bs.Write(def.dffUrl, 128);
	bs.Write(def.dffHash, 65);
	bs.Write(def.txdUrl, 128);
	bs.Write(def.txdHash, 65);
	bs.Write(def.colUrl, 128);
	bs.Write(def.colHash, 65);
	player.sendRPC(CHandlingRPCID::CUSTOM_VEHICLE_DEF, Span<uint8_t>(bs.GetData(), bs.GetNumberOfBytesUsed()), 0, false);
}

void SendCustomVehicleDefToAll(uint32_t modelId)
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core = compo->getCore();
	for (IPlayer* player : core->getPlayers().players())
	{
		SendCustomVehicleDefToPlayer(*player, modelId);
	}
}

void SendCustomVehicleDestroyToPlayer(IPlayer& player, uint32_t modelId)
{
	NetworkBitStream bs;
	bs.Write(modelId);
	player.sendRPC(CHandlingRPCID::DESTROY_CUSTOM_VEHICLE_MODEL, Span<uint8_t>(bs.GetData(), bs.GetNumberOfBytesUsed()), 0, false);
}

void SendCustomVehicleDestroyToAll(uint32_t modelId)
{
	ExtendedVehCompo* compo = ExtendedVehCompo::get();
	ICore* core_ = compo->getCore();
	for (IPlayer* player : core_->getPlayers().players())
	{
		SendCustomVehicleDestroyToPlayer(*player, modelId);
	}
}
}
