#include "filemanager.h"

#include <stdexcept>
#include <iostream>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

#include "../logger.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "File Manager";

        TextFile openTxtFile(const std::string& filename) noexcept
        {
            LOG_THIS(DOM);

            try
            {
                // Todo put the path of the file in the textFile struct
                fs::path p {filename};

                if (not fs::exists(p))
                {
                    LOG_INFO(DOM, "Couldn't open file '" << filename << "' : File doesn't exist.");
                    return TextFile{filename, ""};
                }

                LOG_INFO(DOM, "Reading file '" << filename << "'");

                std::fstream file;

                file.open(filename, std::ios::in);

                bool firstLine = false;

                if (file.is_open())
                {
                    std::string temp;
                    std::string buffer;
                    
                    while (std::getline(file, buffer))
                    {
                        if (firstLine)
                            temp += "\n";
                        else
                            firstLine = true;

                        temp += buffer;
                    }

                    file.close();

                    // Todo check why I needed to use relative paths here ?
                    // return TextFile{p.relative_path().string(), temp};
                    return TextFile{filename, temp};
                }

                // return TextFile{p.relative_path().string(), ""};
                return TextFile{filename, ""};
            }
            catch (const std::exception& e)
            {
                LOG_INFO(DOM, "Couldn't open file '" << filename << "' : " << e.what());

                return TextFile{filename, ""};
            }
        }
    }

    TextFile ResourceAccessor::openTextFile(const std::string& filepath) noexcept
    {
        LOG_THIS(DOM);

        return openTxtFile(":/" + filepath);
    }

    std::vector<TextFile> ResourceAccessor::openTextFolder(const std::string& foldername) noexcept
    {
        LOG_THIS(DOM);

        std::vector<TextFile> folder;

        // Todo

        // foreach(const QString& fileName, QDir((":/" + foldername).c_str()).entryList())
        // {
        //     folder.push_back(openTxtFile(":/" + foldername + fileName.toStdString()));
        // }

        return folder;
    }

    TextFile FileAccessor::openTextFile(const std::string& filepath) noexcept
    {
        LOG_THIS(DOM);

        return openTxtFile(filepath);
    }

    std::vector<TextFile> FileAccessor::openTextFolder(const std::string& foldername, bool recursive) noexcept
    {
        LOG_THIS(DOM);

        std::vector<TextFile> folder;

        // Todo

        fs::path p {foldername};

        if (not fs::exists(p))
        {
            LOG_INFO(DOM, "Couldn't open folder '" << foldername << "' : Folder doesn't exist.");
            return folder;
        }

        LOG_INFO(DOM, "Opening folder '" << foldername << "'");

        if (recursive)
        {
            for (const auto& file : fs::recursive_directory_iterator(foldername))
            {
                LOG_INFO(DOM, "Opening file '" << file.path().string() << "'");

                folder.push_back(openTextFile(file.path().string()));
            }
        }
        else
        {
            for (const auto& file : fs::directory_iterator(foldername))
            {
                LOG_INFO(DOM, "Opening file '" << file.path().string() << "'");

                folder.push_back(openTextFile(file.path().string()));
            }
        }

        return folder;
    }

    bool FileAccessor::writeToFile(const TextFile& file, const std::string& data, bool truncate) noexcept
    {
        LOG_THIS(DOM);

        auto flags = truncate ? std::ofstream::out | std::ofstream::trunc : std::ofstream::out;

        try
        {
            std::ofstream p{file.filepath, flags};

            if (not p)
            {
                LOG_ERROR(DOM, "Couldn't open file '" << file.filepath << "' : File is unaccessible");
                return false;
            }
            else
            {
                p << data;
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(DOM, e.what());
            return false;
        }

        return true;
    }

    /**
     * @brief Universal file accessor for both res file and system files
     * 
     * By default, it tries to open files on the system to allow override of the base file for the users
     * 
     * If the file does not exist or is empty on the system, it then tries to open it in res file.
     * 
     * If is still empty on the res file, then the user wants to write to the system 
     * so we return the system file path 
     * 
     * @param filepath Path to the file to be accessed
     * @return TextFile Structure representing the accessed file (path and contents)
     */
    TextFile UniversalFileAccessor::openTextFile(const std::string& filepath) noexcept
    {
        LOG_THIS(DOM);

        TextFile resFile;

        auto file = FileAccessor::openTextFile(filepath);

        if (file.data == "")
            resFile = ResourceAccessor::openTextFile(filepath);

        if (resFile.data == "")
        {
            return file;
        }
        else
        {
            return resFile;
        }
    }

    std::vector<TextFile> UniversalFileAccessor::openTextFolder(const std::string& foldername) noexcept
    {
        LOG_THIS(DOM);

        std::vector<TextFile> folder = FileAccessor::openTextFolder(foldername);

        if (folder.size() == 0)
            folder = ResourceAccessor::openTextFolder(foldername);

        return folder;
    }

    bool UniversalFileAccessor::writeToFile(const TextFile& file, const std::string& data, bool truncate) noexcept
    {
        return FileAccessor::writeToFile(file, data, truncate);
    }

    std::string UniversalFileAccessor::getFileName(const TextFile& file) noexcept
    {
        LOG_THIS(DOM);

        fs::path p {file.filepath};

        return p.filename().string();
    }

    std::string UniversalFileAccessor::getFoldername(const TextFile& file) noexcept
    {
        LOG_THIS(DOM);

        fs::path p {file.filepath};

        return p.relative_path().remove_filename().string();
    }

    std::string UniversalFileAccessor::getRelativePath(const TextFile& file) noexcept
    {
        LOG_THIS(DOM);

        return file.filepath;
    }
}