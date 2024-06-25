#include "lcppacket_drop.h"

PacketDB<LcpDropPacket> LcpDropPacket::_packetdb;
PacketDB<LcpDropAck> LcpDropAck::_packetdb;
PacketDB<LcpDropNack> LcpDropNack::_packetdb;
