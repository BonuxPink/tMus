/*
 * Copyright (C) 2023-2024 Dāniels Ponamarjovs <bonux@duck.com>
 *
 * This file is part of tMus.
 *
 * tMus is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * tMus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tMus. If not, see <https://www.gnu.org/licenses/>.
 */

#include "AudioLoop.hpp"

#include "globals.hpp"
#include "ut.hpp"
#include "tMus.hpp"
#include "StatusView.hpp"

using namespace boost::ut;

int main()
{
    auto correct = std::filesystem::path("tests/misc/output.wav");
    auto incorrect = std::filesystem::path("meow");

    "AudioFileManager"_test = [&]
    {
        notcurses_options opts{ .termtype = nullptr,
                                .loglevel = NCLOGLEVEL_FATAL,
                                .margin_t = 0, .margin_r = 0,
                                .margin_b = 0, .margin_l = 0,
                                .flags = NCOPTION_SUPPRESS_BANNERS,
        };

        ncpp::NotCurses nc{ opts };

        tMus::Init();
        tMus::InitLog();

        ContextData data1
        {
            .format_ctx = std::shared_ptr<AVFormatContext>(nullptr, [](AVFormatContext* ptr)
            {
                avformat_free_context(ptr);
            }),
            .codec_ctx = nullptr
        };

        ContextData data2
        {
            .format_ctx = std::shared_ptr<AVFormatContext>(nullptr, [](AVFormatContext* ptr)
            {
                avformat_free_context(ptr);
            }),
            .codec_ctx = std::make_shared<AVCodecContext>()
        };

        auto should_not_throw = [&] { AudioFileManager p { correct,   data1 }; };
        auto will_throw       = [&] { AudioFileManager p { incorrect, data2 }; };

        expect (nothrow (should_not_throw));
        expect (throws<std::runtime_error>(will_throw));
    };

    "AudioLoop"_test = [&]
    {
#if 0
        notcurses_options opts{ .termtype = nullptr,
                                .loglevel = NCLOGLEVEL_FATAL,
                                .margin_t = 0, .margin_r = 0,
                                .margin_b = 0, .margin_l = 0,
                                .flags = NCOPTION_SUPPRESS_BANNERS,
        };

        ncpp::NotCurses nc{ opts };

        tMus::Init();
        tMus::InitLog();
        auto should_not_fail = [&] { AudioLoop p{ correct }; };

        expect(nothrow(should_not_fail));

        auto can_be_stopped = [&](std::stop_token st)
        {
            AudioLoop loop{ correct };
            StatusView::Create(loop.getContextData());
            loop.consumer_loop(st);
        };

        std::jthread th{ can_be_stopped };
        expect (th.request_stop());

        th.join();

        expect (th.joinable() == false);

        Globals::stop_request = true;
        th = std::jthread { can_be_stopped };

        expect (th.joinable() == false);
#endif
    };
}
