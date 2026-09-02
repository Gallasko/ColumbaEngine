#include "gtest/gtest.h"

#include "Interpreter/lexer.h"
#include "Interpreter/parser.h"
#include "Interpreter/resolver.h"

#include "../mocklogger.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace pg {
namespace test {

/**
 * AST parser parity gate
 *
 * The AST front-end (Interpreter/parser.h) must accept every script the
 * Pratt front-end (Compiler/cparser.h) accepts, so the whole script corpus
 * is used as a grammar-acceptance oracle: every .pg file under
 * test/pgcompiler/scripts must parse without error.
 *
 * This is a parse-only check: semantic validation (break outside a loop,
 * unknown modules, ...) belongs to later compilation stages and is NOT
 * checked here.
 */
class AstParserParity : public ::testing::Test
{
protected:
    static constexpr const char* scriptsDir = "test/pgcompiler/scripts";

    // Scripts the AST parser is expected to reject, with the reason.
    // Anything here must actually fail (tracked by the
    // ExpectedFailListIsMinimal test below).
    const std::set<std::string>& expectedFailures() const
    {
        static const std::set<std::string> failures = {
            // Intentional-error script ('1 + + 2'); CParser rejects it too
            // (its ScriptTestBench error test is currently disabled)
            "syntax_error.pg",
        };

        return failures;
    }

    bool parseFile(const std::string& path, std::string& error)
    {
        try
        {
            Lexer lexer;
            lexer.readFromFile(path);

            Parser parser(lexer.getTokens());
            parser.parse();

            if (parser.hasError())
            {
                error = "parser reported an error";
                return false;
            }
        }
        catch (const std::exception& e)
        {
            error = e.what();
            return false;
        }

        return true;
    }

    std::vector<std::string> collectScripts() const
    {
        std::vector<std::string> scripts;

        for (const auto& entry : std::filesystem::directory_iterator(scriptsDir))
        {
            if (entry.path().extension() == ".pg")
                scripts.push_back(entry.path().string());
        }

        std::sort(scripts.begin(), scripts.end());

        return scripts;
    }
};

TEST_F(AstParserParity, AllCorpusScriptsParse)
{
    MockLogger<TerminalSink> logger;

    auto scripts = collectScripts();

    ASSERT_FALSE(scripts.empty()) << "No scripts found in " << scriptsDir
        << " - check the test working directory";

    size_t parsed = 0, skipped = 0;
    std::vector<std::string> failures;

    for (const auto& script : scripts)
    {
        auto name = std::filesystem::path(script).filename().string();

        if (expectedFailures().count(name) > 0)
        {
            skipped++;
            continue;
        }

        std::string error;
        if (parseFile(script, error))
        {
            parsed++;
        }
        else
        {
            failures.push_back(name + ": " + error);
        }
    }

    std::string failureReport;
    for (const auto& failure : failures)
        failureReport += "  " + failure + "\n";

    EXPECT_TRUE(failures.empty())
        << failures.size() << " corpus script(s) rejected by the AST parser:\n"
        << failureReport;

    std::cout << "[AstParserParity] parsed " << parsed << " scripts, "
              << skipped << " on the expected-fail list, "
              << failures.size() << " unexpected failures" << std::endl;
}

// Scripts on the expected-fail list must actually fail; anything that starts
// parsing successfully must be removed from the list so coverage only grows.
TEST_F(AstParserParity, ExpectedFailListIsMinimal)
{
    MockLogger<TerminalSink> logger;

    std::vector<std::string> stale;

    for (const auto& name : expectedFailures())
    {
        std::string error;
        if (parseFile(std::string(scriptsDir) + "/" + name, error))
            stale.push_back(name);
    }

    std::string report;
    for (const auto& name : stale)
        report += "  " + name + "\n";

    EXPECT_TRUE(stale.empty())
        << "These scripts now parse and must be removed from the expected-fail list:\n"
        << report;
}

} // namespace test
} // namespace pg
