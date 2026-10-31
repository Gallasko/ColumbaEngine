#include "modfiles.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

namespace fs = std::filesystem;

namespace arena::net
{
    namespace
    {
        bool endsWith(const std::string& s, const std::string& end)
        {
            return s.size() >= end.size() and s.compare(s.size() - end.size(), end.size(), end) == 0;
        }

        bool modFile(const std::string& path)
        {
            return endsWith(path, ".json") or endsWith(path, ".pg");
        }

        // CON, NUL, COM1... are devices on Windows, whatever the extension
        bool reservedName(const std::string& part)
        {
            std::string stem = part.substr(0, part.find('.'));
            std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (stem == "con" or stem == "prn" or stem == "aux" or stem == "nul")
                return true;

            return stem.size() == 4 and (stem.compare(0, 3, "com") == 0 or stem.compare(0, 3, "lpt") == 0) and std::isdigit(static_cast<unsigned char>(stem[3]));
        }

        bool safePart(const std::string& part)
        {
            if (part.empty() or part.front() == '.' or part.back() == '.' or reservedName(part))
                return false;

            return std::all_of(part.begin(), part.end(), [](unsigned char c) { return std::isalnum(c) or c == '.' or c == '_' or c == '-'; });
        }
    }

    std::vector<ModFile> readModFiles(const std::string& dir)
    {
        std::vector<ModFile> files;
        std::error_code ec;

        for (fs::recursive_directory_iterator it(dir, ec), end; not ec and it != end; it.increment(ec))
        {
            if (not it->is_regular_file(ec))
                continue;

            const std::string path = fs::relative(it->path(), dir, ec).generic_string();

            if (ec or not modFile(path))
                continue;

            std::ifstream in(it->path(), std::ios::binary);
            files.push_back({path, std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>())});
        }

        std::sort(files.begin(), files.end(), [](const ModFile& a, const ModFile& b) { return a.path < b.path; });
        return files;
    }

    bool safeModPath(const std::string& path)
    {
        if (path.empty() or path.size() > 200 or not modFile(path))
            return false;

        std::stringstream parts(path);
        std::string part;

        while (std::getline(parts, part, '/'))
            if (not safePart(part))
                return false;

        // getline leaves a trailing slash unseen, but such a path has no .json or .pg at its end
        return path.front() != '/';
    }

    bool writeModFiles(const std::vector<ModFile>& files, const std::string& dir, std::string& error)
    {
        if (files.empty() or files.size() > MAX_MOD_FILES)
        {
            error = std::to_string(files.size()) + " files";
            return false;
        }

        size_t total = 0;

        for (const auto& f : files)
        {
            if (not safeModPath(f.path))
            {
                error = "unsafe path " + f.path;
                return false;
            }

            total += f.data.size();
        }

        if (total > MAX_MOD_BYTES)
        {
            error = "too big (" + std::to_string(total) + " bytes)";
            return false;
        }

        std::error_code ec;
        fs::remove_all(dir, ec);

        for (const auto& f : files)
        {
            const fs::path target = fs::path(dir) / f.path;

            fs::create_directories(target.parent_path(), ec);

            std::ofstream out(target, std::ios::binary | std::ios::trunc);
            out.write(f.data.data(), static_cast<std::streamsize>(f.data.size()));

            if (not out)
            {
                error = "cannot write " + target.string();
                return false;
            }
        }

        return true;
    }
}
