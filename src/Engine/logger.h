#pragma once

#include <unordered_map>
#include <vector>

#include <memory>
#include <mutex>

#include <string>

#include <chrono>

// Todo include date when printing log 
#include "Helpers/date.h"

#include "Files/filemanager.h"

#define _CONCAT_(x, y) x ## y
#define CONCAT(x, y) _CONCAT_(x, y)

#define _LOG(value) pg::Logger::Logging CONCAT(_anonymous, __LINE__) = value

#define _SINGLE_LOG(scope, msg, level) pg::Logger::_single_log(__LINE__, __FILE__ != nullptr ? std::string(__FILE__) : "", __func__ != nullptr ? std::string(__func__) : "", 0, "", scope, pg::Strfy() << msg, level)

#define LOG_TEST(scope, msg) _SINGLE_LOG(scope, msg, pg::Logger::InfoLevel::test)

#ifdef DEBUG
#define LOG_THIS(scope)
#define LOG_THIS_MEMBER(scope)
#define LOG_MILE(scope, msg)
// #define LOG_THIS(scope) _LOG(pg::Logger::_log(__LINE__, __FILE__ != nullptr ? std::string(__FILE__) : "", __func__ != nullptr ? std::string(__func__) : "", 0, "", scope, "", pg::Logger::InfoLevel::log))
// #define LOG_THIS_MEMBER(scope) _LOG(pg::Logger::_log(__LINE__, __FILE__ != nullptr ? std::string(__FILE__) : "", __func__ != nullptr ? std::string(__func__) : "", this, typeid(*this).name(), scope, "", pg::Logger::InfoLevel::log))
// #define LOG_MILE(scope, msg) _SINGLE_LOG(scope, msg, pg::Logger::InfoLevel::mile)
#define LOG_INFO(scope, msg) _SINGLE_LOG(scope, msg, pg::Logger::InfoLevel::info)
#define LOG_ERROR(scope, msg) _SINGLE_LOG(scope, msg, pg::Logger::InfoLevel::error)
#else
#define LOG_THIS(scope)
#define LOG_THIS_MEMBER(scope)
#define LOG_MILE(scope, msg)

#ifdef __EMSCRIPTEN__
#define LOG_INFO(scope, msg)
#define LOG_ERROR(scope, msg)
// #define LOG_INFO(scope, msg) _SINGLE_LOG(scope, msg, pg::Logger::InfoLevel::info)
// #define LOG_ERROR(scope, msg) _SINGLE_LOG(scope, msg, pg::Logger::InfoLevel::error)
#else
// Let those even in release

// #define LOG_THIS(scope) _LOG(pg::Logger::_log(__LINE__, __FILE__ != nullptr ? std::string(__FILE__) : "", __func__ != nullptr ? std::string(__func__) : "", 0, "", scope, "", pg::Logger::InfoLevel::log))
// #define LOG_THIS_MEMBER(scope) _LOG(pg::Logger::_log(__LINE__, __FILE__ != nullptr ? std::string(__FILE__) : "", __func__ != nullptr ? std::string(__func__) : "", this, typeid(*this).name(), scope, "", pg::Logger::InfoLevel::log))
// #define LOG_TEST(scope, msg) _SINGLE_LOG(scope, msg, pg::Logger::InfoLevel::test)
// #define LOG_MILE(scope, msg) _SINGLE_LOG(scope, msg, pg::Logger::InfoLevel::mile)

#define LOG_INFO(scope, msg) _SINGLE_LOG(scope, msg, pg::Logger::InfoLevel::info)
#define LOG_ERROR(scope, msg) _SINGLE_LOG(scope, msg, pg::Logger::InfoLevel::error)
#endif
#endif

//TODO Filter only filter log, info, alert and warning level message -> error and critical are not filtered !
//TODO if a log is whitelisted it is shown even if it/was blacklisted

namespace pg
{
    class ElementType;

    /**
     * @brief Helper struct to stringfy log messages
     * 
     */
    class Strfy 
    {
    public:
        Strfy(const std::string& msg) : data(msg) {}
        Strfy() : data("") {}

        template <typename T>
        Strfy& operator<<(const T& value)
        {
            data += std::to_string(value);
            return *this;
        }

        Strfy& operator<<(const std::string& value)
        {
            data += value;
            return *this;
        }

        Strfy& operator<<(std::string_view value)
        {
            data += value;
            return *this;
        }

        Strfy& operator<<(const ElementType& value);

        Strfy& operator<<(char* value)
        {
            data += value;
            return *this;
        }

        Strfy& operator<<(const char* value)
        {
            data += value;
            return *this;
        }

