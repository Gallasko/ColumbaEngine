#include "logger.h"

// Only used for the console sink 
#include <iostream>
// Used for dumping to a file with file sink
#include <fstream>

#include "Memory/elementtype.h"

namespace pg
{
    std::vector<Logger::LogSinkPtr> Logger::sinks;

    namespace
    {
        template <typename Container, typename Value>
        int findSinkPos(const Value& value, const Container& container)
        {
            for (long long unsigned int i = 0; i < container.size(); i++)
                if (value == container.at(i))
                    return i;

            return -1;
        }

        std::string logLevelString(const Logger::InfoLevel& level)
        {
            std::string logLevelStringBuffer = "";

            switch (level)
            {
                case Logger::InfoLevel::log:
                    logLevelStringBuffer = "[Log]: ";
                    break;

                case Logger::InfoLevel::test:
                    logLevelStringBuffer = "[Test]: ";
                    break;

                case Logger::InfoLevel::mile:
                    logLevelStringBuffer = "[Mile]: ";
                    break;

                case Logger::InfoLevel::info:
                    logLevelStringBuffer = "[Info]: ";
                    break;

                case Logger::InfoLevel::alert:
                    logLevelStringBuffer = "[Alert]: ";
                    break;

                case Logger::InfoLevel::warning:
                    logLevelStringBuffer = "[Warning]: ";
                    break;

                case Logger::InfoLevel::error:
                    logLevelStringBuffer = "[Error]: ";
                    break;

                case Logger::InfoLevel::critical:
                    logLevelStringBuffer = "[Critical]: ";
                    break;

                default:
                    logLevelStringBuffer = "!!! [Unknown] !!!: ";
                    break;
            }

            return logLevelStringBuffer; 
        }

        std::string logPositionString(const std::string& filename, const std::string& objectName, const std::string& function, const int line)
        {
            std::string logPositionStringBuffer = filename + ":" + std::to_string(line) + ": ";

            logPositionStringBuffer += function;

            if (objectName != "")
                logPositionStringBuffer += " Object: " + std::string(objectName);

            return logPositionStringBuffer;
        }
    }

    Strfy& Strfy::operator<<(const ElementType& value)
    {
        data += value.toString();
        return *this;
    }

    void Logger::LogSink::operator<<(const Logger::Info& log)
    {
        bool accepted = true;

        // Structure binding can t be done because filter is pointer
        //for (auto [filterName, filter] : filters)
        for (const auto& filter : filters)
            if (filter.second->isFiltered(log))
                accepted = false;

        if (accepted)
            processLog(log);
    }

    void Logger::removeSink(std::shared_ptr<Logger::LogSink> sink)
    {
        const auto& logger = Logger::getLogger();

        // Fonctor to use C++ scope initialisation to easely lock log pushback
        std::lock_guard<std::recursive_mutex> lock(logger->_lock);

        const auto& it = findSinkPos(sink, sinks);

        if (it != -1)
            sinks.erase(sinks.begin() + it);
    }

    void TerminalSink::processLog(const Logger::Info& log)
    {
        if (log.level == Logger::InfoLevel::error)
            std::cout << "\033[31m";
        else if (log.level == Logger::InfoLevel::warning)
            std::cout << "\033[33m";

        std::cout << logLevelString(log.level) << "'" << log.scope << "' " << logPositionString(log.filename, log.objectName, log.function, log.line) << " " << log.message;

        // Reset any color put on the text
        std::cout << "\033[0m" << std::endl;
        
        //if(not ignoreNonErrors and log.level == Logger::InfoLevel::log)
        //    std::cout << log.line << ", " << log.filename << ", " << log.function << ", " << log.objectName << "," << log.scope << ", " << log.message << ", " << static_cast<int>(log.level) << std::endl;
    }

    void FileSink::processLog(const Logger::Info& log)
    {
        dataBuffer += Strfy() << logLevelString(log.level) << "'" << log.scope << "' " << logPositionString(log.filename, log.objectName, log.function, log.line) << " " << log.message << "\n";
        //if(not ignoreNonErrors and log.level == Logger::InfoLevel::log)
        //    std::cout << log.line << ", " << log.filename << ", " << log.function << ", " << log.objectName << "," << log.scope << ", " << log.message << ", " << static_cast<int>(log.level) << std::endl;
    }

    FileSink::~FileSink()
    {
        std::ofstream f(filename);
        f.write(dataBuffer.data(), dataBuffer.size());
    }

}
