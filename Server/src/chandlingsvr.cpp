#include "chandlingsvr.h"

#include "Hooks.hpp"
#include "CPlayer.h"
#include "Actions.h"
#include "Natives.hpp"
#include "PacketEnum.h"
#include "HandlingManager.h"
#include "HandlingDefault.h"

#include <pawn-natives/NativeFunc.hpp>
namespace NativeHook {
    std::unordered_map<amx_native_fn_t, std::function<cell(AMX *, cell *, amx_native_fn_t)>> NativeHookManager::g_hookMap;
}
PawnLookup* getAmxLookups()
{
    static PawnLookup lookups;
    return &lookups;
}
#include <pawn-natives/NativesMain.hpp>

using namespace NativeHook;

ICore *core_{};
IPawnComponent *pawn_component_{};
IVehiclesComponent *vehicles_ = nullptr;

StringView CHandlingCompo::componentName() const
{
	return "CHandling";
}

SemanticVersion CHandlingCompo::componentVersion() const
{
	return SemanticVersion(CHANDLING_VERSION_MAJOR, CHANDLING_VERSION_MINOR, CHANDLING_VERSION_PATCH, 0);
}

void CHandlingCompo::onLoad(ICore *c)
{
	core_ = c;

	getCore() = c;
	get() = this;

	core_->getPlayers().getPlayerConnectDispatcher().addEventHandler(this);
}

IVehicle* CHandlingCompo::GetVehicleByID(int vehicleid)
{
	if (!CVehicleMgr::IS_VALID_VEHICLEID(vehicleid))
		return nullptr;

	if (get() && get()->vehicles_)
	{
		return get()->vehicles_->get(vehicleid);
	}
	return nullptr;
}

bool CHandlingCompo::IsValidVehicle(int vehicleid)
{
	return GetVehicleByID(vehicleid) != nullptr;
}

IPlayer* CHandlingCompo::GetPlayerByID(int playerid)
{
	if (!IS_VALID_PLAYERID(playerid))
		return nullptr;

	ICore* core = getCore();
	if (!core)
		return nullptr;

	const auto& players_ = core->getPlayers().players();
	for (auto it = players_.begin(); it != players_.end(); ++it)
	{
		IPlayer* player = *it;
		if (player && player->getID() == playerid)
		{
			return player;
		}
	}
	return nullptr;
}

void CHandlingCompo::onInit(IComponentList *components)
{
	StringView name = componentName();
	pawn_component_ = components->queryComponent<IPawnComponent>();
	if (!pawn_component_)
	{
		core_->logLn(LogLevel::Error,
					 "Error loading component %.*s: Pawn component not loaded",
					 name.length(), name.data());
		return;
	}

	core_->getEventDispatcher().addEventHandler(this);
	if (pawn_component_)
	{
		pawn_component_->getEventDispatcher().addEventHandler(this);
		AMX_EXPORTS_DTA = const_cast<void **>(pawn_component_->getAmxFunctions().data());
	}

	vehicles_ = components->queryComponent<IVehiclesComponent>();
	if (!vehicles_)
	{
		core_->logLn(LogLevel::Error,
					 "Error loading component %.*s: Vehicles component not loaded",
					 name.length(), name.data());
		return;
	}
	if (vehicles_)
	{
		vehicles_->getPoolEventDispatcher().addEventHandler(this);
	}

	for (auto network : core_->getNetworks())
	{
		network->getInEventDispatcher().addEventHandler(this);
		network->getOutEventDispatcher().addEventHandler(this);
	}
}