        Strfy& operator<<(const unsigned char* value)
        {
            data += reinterpret_cast<const char*>(value);
            return *this;
        }

        Strfy& operator<<(const Strfy& value)
        {
            data += value.data;
            return *this;
        }

        explicit operator std::string() const
        {
            return data;
        }

        operator const char*() const
        {
            return data.c_str();
        }

        std::string getData() const { return data; }

    private:
        std::string data;
    };

    /**
     * @class Logger
     * 
     * Main logging class responsible of managing logging sinks and print various info of the code
     * 
     */
    class Logger
    {
    public:
        /**
         * @enum LogLevel
         * 
         * A enum class holding the emergency level of the log
         * 
         * @todo add a debug level
         */
        enum class InfoLevel
        {
            log      = 0, ///< Log level used anywhere for basic logging
            test     = 1, ///< Log level only used for testing purposes
            mile     = 2, ///< Log level used to print relevant information of some in build data types
            info     = 3, ///< Info level used to print some important and informative message about the execution of the code
            alert    = 4, ///< Alert level used to alert the dev of weird branchings that can affect the output 
            warning  = 5, ///< Warning level used to warn the developer of an error that is non blocking 
            error    = 6, ///< Error level used to tell the developer of an error that is blocking and may need a restart of a component
            critical = 7  ///< Critical level used to tell the developer of an error that is critical to the integrity of the application and need a full reboot of it
        };

        /**
         * @struct Info
         * 
         * A structure holding all the information about a log message
         */
        struct Info
        {
            const int line;                   ///< The line number of the message
            const std::string filename;       ///< The name of the file where the log message happened
            const std::string function;       ///< The name of the function where the log message happened
            const void* object;               ///< A pointer to the object where the log message happened
            const std::string objectName;     ///< The name of the object class where the log message happened

            const std::string scope;          ///< The scope of the log
            const std::string message;        ///< The message string

            const InfoLevel level;            ///< The emergency level of the log
        };

        class Logging
        {
        friend class Logger;
        public:
            Logging(const int line, const std::string& file, const std::string& function, const void* object, const std::string& objectName, const std::string& scope, const std::string& msg, const Logger::InfoLevel& level)
             : line(line), file(file), function(function), object(object), objectName(objectName), scope(scope), msg(msg), level(level)
            {
                auto& logger = Logger::getLogger();
                
                // Fonctor to use C++ scope initialisation to easely lock log pushback
                // std::lock_guard<std::recursive_mutex> lock(logger->_lock);

                const auto log = Logger::Info{line, this->file, "Enter in: '" + function + "'", object, this->objectName, this->scope, this->msg, level};

                // Call all the sink registered and push the received message to them
                for (const auto& sink : logger->sinks)
                    *sink << log; 
            }

            ~Logging()
            {
                auto& logger = Logger::getLogger();

                // Fonctor to use C++ scope initialisation to easely lock log pushback
                // std::lock_guard<std::recursive_mutex> lock(logger->_lock);

                const auto log = Logger::Info{line, file, "Exit out: '" + function + "'", object, objectName, scope, msg, level};

                // Call all the sink registered and push the received message to them
                for (const auto& sink : logger->sinks)
                    *sink << log;  
            }

            const int line;
            const std::string file;
            const std::string function;
            const void* object;
            const std::string objectName;
            const std::string scope;
            const std::string msg;
            const Logger::InfoLevel level;
        };

        /**
         * @class LogSink
         * 
         * Pure virtual class to create derive classes used to be end points of the logs system
         * 
         */
        class LogSink
        {
        friend class Logger;
        public:
            class Filter
            {
            public:
                virtual ~Filter() {}

                /**
                 * @brief Check if the log need to be filtered
                 * 
                 * @param log The log to be checked
                 * @return true if the log need to be filtered out and false if the filter doesn t apply to the log
                 */
                virtual bool isFiltered(const Logger::Info& log) const = 0;
            };

            class FilterFile : public Filter
            {
            public:
                FilterFile(std::string_view filename, bool blacklisted = true) : filename(filename), blacklisted(blacklisted) {}
                
                inline virtual bool isFiltered(const Logger::Info& log) const
                {
                    return blacklisted ? log.filename == filename : log.filename != filename; 
                }

            private:
                std::string filename;
                bool blacklisted;
            };

            class FilterFunction : public Filter
            {
            public:
                FilterFunction(std::string_view function, bool blacklisted = true) : function(function), blacklisted(blacklisted) {}
                
                inline virtual bool isFiltered(const Logger::Info& log) const
                {
                    return blacklisted ? log.function == function : log.function != function; 
                }

