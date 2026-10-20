#include "stdafx.h"

#include <gtest/gtest.h>

#include <filesystem>

#include "Core/bugreport.h"
#include "Core/logring.h"

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
    }
}
