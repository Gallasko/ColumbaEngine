#include "stdafx.h"

#include "gtest/gtest.h"

#include "mockinterpreter.h"

#include "ECS/ecsmodule.h"

#include "mocklogger.h"

namespace pg
{
    namespace test
    {
        namespace
        {
            const char * testScript1 =  "var a = 1; var b = 2; var c = 3; \n"
                                        "a = b + c                        \n"
                                        "ExpectEq(a, 5);                  \n";

            const char * testScript2 =  "var a = 1; var b = 2; var c = 3; \n"
                                        "if (a == 1) ExpectEq(b, 2);      \n"
                                        "if (b < c)  ExpectEq(c, 3);      \n";
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(interpreter_test, simple_script_opening_test)
        {
            MockLogger logger;

            MockInterpreter interpreter;

            interpreter.interpretFromFile("TestScripts/BasicOperation/addition.pg");

            EXPECT_EQ(logger.getNbError(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(interpreter_test, simple_script_from_file_test)
        {
            MockLogger logger;

            MockInterpreter interpreter;

            interpreter.interpretFromText(testScript1);

            EXPECT_EQ(logger.getNbError(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(interpreter_test, second_simple_script_from_file_test)
        {
            MockLogger logger;

            MockInterpreter interpreter;

            interpreter.interpretFromText(testScript2);

            EXPECT_EQ(logger.getNbError(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(interpreter_test, open_multiple_script_from_file_test)
        {
            MockLogger logger;

            MockInterpreter interpreter;

            interpreter.interpretFromText(testScript1);

            interpreter.interpretFromText(testScript2);

            EXPECT_EQ(logger.getNbError(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(interpreter_test, assignment_test)
        {
            MockLogger logger;
            MockInterpreter interpreter;

            auto scripts = UniversalFileAccessor::openTextFolder("TestScripts/Assignment/");

            for (auto script : scripts)
            {
                interpreter.interpretFromFile(script);
            }

            EXPECT_EQ(logger.getNbError(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(interpreter_test, basic_operation_test)
        {
            MockLogger logger;
            MockInterpreter interpreter;

            auto scripts = UniversalFileAccessor::openTextFolder("TestScripts/BasicOperation/");

            for (auto script : scripts)
            {
                interpreter.interpretFromFile(script);
            }

            EXPECT_EQ(logger.getNbError(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(interpreter_test, conditionnal_test)
        {
            MockLogger logger;
            MockInterpreter interpreter;

            auto scripts = UniversalFileAccessor::openTextFolder("TestScripts/Conditionnal/");

            for (auto script : scripts)
            {
                interpreter.interpretFromFile(script);
            }

            EXPECT_EQ(logger.getNbError(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(interpreter_test, import_test)
        {
            // Todo make a test where you try to run two script with the same name but in differents folders and check if they are no conflicts !
            // MockLogger<TerminalSink> logger;
            MockLogger logger;
            MockInterpreter interpreter;

            auto scripts = UniversalFileAccessor::openTextFolder("TestScripts/Import/");

            for (auto script : scripts)
            {
                interpreter.interpretFromFile(script);
            }

            EXPECT_EQ(logger.getNbError(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(interpreter_test, functionnal_test)
        {
            MockLogger logger;
            // logger.addFilter("Log Level Filter", new Logger::LogSink::FilterLogLevel(Logger::InfoLevel::log));
            MockInterpreter interpreter;

            auto scripts = UniversalFileAccessor::openTextFolder("TestScripts/Functionnal/");

            for (auto script : scripts)
            {
                interpreter.interpretFromFile(script);
            }

            EXPECT_EQ(logger.getNbError(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(interpreter_test, table_test)
        {
            MockLogger logger;
            // logger.addFilter("Log Level Filter", new Logger::LogSink::FilterLogLevel(Logger::InfoLevel::log));

            MockInterpreter interpreter;

            auto scripts = UniversalFileAccessor::openTextFolder("TestScripts/Table/");

            for (auto script : scripts)
            {
                interpreter.interpretFromFile(script);
            }

            EXPECT_EQ(logger.getNbError(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(interpreter_test, ecs_interpreter_module_test)
        {
            MockLogger logger;
            // logger.addFilter("Log Level Filter", new Logger::LogSink::FilterLogLevel(Logger::InfoLevel::log));

            // MockInterpreter interpreter;

            auto scripts = UniversalFileAccessor::openTextFolder("TestScripts/EcsModule/");

            for (auto script : scripts)
            {
                EntitySystem ecs;

                auto interpreter = ecs.createSystem<PgInterpreter>();

                // Empty callback in case Execute callback is never called
                std::function<void()> callback = [](){};

                interpreter->addSystemModule("ecs", EcsModule{&ecs});
                interpreter->addSystemFunction<ExpectTrue>("ExpectTrue");
                interpreter->addSystemFunction<ExpectEq>("ExpectEq");
                interpreter->addSystemFunction<PostExecutionCallback>("ExecuteCallback", &callback);

                interpreter->interpretFromFile(script);

                ecs.executeOnce();

                callback();
            }

            EXPECT_EQ(logger.getNbError(), 0);
        }

    } // namespace test

} // namespace pg