            private:
                std::string function;
                bool blacklisted;
            };

            // TODO check if this function is relevant and if it doesn t crash if ptr a non existant
            class FilterObject : public Filter
            {
            public:
                FilterObject(void* object, bool blacklisted = true) : object(object), blacklisted(blacklisted) {}
                
                inline virtual bool isFiltered(const Logger::Info& log) const
                {
                    if (object == nullptr or log.object == nullptr)
                        return true;
    
                    return blacklisted ? log.object == object : log.object != object; 
                }

            private:
                void* object;
                bool blacklisted;
            };

            class FilterObjectName : public Filter
            {
            public:
                FilterObjectName(std::string_view objectName, bool blacklisted = true) : objectName(objectName), blacklisted(blacklisted) {}
                
                inline virtual bool isFiltered(const Logger::Info& log) const
                {
                    return blacklisted ? log.objectName == objectName : log.objectName != objectName; 
                }

            private:
                std::string objectName;
                bool blacklisted;
            };

            class FilterScope : public Filter
            {
            public:
                FilterScope(std::string_view scope, bool blacklisted = true) : scope(scope), blacklisted(blacklisted) {}
                
                inline virtual bool isFiltered(const Logger::Info& log) const
                {
                    return blacklisted ? log.scope == scope : log.scope != scope; 
                }

            private:
                std::string scope;
                bool blacklisted;
            };

            class FilterLogLevel : public Filter
            {
            public:
                FilterLogLevel(const Logger::InfoLevel& level, bool blacklisted = true) : level(level), blacklisted(blacklisted) {}
                
                inline virtual bool isFiltered(const Logger::Info& log) const
                {
                    return blacklisted ? log.level == level : log.level != level; 
                }

            private:
                Logger::InfoLevel level;
                bool blacklisted;
            };

            // Todo delete all filters registered
            /** Virtual destructor for LogSink's children */
            virtual ~LogSink() {}

            // Todo: check if a filter is not already set with this filtername
            // Todo: do a function that remove the filter
            // Todo: when adding a filter with whitelist activated (blacklisted set to false) => make all call to operator<< only accept log if it is in the whitelist
            inline void addFilter(const std::string& filterName, Filter* filter) { filters[filterName] = filter; }

            /** Stream operator used to push the log into the sink */
            virtual void operator<<(const Logger::Info& log);

        private:
            virtual void processLog(const Logger::Info& log) = 0;

            // Todo delete all the element in it
            std::unordered_map<std::string, Filter*> filters;
        };

        // Typedefs

        /** Logger unique pointer type definition */
        typedef std::unique_ptr<Logger> LoggerPtr;
        /** LogSink unique pointer type definition */
        typedef std::shared_ptr<LogSink> LogSinkPtr;

    public:
        /**
         * @brief Register a sink to dumb log into
         * 
         * @tparam Sink Which type of sink to be initialised
         * @tparam Args Variadic list of arguments to initialize the sink
         * @param args Arguments of the sink to be initialised
         * @return a pointer to the sink instance registered 
         */
        template <typename Sink, typename... Args>
        inline static std::shared_ptr<Logger::LogSink> registerSink(Args... args);

        static void removeSink(std::shared_ptr<Logger::LogSink> sink);

        /**
         * @brief Main function used to register a single log message
         * 
         * @param line          Line where the log message happened
         * @param file          File where the log message happened
         * @param function      Function where the log message happened
         * @param object        A reference to the object where the log message happened
         * @param objectName    Name of the object where the log message happened
         * @param scope         Scope of the message for filtering and priority
         * @param msg           Message to be logged
         * @param level         Level of emergency of the message
         */
        inline static void _single_log(const int line, const std::string& file, const std::string& function, const void* object, const std::string& objectName, const std::string& scope, const std::string& msg, const Logger::InfoLevel& level)
        {
            // const auto& logger = Logger::getLogger();

            // Fonctor to use C++ scope initialisation to easely lock log pushback
            // std::lock_guard<std::recursive_mutex> lock(logger->_lock);

            const auto log = Logger::Info{line, file, "In function: '" + std::string(function) + "'", object, objectName, scope, msg, level};

            // Call all the sink registered and push the received message to them
            for (const auto& sink : sinks)
                *sink << log;
        }

