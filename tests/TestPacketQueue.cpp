#include <catch2/catch_test_macros.hpp>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <cstdio>
#include <array>
#include "../src/Frame.hpp"

extern "C"
{
    #include <libavutil/rational.h>
    #include <libavcodec/avcodec.h>
    #include <libavcodec/codec.h>
    #include <libavcodec/defs.h>
    #include <libavcodec/packet.h>
    #include <libavutil/avutil.h>
    #include <libavutil/error.h>
    #include <libavformat/avformat.h>
}

using namespace std::chrono_literals;

TEST_CASE ( "PacketQueue", "[PacketQueue]" )
{
    PacketQueue pq;
    std::condition_variable cv;
    pq.cv = &cv;

    auto cvTest = [&]() { std::mutex mtx; std::unique_lock lk{ mtx }; cv.wait(lk); };
    std::jthread th{ cvTest };

    std::this_thread::sleep_for(10ms);

    AVPacket pk
    {
        .buf = nullptr,
        .pts = 42,
        .dts = 7,
        .data = nullptr,
        .size = 11,
        .stream_index = 99,
        .flags = 44,
        .side_data = nullptr,
        .side_data_elems = 457,
        .duration = 1000,
        .pos = 100,
        .opaque = nullptr,
        .opaque_ref = nullptr,
        .time_base = AVRational{ .num = 0, .den = 0 },
    };

    REQUIRE ( pk.buf             == nullptr );
    REQUIRE ( pk.pts             == 42 );
    REQUIRE ( pk.dts             == 7 );
    REQUIRE ( pk.data            == nullptr );
    REQUIRE ( pk.size            == 11 );
    REQUIRE ( pk.stream_index    == 99 );
    REQUIRE ( pk.flags           == 44 );
    REQUIRE ( pk.side_data       == nullptr );
    REQUIRE ( pk.side_data_elems == 457 );
    REQUIRE ( pk.duration        == 1000 );
    REQUIRE ( pk.pos             == 100 );
    REQUIRE ( pk.opaque          == nullptr );
    REQUIRE ( pk.opaque_ref      == nullptr );

    pq.put(&pk);

    AVPacket outPkt{};

    REQUIRE ( pq.nb_packets == 1 );

    pq.get(outPkt);

    REQUIRE ( pq.nb_packets == 0 );

    REQUIRE ( outPkt.buf             == nullptr );
    REQUIRE ( outPkt.pts             == 42 );
    REQUIRE ( outPkt.dts             == 7 );
    REQUIRE ( outPkt.data            == nullptr );
    REQUIRE ( outPkt.size            == 11 );
    REQUIRE ( outPkt.stream_index    == 99 );
    REQUIRE ( outPkt.flags           == 44 );
    REQUIRE ( outPkt.side_data       == nullptr );
    REQUIRE ( outPkt.side_data_elems == 457 );
    REQUIRE ( outPkt.duration        == 1000 );
    REQUIRE ( outPkt.pos             == 100 );
    REQUIRE ( outPkt.opaque          == nullptr );
    REQUIRE ( outPkt.opaque_ref      == nullptr );

    REQUIRE ( outPkt.time_base.den   == 0 );
    REQUIRE ( outPkt.time_base.num   == 0 );

    REQUIRE ( pq.m_duration          != pk.duration );

    pk.data = nullptr;
    pk.size = 0;
    pq.put(&pk);

    REQUIRE ( pq.nb_packets == 1 );

    pq.get(outPkt);

    REQUIRE ( pq.nb_packets == 0 );
    REQUIRE ( outPkt.stream_index == 0 );

    pq.abort();

    REQUIRE ( pq.abort_request == true );

    th.join();

    // pq.flush();

    REQUIRE ( pq.nb_packets == 0 );
    REQUIRE ( pq.m_size     == -11 );
    REQUIRE ( pq.m_duration == -1000 );

    SUCCEED ( "OK" );
}

TEST_CASE ( "FrameQueue", "[FrameQueue]" )
{
    PacketQueue m{};
    FrameQueue fq{ &m.abort_request };

    Frame* f = fq.peek_writable();
    f->frame = nullptr;
    f->duration = 10.5;
    f->pts = 11.0;
    f->pos = 7777;

    fq.push();

    auto* p = fq.peek();

    REQUIRE ( p->duration == 10.5 );
    REQUIRE ( p->frame == nullptr );
    REQUIRE ( p->pts == 11.0 );
    REQUIRE ( p->pos == 7777 );

    auto* otherFrame = fq.peek_writable();

    otherFrame->duration = 1.5;
    otherFrame->frame = nullptr;
    otherFrame->pts = 2.0;
    otherFrame->pos = 11111;

    fq.push();
    fq.next();

    auto* otherFarmeGot = fq.peek();

    REQUIRE ( otherFarmeGot->duration == 1.5 );
    REQUIRE ( otherFarmeGot->frame == nullptr );
    REQUIRE ( otherFarmeGot->pts == 2.0 );
    REQUIRE ( otherFarmeGot->pos == 11111 );

    // th.join();
    SUCCEED ( "OK" );
}
