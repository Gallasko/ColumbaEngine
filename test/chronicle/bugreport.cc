#include "stdafx.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "Core/bugreport.h"
#include "Core/bugsend.h"
#include "Core/logring.h"
#include "Core/screenshot.h"

#include "ECS/entitysystem.h"
#include "Renderer/renderer.h"

#include <thread>

#include "mocklogger.h"

using namespace chronicle;

namespace pg
{
    namespace test
    {
        static bool validUtf8(const std::string& s)
        {
            size_t i = 0;
            while (i < s.size())
            {
                unsigned char c = s[i];
                size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
                if (n == 0 || i + n > s.size())
                    return false;
                for (size_t k = 1; k < n; ++k)
                    if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80)
                        return false;
                i += n;
            }
            return true;
        }

        TEST(bugreport_test, the_body_carries_every_field_escaped)
        {
            BugReport r;
            r.playerId = "abc";
            r.sessionId = "sess";
            r.version = "0.8";
            r.message = "He said \"hi\"\nthen caf\xC3\xA9 \\ \t";
            r.screen = "data:image/jpeg;base64,AAAA";
            r.save = "line1\nline2";
            r.logs = "log";
            r.place = "life, age 7";
            r.client = "UA";

            auto json = bugReportJson(r);

            EXPECT_EQ(json.front(), '{');
            EXPECT_EQ(json.back(), '}');
            EXPECT_NE(json.find("\"player_id\":\"abc\""), std::string::npos);
            EXPECT_NE(json.find("\"session_id\":\"sess\""), std::string::npos);
            EXPECT_NE(json.find("\"version\":\"0.8\""), std::string::npos);
            EXPECT_NE(json.find("\"message\":\"He said \\\"hi\\\"\\nthen caf\xC3\xA9 \\\\ \\t\""), std::string::npos);
            EXPECT_NE(json.find("\"screen\":\"data:image/jpeg;base64,AAAA\""), std::string::npos);
            EXPECT_NE(json.find("\"save\":\"line1\\nline2\""), std::string::npos);
            EXPECT_NE(json.find("\"logs\":\"log\""), std::string::npos);
            EXPECT_NE(json.find("\"place\":\"life, age 7\""), std::string::npos);
            EXPECT_NE(json.find("\"client\":\"UA\""), std::string::npos);
            // No raw newline or tab survives
            EXPECT_EQ(json.find('\n'), std::string::npos);
            EXPECT_EQ(json.find('\t'), std::string::npos);
        }

        TEST(bugreport_test, a_long_message_is_clipped_at_a_character)
        {
            std::string message;
            for (int i = 0; i < 2005; ++i)
                message += "\xC3\xA9";

            auto clipped = clipMessage(message);

            EXPECT_LE(clipped.size(), MaxMessage);
            EXPECT_TRUE(validUtf8(clipped));
            EXPECT_EQ(clipped.size(), 2000u);

            EXPECT_EQ(clipMessage("short"), "short");
        }

        TEST(bugreport_test, a_body_over_two_megabytes_is_refused)
        {
            EXPECT_TRUE(fits(std::string(MaxBody, 'a')));
            EXPECT_FALSE(fits(std::string(MaxBody + 1, 'a')));

            BugReport r;
            r.save = std::string(MaxBody, 'x');
            EXPECT_FALSE(fits(bugReportJson(r)));
        }

        TEST(bugreport_test, three_reports_then_a_wait)
        {
            ReportLimit limit;
            EXPECT_TRUE(limit.allowed(0));

            limit.sent(0);
            limit.sent(10);
            EXPECT_TRUE(limit.allowed(15));
            limit.sent(20);

            EXPECT_FALSE(limit.allowed(30));
            EXPECT_EQ(limit.waitSeconds(30), 870);
            EXPECT_TRUE(limit.allowed(901));
            EXPECT_EQ(limit.waitSeconds(901), 0);
        }

        TEST(bugreport_test, the_limit_survives_its_text)
        {
            ReportLimit limit;
            limit.sent(100);
            limit.sent(200);
            limit.sent(300);

            auto again = ReportLimit::fromText(limit.text());

            EXPECT_EQ(again.text(), limit.text());
            EXPECT_FALSE(again.allowed(400));
            EXPECT_EQ(again.waitSeconds(400), 600);

            EXPECT_TRUE(ReportLimit::fromText("").allowed(0));
            EXPECT_TRUE(ReportLimit::fromText("garbage\n\n12x\n").allowed(0));
        }

