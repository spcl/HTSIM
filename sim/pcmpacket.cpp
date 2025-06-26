#include "pcmpacket.h"

PacketDB<PcmPacket> PcmPacket::_packetdb;
PacketDB<PcmAck> PcmAck::_packetdb;
PacketDB<PcmNack> PcmNack::_packetdb;
