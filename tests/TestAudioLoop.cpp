#include "AudioLoop.hpp"

#include "globals.hpp"
#include "ut.hpp"
#include "tMus.hpp"

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
}
