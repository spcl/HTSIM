#include "lcppacket.h"

PacketDB<LcpPacket> LcpPacket::_packetdb;
PacketDB<LcpAck> LcpAck::_packetdb;
PacketDB<LcpNack> LcpNack::_packetdb;
