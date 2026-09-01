#pragma once

#pragma pack(push, 1)
struct RPC_WorldVehicleAdd_Data {
	uint16_t vehicleId;
	uint32_t modelId;
	float pos[3];
	float angle;
	uint8_t color1;
	uint8_t color2;
	float health;
	uint8_t interior;
	uint32_t doorDamage;
	uint32_t panelDamage;
	uint8_t lightDamage;
	uint8_t tireDamage;
	uint8_t addSiren;
	uint8_t modSlots[14];
	uint8_t paintjob;
	uint32_t bodyColor1;
	uint32_t bodyColor2;
	uint8_t zAngle;
};

struct CustomVehicleDef {
	uint32_t customModelId;
	uint32_t visualBaseModelId; // Dictates dummy nodes and collision
	uint32_t audioBaseModelId; // Dictates base audio properties
	uint32_t handlingBaseModelId; // Dictates physics (acceleration, suspension)
	int16_t engineSoundId; // Dictates custom engine sound (-1 for audioBase default)
	char dffUrl[128];
	char dffHash[65]; // SHA-256 Hex String + Null Terminator
	char txdUrl[128];
	char txdHash[65]; // SHA-256 Hex String + Null Terminator
	char colUrl[128];
	char colHash[65];
};

struct VehicleCacheEntry {
	uint16_t sampVehicleId;
	CVehicle* gameVehicle;

	uint32_t logicalModelId;
	uint32_t gtaModelId;
};
#pragma pack(pop)

//
constexpr uint16_t PKT_CHANDLING = 251;
constexpr uint32_t EXTENDEDVEH_COMPAT_VERSION = 0x1001D;
//

constexpr uint16_t RPC_WORLD_VEHICLE_ADD = 164;
constexpr uint16_t RPC_CUSTOM_VEHICLE_DEF = 250;
constexpr uint16_t RPC_DESTROY_CUSTOM_VEHICLE_MODEL = 251;

constexpr uint32_t BASE_MODEL_START = 400;
constexpr uint32_t BASE_MODEL_END = 611;
constexpr uint32_t BASE_VEHICLE_MODELS = 212; // (611 - 400 + 1)
constexpr uint32_t CUSTOM_MODEL_BASE_ID = 20000;

constexpr uint32_t MAX_SAMP_VEHICLES = 65535;
constexpr uint16_t DEFAULT_MAX_VEHICLES = 2000;
constexpr uint16_t INVALID_VEHICLE_ID = 0xFFFF;
constexpr uint16_t INVALID_PLAYER_ID = 0xFFFF;

inline constexpr bool IsBaseVehicleModel(uint32_t modelId)
{
	return modelId >= BASE_MODEL_START && modelId <= BASE_MODEL_END;
}

inline constexpr bool IsCustomVehicleModel(uint32_t modelId)
{
	return modelId >= CUSTOM_MODEL_BASE_ID;
}

inline constexpr uint32_t GetBaseModelIndex(uint32_t modelId)
{
	return modelId - BASE_MODEL_START;
}
