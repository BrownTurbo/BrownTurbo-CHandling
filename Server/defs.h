#pragma once
#include <cstdint>

// Model limits
constexpr uint32_t BASE_MODEL_START       = 400;
constexpr uint32_t BASE_MODEL_END         = 611;
constexpr uint32_t BASE_VEHICLE_MODELS    = 212;   // (611 - 400 + 1)
constexpr uint32_t CUSTOM_MODEL_BASE_ID   = 20000;

// Vehicle limits
constexpr uint16_t DEFAULT_MAX_VEHICLES   = 2000;  // Standard SA-MP limit
constexpr uint16_t MAX_NETWORK_VEHICLES   = 65535; // Maximum 16-bit network ID
constexpr uint16_t INVALID_VEHICLE_ID     = 0xFFFF;
constexpr uint16_t INVALID_PLAYER_ID      = 0xFFFF;

// Helper functions
inline constexpr bool IsBaseVehicleModel(uint32_t modelId) {
    return modelId >= BASE_MODEL_START && modelId <= BASE_MODEL_END;
}

inline constexpr bool IsCustomVehicleModel(uint32_t modelId) {
    return modelId >= CUSTOM_MODEL_BASE_ID;
}

inline constexpr uint32_t GetBaseModelIndex(uint32_t modelId) {
    return modelId - BASE_MODEL_START; // 0 .. 211
}