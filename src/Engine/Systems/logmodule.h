#pragma once

#include "logger.h"

#include "Interpreter/pginterpreter.h"

namespace pg
{
    // Todo change and remove
    class TestPrint : public Function
    {
        using Function::Function;
    public:
        void setUp()
        {
            setArity(1, 1);
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            // TODO: change this

            auto v = args.front()->getElement();
            args.pop();

            // Todo
            std::cout << "[Interpreter]: " << v.toString() << std::endl;

            return nullptr;
        }
    };

    class DebugPrint : public Function
    {
        using Function::Function;
    public:
        void setUp()
        {
            setArity(1, 1);
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            // TODO: change this

            auto v = args.front();
            args.pop();

            std::cout << "[Interpreter]: " << v->getElement().toString() << std::endl;

            if (v->getType() == "ClassInstance")
            {
                auto obj = std::static_pointer_cast<ClassInstance>(v);

                const auto& fields = obj->getFields();

                for (const auto& field : fields)
                {
                    std::cout << "[Field] " << field.key << " : " << field.value->getElement().toString() << std::endl;
                }

                const auto& methods = obj->getMethods();

                for (const auto& method : methods)
                {
                    std::cout << "[Method] " << method.first << " : " << method.second->getElement().toString() << std::endl;
                }
            }

            return nullptr;
        }
    };

    class AddFilterScopeFunction : public Function
    {
        using Function::Function;
    public:
        void setUp(std::shared_ptr<Logger::LogSink> sink)
        {
            setArity(2, 2);
            
            this->sink = sink;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto name = args.front()->getElement();
            args.pop();

            auto scope = args.front()->getElement();
            args.pop();

            if(not name.isLitteral() or not scope.isLitteral())
            {
                LOG_ERROR("AddFilterScopeFunction", "Cannot create apply a new filter on sink, values passed are not litterals");
                return nullptr;
            }

            if (sink)
                sink->addFilter(name.toString(), new pg::Logger::LogSink::FilterScope(scope.toString()));
            else
            {
                LOG_ERROR("AddFilterScopeFunction", "Trying to add a filter to a sinkless log module");
            }

            return nullptr;
        }

        std::shared_ptr<Logger::LogSink> sink;
    };

    class AddFilterLevelFunction : public Function
    {
        using Function::Function;
    public:
        void setUp(std::shared_ptr<Logger::LogSink> sink)
        {
            setArity(2, 2);
            
            this->sink = sink;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto name = args.front()->getElement();
            args.pop();

            auto scope = args.front()->getElement();
            args.pop();

            if(not name.isLitteral() or not scope.isLitteral())
            {
                LOG_ERROR("AddFilterLevelFunction", "Cannot create apply a new filter on sink, values passed are not litterals");
                return nullptr;
            }

            pg::Logger::InfoLevel logLevel;

            if(scope.toString() == "log")
                logLevel = pg::Logger::InfoLevel::log;
            else if(scope.toString() == "info")
                logLevel = pg::Logger::InfoLevel::info;
            else if(scope.toString() == "mile")
                logLevel = pg::Logger::InfoLevel::mile;
            else if(scope.toString() == "test")
                logLevel = pg::Logger::InfoLevel::test;
            else if(scope.toString() == "error")
                logLevel = pg::Logger::InfoLevel::error;
            else
            {
                LOG_ERROR("AddFilterLevelFunction", "Trying to filter an unknown log level: " << scope.toString());
                return nullptr;
            }

            if (sink)
                sink->addFilter(name.toString(), new pg::Logger::LogSink::FilterLogLevel(logLevel));
            else
            {
                LOG_ERROR("AddFilterLevelFunction", "Trying to add a filter to a sinkless log module");
            }

            return nullptr;
        }

        std::shared_ptr<Logger::LogSink> sink;
    };

    struct LogModule : public SysModule
    {
        LogModule(std::shared_ptr<Logger::LogSink> terminalSink)
        {            
            addSystemFunction<TestPrint>("log");
            addSystemFunction<DebugPrint>("debugLog");
            addSystemFunction<AddFilterScopeFunction>("addFilterScope", terminalSink);
            addSystemFunction<AddFilterLevelFunction>("addFilterLevel", terminalSink);
        }
    };
}