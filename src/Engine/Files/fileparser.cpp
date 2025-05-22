#include "stdafx.h"

#include "fileparser.h"

#include <algorithm>

#include "../logger.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Parser";
    }

    FileParser::FileParser(const TextFile& file) : file(file), stream(file.data, std::ios::binary)
    {
        LOG_THIS_MEMBER(DOM);

        // Load the version of the file if it exists
        if (std::getline(stream, currentLine) and currentLine == "Version")
        {
            if (std::getline(stream, currentLine))
            {
                version = currentLine;
                LOG_INFO(DOM, "File " << file.filepath <<  " has version " << version);
            }
        }
    }

    bool FileParser::advance()
    {
        currentLine = nextLine;

        try
        {
            if (not std::getline(stream, nextLine))
            {
                LOG_ERROR(DOM, "Couldn't advance in file: " << this->file.filepath);

                return false;
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(DOM, "Couldn't advance in file: " << this->file.filepath << ", error: " << e.what());

            return false;
        }

        return true;
    }

    void FileParser::run()
    {
        LOG_THIS_MEMBER(DOM);

        try
        {
            while (std::getline(stream, nextLine))
            {
                std::for_each(callbacks.begin(), callbacks.end(), [&](const FileParser::ParsingCallback& element) { executeCallback(currentLine, element); });

                currentLine = nextLine;
            }

            std::for_each(callbacks.begin(), callbacks.end(), [&](const FileParser::ParsingCallback& element) { executeCallback(currentLine, element); });
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(DOM, "Failed to parse the file correctly, current line: " << currentLine << " error: " << e.what());
        }
    }

    void executeCallback(const std::string& line, const FileParser::ParsingCallback& callback)
    {
        if (std::regex_search(line, callback.pattern))
            callback.callback(line);
    }

}

