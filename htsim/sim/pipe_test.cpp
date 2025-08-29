// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "pipe.h"

#include <cstdint>
#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "eventlist.h"
#include "network.h"

constexpr uint32_t PIPE_DELAY_PICOSECONDS = 10000000;  // 10us
constexpr uint32_t PACKET_SIZE            = 1000;

class MockPacket : public Packet {
   public:
    MockPacket(uint32_t size) {
        _size = size;
        _flow = new PacketFlow(nullptr);
    }
    ~MockPacket() { delete _flow; }
    PktPriority priority() const override { return Packet::PRIO_LO; }

    MOCK_METHOD(PacketSink*, sendOn, (), (override));
};

class PipeTest : public ::testing::Test {
   protected:
    std::unique_ptr<Pipe>      pipe_;
    std::unique_ptr<EventList> eventlist_;

    virtual void SetUp() {
        eventlist_ = std::make_unique<EventList>();
        // Make end time large enough to avoid early termination
        eventlist_->setEndtime(timeFromSec(100));

        // Create pipe with 10us delay
        pipe_ = std::make_unique<Pipe>(PIPE_DELAY_PICOSECONDS, *eventlist_);
    }

    virtual void TearDown() {}
};

TEST_F(PipeTest, PacketDeliveredAfterDelay) {
    std::unique_ptr<MockPacket> pkt = std::make_unique<MockPacket>(PACKET_SIZE);

    // Packet should be delivered after the pipe delay
    EXPECT_CALL(*pkt, sendOn()).Times(1);

    pipe_->receivePacket(*pkt);
    EXPECT_TRUE(eventlist_->doNextEvent());
    EXPECT_EQ(eventlist_->now(), PIPE_DELAY_PICOSECONDS);
}

TEST_F(PipeTest, MultiplePacketsDeliveredInOrder) {
    std::unique_ptr<MockPacket> pkt1 = std::make_unique<MockPacket>(PACKET_SIZE);
    std::unique_ptr<MockPacket> pkt2 = std::make_unique<MockPacket>(PACKET_SIZE);

    testing::Sequence seq;
    EXPECT_CALL(*pkt1, sendOn()).Times(1).InSequence(seq);
    EXPECT_CALL(*pkt2, sendOn()).Times(1).InSequence(seq);

    pipe_->receivePacket(*pkt1);
    pipe_->receivePacket(*pkt2);

    // First packet should be delivered after pipe delay
    EXPECT_TRUE(eventlist_->doNextEvent());
    EXPECT_EQ(eventlist_->now(), PIPE_DELAY_PICOSECONDS);

    // Second packet should be delivered immediately after first
    EXPECT_TRUE(eventlist_->doNextEvent());
    EXPECT_EQ(eventlist_->now(), PIPE_DELAY_PICOSECONDS);
}

TEST_F(PipeTest, DelayConfiguration) {
    EXPECT_EQ(pipe_->delay(), PIPE_DELAY_PICOSECONDS);
}

TEST_F(PipeTest, ConstructorSetsName) {
    EXPECT_EQ(pipe_->nodename(), "pipe(10us)");
}

TEST_F(PipeTest, CustomNameConfiguration) {
    pipe_->forceName("custom_pipe");
    EXPECT_EQ(pipe_->nodename(), "custom_pipe");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}