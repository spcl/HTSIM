#include "uecpacket_drop.h"

PacketDB<UecDropPacket> UecDropPacket::_packetdb;
PacketDB<UecDropAck> UecDropAck::_packetdb;
PacketDB<UecDropNack> UecDropNack::_packetdb;
