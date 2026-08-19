#pragma once
// CHandling packets

enum CHandlingPacketID : unsigned char
{
	ID_CHANDLING = 251 // one packet to rule them all, we don't want to use any ID occupied by samp so let's keep it minimal
};

enum CHandlingRPCID : unsigned char
{
  CUSTOM_VEHICLE_DEF = 250,
  DESTROY_CUSTOM_VEHICLE_MODEL = 251
};