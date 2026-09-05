#pragma once

#include <array>
#include <cstdint>

namespace CustomVeh::Protocol
{
constexpr uint16_t VERSION = 2;

constexpr uint8_t PACKET_ID = 251;

enum class Action : uint8_t
{
	Init = 10,
	InitResponse = 11,

	ResetModel = 15,
	ResetVehicle = 16,
	SetVehicleHandling = 17,
	SetModelHandling = 18,

	SetPlayerHandling = 19,
	ResetPlayerHandling = 20,
	GetVehicleHandling = 21,
	GetModelHandling = 22,
	GetPlayerHandling = 23,
	ResetAll = 24,

	CustomVehicleDefine = 40,
	CustomVehicleBind = 41,
	CustomVehicleUnbind = 42,
	CustomVehicleDestroy = 43,

	AssetManifest = 50,
	AssetRequest = 51,
	AssetResume = 52,
	AssetBegin = 53,
	AssetChunk = 54,
	AssetEnd = 55,
	AssetVerified = 56,
	AssetCancel = 57,
	AssetRejected = 58,
	AssetReady = 59
};

enum class AssetType : uint8_t
{
	Dff = 0,
	Txd = 1,
	Col = 2
};

enum AssetFlags : uint32_t
{
	None = 0,
	HasDff = 1u << 0,
	HasTxd = 1u << 1,
	HasCol = 1u << 2
};

#pragma pack(push, 1)

struct AssetDescriptor
{
	AssetType type;
	uint64_t size;
	uint32_t compressedSize;
	uint32_t chunkSize;
	uint32_t chunkCount;
	std::string sha256;
	std::string filename;
};

struct EngineSound
{
	int16_t OnSound;
	int16_t OffSound;
};

struct CelerateSound
{
	int16_t accelerateSound;
	int16_t decelerateSound;
};

struct VehicleDefinition
{
	uint32_t protocol;
	uint32_t customModelId;

	uint16_t visualBaseModel;
	uint16_t handlingBaseModel;
	uint16_t audioBaseModel;

	EngineSound engineSoundId;
	CelerateSound celerateSoundId;

	uint32_t flags;

	AssetDescriptor dff;
	AssetDescriptor txd;
	AssetDescriptor col;
};

struct VehicleBinding
{
	uint32_t protocol;

	uint16_t sampVehicleId;
	uint32_t customModelId;
};

struct VehicleUnbinding
{
	uint32_t protocol;
	uint16_t sampVehicleId;
};

struct AssetRequest
{
	uint32_t protocol;

	uint32_t transferId;
	uint32_t customModelId;

	AssetType type;

	std::string sha256;
};

struct AssetResume
{
	uint32_t protocol;

	uint32_t transferId;
	uint32_t customModelId;

	AssetType type;

	std::string sha256;

	uint32_t nextChunk;
};

struct AssetBegin
{
	uint32_t protocol;

	uint32_t transferId;
	uint32_t customModelId;

	AssetType type;

	uint64_t uncompressedSize;
	uint64_t compressedSize;

	uint32_t chunkSize;
	uint32_t chunkCount;

	std::string sha256;
};

struct AssetChunkHeader
{
	uint32_t transferId;
	uint32_t chunkIndex;
	uint16_t payloadSize;
};

struct AssetEnd
{
	uint32_t transferId;

	uint32_t chunkCount;

	std::string sha256;
};

struct AssetVerified
{
	uint32_t protocol;

	uint32_t transferId;
	uint32_t customModelId;

	AssetType type;

	std::string sha256;
};

#pragma pack(pop)
}