         /**
         * @brief Overload function used to register a single log message
         * 
         * @param[in] line          Line where the log message happened
         * @param[in] file          File where the log message happened
         * @param[in] function      Function where the log message happened
         * @param[in] object        A reference to the object where the log message happened
         * @param[in] objectName    Name of the object where the log message happened
         * @param[in] scope         Scope of the message for filtering and priority
         * @param[in] msg           Message to be logged
         * @param[in] level         Level of emergency of the message
         */
        inline static void _single_log(const int line, const std::string& file, const std::string& function, const void* object, const std::string& objectName, const std::string& scope, const Strfy& msg, const Logger::InfoLevel& level)
        {
            _single_log(line, file, function, object, objectName, scope, msg.getData(), level);
        }

        /**
         * @brief Main function used to register a log message
         * 
         * @param[in] line          Line where the log message happened
         * @param[in] file          File where the log message happened
         * @param[in] function      Function where the log message happened
         * @param[in] object        A reference to the object where the log message happened
         * @param[in] objectName    Name of the object where the log message happened
         * @param[in] scope         Scope of the message for filtering and priority
         * @param[in] msg           Message to be logged
         * @param[in] level         Level of emergency of the message
         */
        inline static Logging _log(const int line, const std::string& file, const std::string& function, const void* object, const std::string& objectName, const std::string& scope, const std::string& msg, const Logger::InfoLevel& level)
        {
            return Logging(line, file, function, object, objectName, scope, msg, level);
        }

        /**
         * @brief Overload function used to register a log message
         * 
         * @param[in] line          Line where the log message happened
         * @param[in] file          File where the log message happened
         * @param[in] function      Function where the log message happened
         * @param[in] object        A reference to the object where the log message happened
         * @param[in] objectName    Name of the object where the log message happened
         * @param[in] scope         Scope of the message for filtering and priority
         * @param[in] msg           Message to be logged
         * @param[in] level         Level of emergency of the message
         */
        // inline static Logging _log(const int line, std::string_view file, std::string_view function, const void* object, std::string_view objectName, std::string_view scope, std::string_view msg, const Logger::InfoLevel& level)
        // {
        //     // Call the log function
        //     return _log(line, file, function, object, objectName, scope, msg, level);
        // }

        /**
         * @brief Get the reference of the unique Logger instance
         * 
         * @return a pointer to the logger instance.
         * 
         * This function create an Logger object the first time it is called and then return an unique reference to this object
         *
         */
        inline static const LoggerPtr& getLogger() { static LoggerPtr logger = LoggerPtr(new Logger()); return logger; }

        /**
         * @brief Get the number of sinks currently attached to the logger subsystem
         * 
         * @return the number of sink
         */
        inline static unsigned int getNbSink() { return sinks.size(); }

    // TODO commit log to file when a certain threashold of message is passed
    // Define in #define max log length
    private:
        /** Current batch of log */
        static std::vector<LogSinkPtr> sinks;
        
        /** Mutex for pushing and accessing logs */ 
        mutable std::recursive_mutex _lock;
    };

    // Todo test and change with Args&&
    template <typename Sink, typename... Args>
    std::shared_ptr<Logger::LogSink> Logger::registerSink(Args... args)
    {
        const auto& logger = Logger::getLogger();
        // Fonctor to use C++ scope initialisation to easely lock log pushback
        std::lock_guard<std::recursive_mutex> lock(logger->_lock);

        // Create an unique reference to the sink created
        std::shared_ptr<Logger::LogSink> sink = std::make_shared<Sink>(args...);

        // Push back the sink in the vector
        sinks.push_back(sink);

        return sink;
    }

    // TODO create a filter function for the sinks to escape message depending on the scope
    class TerminalSink : public Logger::LogSink
    {
    friend class Logger;
    public:
        TerminalSink() : ignoreNonErrors(false) {}
        TerminalSink(bool ignoreNonErrors) : ignoreNonErrors(ignoreNonErrors) {}
        
        virtual ~TerminalSink() {}

        /** Stream operator used to get the log and print the message to the console */
        virtual void processLog(const Logger::Info& log) override;

    private:
        /** Flag indicating whether we should ignore errors or not */
        bool ignoreNonErrors;
    };

    class FileSink : public Logger::LogSink
    {
    friend class Logger;
    public:
        FileSink(const std::string& fileName = "log.txt", bool ignoreNonErrors = false) : filename(fileName), dataBuffer(""), ignoreNonErrors(ignoreNonErrors) {}
        
        virtual ~FileSink() override;
        
        /** Stream operator used to get the log and print the message to the console */
        virtual void processLog(const Logger::Info& log) override;

    private:
        /** The file to write the log */
        std::string filename;

        /** The actual data to be written */
        std::string dataBuffer;

        /** Flag indicating whether we should ignore errors or not */
        bool ignoreNonErrors;
    };
}
