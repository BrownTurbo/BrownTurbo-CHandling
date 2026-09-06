#pragma once
#include <cstdint>
#include <span>
#include <sdk.hpp>
#include "../../Shared/CustomVehicleProtocol.hpp"

namespace CustomVehicleTransport
{
void SendVehicleBind(IPlayer& player, uint16_t sampVehicleId, uint32_t customModelId);
void SendVehicleUnbind(IPlayer& player, uint16_t sampVehicleId);
}
