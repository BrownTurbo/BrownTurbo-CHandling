#pragma once

#include <sdk.hpp>
#include <RakNet/bitstream.hpp>
#include "PacketEnum.h"
#include <Server/Components/Pawn/pawn.hpp>
#include <cstdint>

// action identifier is sent as single byte
enum CHandlingAction : unsigned char {
	ACTION_INIT = 10,
	ACTION_INIT_RESPONSE = 11,
	ACTION_RESET_MODEL = 15,
	ACTION_RESET_VEHICLE = 16,
	ACTION_SET_VEHICLE_HANDLING = 17,
	ACTION_SET_MODEL_HANDLING = 18,
	ACTION_SET_PLAYER_HANDLING = 19,
	ACTION_RESET_PLAYER_HANDLING = 20,
	ACTION_GET_VEHICLE_HANDLING = 21,
	ACTION_GET_MODEL_HANDLING = 22,
	ACTION_GET_PLAYER_HANDLING = 23,
	ACTION_RESET_ALL = 24,
	ACTION_REQUEST_FILE_TRANSFER = 30,
	ACTION_FILE_TRANSFER_BEGIN = 31,
	ACTION_FILE_TRANSFER_CHUNK = 32,
	ACTION_FILE_TRANSFER_END = 33,
	ACTION_FILE_TRANSFER_CANCEL = 34
};

struct CHandlingActionPacket {
	NetworkBitStream data;

	CHandlingActionPacket(CHandlingAction actionID)
	{
		data.Write((uint8_t)CHandlingPacketID::PKT_CHANDLING);
		data.Write((uint8_t)actionID);
	}
};

namespace Actions {
bool Process(CHandlingAction id, NetworkBitStream& bs, IPlayer& player);
}

enum class ModelFileKind : uint8_t { Dff = 0,
	Txd = 1,
	Col = 2 };

inline constexpr int kFileTransferChannel = 1;
inline constexpr uint32_t kFileChunkSize = 4096;
inline constexpr uint32_t kChunksPerPlayerPerTick = 4;