void CHandlingCompo::onAmxLoad(IPawnScript &script)
{
	HandlingDefault::Initialize();
	HandlingMgr::InitializeModelHandlings();
	NativeHookManager::Instance().LoadAMX(script.GetAMX());
	RegisterNativeHooks();

    pawn_natives::AmxLoad(script.GetAMX());

	core_->logLn(LogLevel::Message, "");
	core_->logLn(LogLevel::Message, " =======================================================================");
	core_->logLn(LogLevel::Message, "  CHandlingSvr %.*d.%.*d.%.*d%.*s by .silent (adapted by Zorono) loaded!", CHANDLING_VERSION_MAJOR, CHANDLING_VERSION_MINOR, CHANDLING_VERSION_PATCH, (CHANDLING_PHASE_DEV ? "-dev" : ""));
	core_->logLn(LogLevel::Message, " =======================================================================");
	core_->logLn(LogLevel::Message, "");
};

void CHandlingCompo::onAmxUnload(IPawnScript &script)
{
	NativeHookManager::Instance().UnloadAMX(script.GetAMX());

	core_->logLn(LogLevel::Message, "");
	core_->logLn(LogLevel::Message, " =========================================================================");
	core_->logLn(LogLevel::Message, "  CHandlingSvr %.*d.%.*d.%.*d%.*s by .silent (adapted by Zorono) unloaded!", CHANDLING_VERSION_MAJOR, CHANDLING_VERSION_MINOR, CHANDLING_VERSION_PATCH, (CHANDLING_PHASE_DEV ? "-dev" : ""));
	core_->logLn(LogLevel::Message, " =========================================================================");
	core_->logLn(LogLevel::Message, "");
};

void CHandlingCompo::onTick(Microseconds elapsed, TimePoint now)
{
	HandlingMgr::ProcessTick();
}

bool CHandlingCompo::onReceivePacket(IPlayer &peer, int id, NetworkBitStream &bs)
{
	if (id == (uint8_t)CHandlingPacketID::ID_CHANDLING)
	{
		core_->logLn(LogLevel::Debug, "[CHandling] Received custom packet ID %.*d from player %.*d (size=%.*d)\n", id, peer.getID(), bs.GetNumberOfBytesUsed());
		if (((bs.GetNumberOfUnreadBits() + 7) >> 3) > 1)
		{
			uint8_t action;
			bs.Read(action);

			Actions::Process((CHandlingAction)action, bs, peer);
		}
	}
	return true;
}

void CHandlingCompo::onFree(IComponent *component)
{
	if (component == pawn_component_)
		pawn_component_ = nullptr;
	else if (component == vehicles_)
		vehicles_ = nullptr;
	else if (component == this)
		core_->getEventDispatcher().removeEventHandler(this);
}

void CHandlingCompo::reset() {}

void CHandlingCompo::free()
{
	delete this;
}

ICore *&CHandlingCompo::getCore()
{
	static ICore *core{};

	return core;
}

CHandlingCompo *&CHandlingCompo::get()
{
	static CHandlingCompo *component{};

	return component;
}

COMPONENT_ENTRY_POINT()
{
	return (IComponent*)CHandlingCompo::get();
}

void CHandlingCompo::onIncomingConnection(IPlayer& player, StringView ipAddress, unsigned short port)
{
	int playerid = player.getID();
	gPlayers[playerid].Reset();
}

void CHandlingCompo::onPlayerConnect(IPlayer& player)
{
	core_->logLn(LogLevel::Debug, "[CHandling] OnPlayerConnect");
	HandlingMgr::OnPlayerConnect(player);
}

void CHandlingCompo::onPlayerDisconnect(IPlayer& player, PeerDisconnectReason reason)
{
	int playerid = player.getID();
	gPlayers[playerid].Reset();
	HandlingMgr::OnPlayerDisconnect(player, reason);
}

void CHandlingCompo::onVehicleStreamIn(IVehicle& vehicle, IPlayer& player)
{
	core_->logLn(LogLevel::Debug, "[CHandling] OnVehicleStreamIn(%d,%d)", vehicle.getID(), player.getID());

	// Send handling modifications for this vehicle
	HandlingMgr::OnVehicleStreamIn(vehicle, player);
}
