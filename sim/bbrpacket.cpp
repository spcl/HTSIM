#include "bbrpacket.h"

PacketDB<BBRPacket> BBRPacket::_packetdb;
PacketDB<BBRAck> BBRAck::_packetdb;
PacketDB<BBRNack> BBRNack::_packetdb;