        TEST(bugreport_test, the_player_id_is_made_once)
        {
            auto dir = std::filesystem::temp_directory_path() / "chronicle_bugreport_test_dir";
            std::filesystem::remove_all(dir);

            auto first = playerIdAt(dir.string());
            auto second = playerIdAt(dir.string());

            EXPECT_EQ(first, second);
            ASSERT_EQ(first.size(), 32u);
            for (char c : first)
                EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));

            std::filesystem::remove_all(dir);
        }

        TEST(bugreport_test, the_wait_is_said_in_minutes)
        {
            EXPECT_EQ(waitLine(870), "You may send another in 15 minutes");
            EXPECT_EQ(waitLine(30), "You may send another in 1 minute");
            EXPECT_EQ(waitLine(60), "You may send another in 1 minute");
            EXPECT_EQ(waitLine(61), "You may send another in 2 minutes");
        }

        TEST(bugreport_test, the_ring_keeps_the_last_three_hundred)
        {
            auto sink = Logger::registerSink<LogRing>();

            for (int i = 1; i <= 310; ++i)
                LOG_ERROR("Ring", "line " << i);

            const auto text = LogRing::instance()->text();
            EXPECT_EQ(text, recentLogs());

            std::vector<std::string> lines;
            size_t at = 0;
            while (at < text.size())
            {
                auto e = text.find('\n', at);
                lines.push_back(text.substr(at, e - at));
                at = e + 1;
            }

            ASSERT_EQ(lines.size(), 300u);
            EXPECT_EQ(lines.front(), "[ERROR] Ring: line 11");
            EXPECT_EQ(lines.back(), "[ERROR] Ring: line 310");

            Logger::removeSink(sink);
        }

        TEST(bugreport_test, the_ring_is_safe_across_threads)
        {
            auto sink = Logger::registerSink<LogRing>();

            auto work = [](int who) { for (int i = 0; i < 200; ++i) LOG_ERROR("Ring", "t" << who << " " << i); };
            std::thread a(work, 1), b(work, 2);
            a.join();
            b.join();

            const auto text = LogRing::instance()->text();
            EXPECT_EQ(std::count(text.begin(), text.end(), '\n'), 300);

            Logger::removeSink(sink);
        }

        TEST(bugreport_test, the_ring_coexists_with_the_mock_logger)
        {
            auto sink = Logger::registerSink<LogRing>();
            MockLogger<> mock;

            LOG_ERROR("Ring", "one");
            LOG_ERROR("Ring", "two");

            EXPECT_EQ(mock.getNbError(), 2u);
            EXPECT_NE(LogRing::instance()->text().find("[ERROR] Ring: two"), std::string::npos);

            Logger::removeSink(sink);
        }

        // A base64 text back to its bytes (the test's own, to read what the encoder wrote)
        static std::vector<unsigned char> unbase64(const std::string& text)
        {
            static const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::vector<unsigned char> out;
            int value = 0, bits = -8;
            for (char c : text)
            {
                if (c == '=')
                    break;
                const auto at = alphabet.find(c);
                if (at == std::string::npos)
                    return {};
                value = (value << 6) + static_cast<int>(at);
                bits += 6;
                if (bits >= 0)
                {
                    out.push_back(static_cast<unsigned char>((value >> bits) & 0xFF));
                    bits -= 8;
                }
            }
            return out;
        }

        TEST(bugreport_test, rows_come_out_top_down)
        {
            // OpenGL gives the bottom row first: A B is the bottom, C D the top
            const std::vector<unsigned char> read = { 1, 1, 1, 1,  2, 2, 2, 2,
                                                      3, 3, 3, 3,  4, 4, 4, 4 };

            const auto flipped = flipRows(read, 2, 2);

            const std::vector<unsigned char> expected = { 3, 3, 3, 3,  4, 4, 4, 4,
                                                          1, 1, 1, 1,  2, 2, 2, 2 };
            EXPECT_EQ(flipped, expected);
        }

        TEST(bugreport_test, a_wide_frame_fits_within_1280)
        {
            int w = 2560, h = 1440;
            std::vector<unsigned char> rgba(static_cast<size_t>(w) * h * 4, 200);

            fitWithin(rgba, w, h);

            EXPECT_EQ(w, 1280);
            EXPECT_EQ(h, 720);
            EXPECT_EQ(rgba.size(), static_cast<size_t>(1280) * 720 * 4);
            EXPECT_EQ(rgba[0], 200);
            EXPECT_EQ(rgba.back(), 200);

            // A tall one fits by its height, and keeps its ratio
            int tw = 1000, th = 2000;
            std::vector<unsigned char> tall(static_cast<size_t>(tw) * th * 4, 10);
            fitWithin(tall, tw, th);
            EXPECT_EQ(th, 1280);
            EXPECT_EQ(tw, 640);
            EXPECT_EQ(tall.size(), static_cast<size_t>(640) * 1280 * 4);

            // A frame within the limit is left as it is
            int sw = 800, sh = 600;
            std::vector<unsigned char> small(static_cast<size_t>(sw) * sh * 4, 7);
            const auto before = small;
            fitWithin(small, sw, sh);
            EXPECT_EQ(sw, 800);
            EXPECT_EQ(sh, 600);
            EXPECT_EQ(small, before);

            // The pixels that fall in one are averaged: a 4x2 frame into 2x1
            int bw = 4, bh = 2;
            std::vector<unsigned char> box = { 0, 0, 0, 255,   100, 100, 100, 255,   40, 40, 40, 255,   40, 40, 40, 255,
                                               100, 100, 100, 255,   0, 0, 0, 255,   40, 40, 40, 255,   40, 40, 40, 255 };
            fitWithin(box, bw, bh, 2);
            EXPECT_EQ(bw, 2);
            EXPECT_EQ(bh, 1);
            const std::vector<unsigned char> averaged = { 50, 50, 50, 255,   40, 40, 40, 255 };
            EXPECT_EQ(box, averaged);
        }

        TEST(bugreport_test, a_frame_is_a_jpeg_data_url)
        {
            const int w = 16, h = 8;
            std::vector<unsigned char> rgba(static_cast<size_t>(w) * h * 4);
            for (size_t i = 0; i < rgba.size(); ++i)
                rgba[i] = static_cast<unsigned char>(i * 7);

            const std::string url = jpegBase64(rgba, w, h);

            const std::string prefix = "data:image/jpeg;base64,";
            ASSERT_EQ(url.rfind(prefix, 0), 0u);

            const auto bytes = unbase64(url.substr(prefix.size()));
            ASSERT_GE(bytes.size(), 4u);
            EXPECT_EQ(bytes[0], 0xFF);
            EXPECT_EQ(bytes[1], 0xD8);
            EXPECT_EQ(bytes[bytes.size() - 2], 0xFF);   // and it ends as a JPEG ends
            EXPECT_EQ(bytes[bytes.size() - 1], 0xD9);
        }

        TEST(bugreport_test, a_capture_reads_the_drawable_size)
        {
            // A dense screen draws the window on twice its pixels: the capture reads them all
            const auto dense = MasterRenderer::captureSize(800, 600, 1600, 1200);
            EXPECT_EQ(dense.width, 1600);
            EXPECT_EQ(dense.height, 1200);

            // Where the two are the same, the capture is the window's, as it always was
            const auto plain = MasterRenderer::captureSize(800, 600, 800, 600);
            EXPECT_EQ(plain.width, 800);
            EXPECT_EQ(plain.height, 600);

            // With no drawable to ask (no window), the window's size
            const auto unknown = MasterRenderer::captureSize(800, 600, 0, 0);
            EXPECT_EQ(unknown.width, 800);
            EXPECT_EQ(unknown.height, 600);

            // The renderer itself, with no window: its own size
            MasterRenderer renderer;
            renderer.setWindowSize(640, 480);
            const auto own = renderer.captureSize();
            EXPECT_EQ(own.width, 640);
            EXPECT_EQ(own.height, 480);
        }

        namespace
        {
            struct FrameAsks : public System<Listener<SaveCurrentFrameEvent>, Listener<ReRendererAll>, StoragePolicy>
            {
                void onEvent(const SaveCurrentFrameEvent&) override { ++saves; }
                void onEvent(const ReRendererAll&) override { ++redraws; }

                int saves = 0, redraws = 0;
            };
        }

        TEST(bugreport_test, a_grab_answers_with_the_next_frame)
        {
            EntitySystem ecs;
            auto asks = ecs.createSystem<FrameAsks>();
            auto grab = ecs.createSystem<FrameGrab>();
            ecs.executeOnce();

            int answers = 0;
            std::string got;
            grab->request([&](std::string dataUrl) { ++answers; got = dataUrl; });
            ecs.executeOnce();

            EXPECT_EQ(asks->saves, 1);     // the frame is asked for, and drawn again so that it comes
            EXPECT_EQ(asks->redraws, 1);
            EXPECT_EQ(answers, 0);
            EXPECT_TRUE(grab->waiting());

            std::vector<unsigned char> pixels(4 * 2 * 4, 128);
            ecs.sendEvent(SavedFrameData{ pixels, 4, 2 });
            ecs.executeOnce();

            EXPECT_EQ(answers, 1);
            EXPECT_EQ(got.rfind("data:image/jpeg;base64,", 0), 0u);
            EXPECT_FALSE(grab->waiting());

            // A frame nobody asked for is not answered
            ecs.sendEvent(SavedFrameData{ pixels, 4, 2 });
            ecs.executeOnce();
            EXPECT_EQ(answers, 1);
        }

        TEST(bugreport_test, a_native_report_is_written_to_a_folder)
        {
            const auto root = std::filesystem::temp_directory_path() / "chronicle_bugsend_test";
            std::filesystem::remove_all(root);

            BugReport report;
            report.message = "it broke";
            report.save = "SAVE";
            report.logs = "LOGS";
            report.place = "PLACE";
            report.screen = "data:image/jpeg;base64,/9j/4AAQ";  // FF D8 FF E0 00 10

            SendResult got = SendResult::Failed;
            std::string line;
            sendReport(report, [&](SendResult r, int, std::string l) { got = r; line = l; }, root.string());

            EXPECT_EQ(got, SendResult::Written);

            const auto reports = root / "reports";
            ASSERT_TRUE(std::filesystem::exists(reports));
            const auto folder = std::filesystem::directory_iterator(reports)->path();
            EXPECT_EQ(line, "Written to " + folder.string());

            for (const char* name : { "message.txt", "screen.jpg", "life.sz", "log.txt", "place.txt" })
                EXPECT_TRUE(std::filesystem::exists(folder / name)) << name;

            std::ifstream jpg(folder / "screen.jpg", std::ios::binary);
            unsigned char head[2] = { 0, 0 };
            jpg.read(reinterpret_cast<char*>(head), 2);
            EXPECT_EQ(head[0], 0xFF);
            EXPECT_EQ(head[1], 0xD8);

            std::ifstream life(folder / "life.sz");
            std::string text((std::istreambuf_iterator<char>(life)), std::istreambuf_iterator<char>());
            EXPECT_EQ(text, "SAVE");

            std::filesystem::remove_all(root);
        }

        TEST(bugreport_test, a_failed_send_keeps_the_words)
        {
            ReportFlow flow;
            flow.message = "my words";

            flow.result(SendResult::Failed);
            EXPECT_EQ(flow.message, "my words");
            flow.result(SendResult::Limited);
            EXPECT_EQ(flow.message, "my words");
            flow.result(SendResult::TooLarge);
            EXPECT_EQ(flow.message, "my words");

            flow.result(SendResult::Sent);
            EXPECT_TRUE(flow.message.empty());

            flow.message = "again";
            flow.result(SendResult::Written);
            EXPECT_TRUE(flow.message.empty());
        }

        TEST(bugreport_test, the_bug_url_is_the_flag_or_the_worker)
        {
            setBugUrl("");
            EXPECT_EQ(bugUrl().substr(bugUrl().size() - 4), "/bug");
            setBugUrl("http://localhost:9/x");
            EXPECT_EQ(bugUrl(), "http://localhost:9/x");
            setBugUrl("");
        }
    }
}
