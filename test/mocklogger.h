#pragma once

#include <mutex>

#include <gtest/gtest.h>

#include "logger.h"

namespace pg
{
    struct LogMessage
    {
        std::string message;
        Logger::InfoLevel level;
    };

    struct MockSink : public Logger::LogSink
    {
        virtual void processLog(const Logger::Info&) override {}
    };

    template <typename SinkType>
    class TestSink : public Logger::LogSink
    {
    friend class Logger;
    public:
        TestSink(bool showObject) : Logger::LogSink(), showObject(showObject)
        {
            resetSink();
        }

        virtual ~TestSink() {}

        void resetSink()
        {
            std::lock_guard<std::mutex> guard(lock);

            nbMessages[Logger::InfoLevel::log]      = 0;
            nbMessages[Logger::InfoLevel::test]     = 0;
            nbMessages[Logger::InfoLevel::mile]     = 0;
            nbMessages[Logger::InfoLevel::info]     = 0;
            nbMessages[Logger::InfoLevel::alert]    = 0;
            nbMessages[Logger::InfoLevel::warning]  = 0;
            nbMessages[Logger::InfoLevel::error]    = 0;
            nbMessages[Logger::InfoLevel::critical] = 0;

            lastMessage = {"", Logger::InfoLevel::log};
        }

        const std::unordered_map<Logger::InfoLevel, unsigned int>& getNbMessages() const { return nbMessages; }
        const LogMessage& getLastMessage() const { return lastMessage; }
        
        // Systems log from the ECS's worker threads, and the Logger does not lock around its sinks
        virtual void processLog(const Logger::Info& log) override
        {
            std::lock_guard<std::mutex> guard(lock);

            std::string objectName = showObject ? log.objectName : "";

            underlyingSink.processLog({log.line, log.filename, log.function, log.object, objectName, log.scope, log.message, log.level});

            nbMessages[log.level]++;

            lastMessage = {log.message, log.level};
        }

    private:
        SinkType underlyingSink;
        std::unordered_map<Logger::InfoLevel, unsigned int> nbMessages;

        LogMessage lastMessage = {"", Logger::InfoLevel::log};

        const bool showObject;

        std::mutex lock;
    };

    template <typename SinkType = MockSink>
    class MockLogger
    {
    public:
        MockLogger(bool showObject = false)
        {
            sink = Logger::registerSink<TestSink<SinkType>>(showObject);
        }
        ~MockLogger()
        {
            Logger::removeSink(sink);
        }

        void reset()
        {
            auto pointer = std::static_pointer_cast<TestSink<SinkType>>(sink);

            pointer->resetSink();
        }

        unsigned int getNbLog() const
        {
            auto pointer = std::static_pointer_cast<TestSink<SinkType>>(sink);

            return pointer->getNbMessages().at(Logger::InfoLevel::log);
        }

        unsigned int getNbTest() const
        {
            auto pointer = std::static_pointer_cast<TestSink<SinkType>>(sink);

            return pointer->getNbMessages().at(Logger::InfoLevel::test);
        }

        unsigned int getNbMile() const
        {
            auto pointer = std::static_pointer_cast<TestSink<SinkType>>(sink);

            return pointer->getNbMessages().at(Logger::InfoLevel::mile);
        }

        unsigned int getNbInfo() const
        {
            auto pointer = std::static_pointer_cast<TestSink<SinkType>>(sink);

            return pointer->getNbMessages().at(Logger::InfoLevel::info);
        }

        unsigned int getNbAlert() const
        {
            auto pointer = std::static_pointer_cast<TestSink<SinkType>>(sink);

            return pointer->getNbMessages().at(Logger::InfoLevel::alert);
        }

        unsigned int getNbWarning() const
        {
            auto pointer = std::static_pointer_cast<TestSink<SinkType>>(sink);

            return pointer->getNbMessages().at(Logger::InfoLevel::warning);
        }

        unsigned int getNbError() const
        {
            auto pointer = std::static_pointer_cast<TestSink<SinkType>>(sink);

            return pointer->getNbMessages().at(Logger::InfoLevel::error);
        }

        unsigned int getNbCritical() const
        {
            auto pointer = std::static_pointer_cast<TestSink<SinkType>>(sink);

            return pointer->getNbMessages().at(Logger::InfoLevel::critical);
        }

        const LogMessage& getLastMessage() const
        {
            auto pointer = std::static_pointer_cast<TestSink<SinkType>>(sink);

            return pointer->getLastMessage();
        }

        void addFilter(const std::string& filterName, Logger::LogSink::Filter* filter) { sink->addFilter(filterName, filter); }

    private:
        std::shared_ptr<Logger::LogSink> sink;
    };
}