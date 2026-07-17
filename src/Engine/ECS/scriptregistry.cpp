#include "stdafx.h"

#include "scriptregistry.h"

#include "entitysystem.h"
#include "sysmodule.h"

#include "Compiler/vm.h"

#include <fstream>

namespace pg
{
    namespace
    {
        constexpr const char* const DOM = "Script Registry";

        bool endsWith(const std::string& str, const std::string& suffix)
        {
            return str.size() >= suffix.size() and str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
    }

    std::shared_ptr<ScriptHandle> ScriptRegistry::load(const std::string& scriptPath, StandardSystemImpl* sysCtx)
    {
        std::string sourcePath, compiledPath;
        bool compileOnLoad = false;

        if (endsWith(scriptPath, ".pg"))
        {
            sourcePath = scriptPath;
            compiledPath = scriptPath + "c";
            compileOnLoad = true;
        }
        else if (endsWith(scriptPath, ".pgc"))
        {
            LOG_MILE(DOM, "Loading precompiled script: " << scriptPath);

            sourcePath = scriptPath;
            compiledPath = scriptPath;
        }
        else
        {
            LOG_ERROR(DOM, "Invalid script file extension. Must be .pg or .pgc: " << scriptPath);
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(mutex);

        auto it = entries.find(sourcePath);

        if (it != entries.end())
        {
            // Same script requested by another user: share the handle. Keep the
            // first compile context seen, only fill it in if we had none.
            if (it->second.compileCtx == nullptr)
                it->second.compileCtx = sysCtx;

            return it->second.handle;
        }

        Entry entry;
        entry.handle = std::make_shared<ScriptHandle>();
        entry.handle->sourcePath = sourcePath;
        entry.handle->compiledPath = compiledPath;
        entry.compileCtx = sysCtx;
        entry.compileOnLoad = compileOnLoad;

        if (compileOnLoad)
        {
            auto error = compile(sourcePath, compiledPath, sysCtx);

            // On compile error we still try to read a previously generated .pgc
            // below, so the last good version keeps running.
            if (not error.empty())
                LOG_ERROR(DOM, "Failed to compile script: " << sourcePath << " (" << error << ")");
        }

        auto bytes = readAllBytes(compiledPath);

        if (bytes.empty())
        {
            LOG_ERROR(DOM, "No bytecode available for script: " << sourcePath);
        }
        else
        {
            LOG_MILE(DOM, "Cached bytecode " << compiledPath << " (" << bytes.size() << " bytes)");

            entry.handle->code = std::make_shared<const std::vector<char>>(std::move(bytes));
        }

        std::error_code ec;
        entry.lastWrite = std::filesystem::last_write_time(sourcePath, ec);

        auto handle = entry.handle;
        entries.emplace(sourcePath, std::move(entry));

        return handle;
    }

    std::string ScriptRegistry::compile(const std::string& sourcePath, const std::string& compiledPath, StandardSystemImpl* sysCtx)
    {
        VM compiler;
        ecsRef->setupVm(compiler);

        if (sysCtx)
        {
            compiler.addNativeModule("sys", SystemModule{sysCtx});
        }

        auto result = compiler.interpretFromFile(sourcePath, true, compiledPath);

        if (result != InterpretResult::OK)
        {
            return result == InterpretResult::COMPILE_ERROR ? "compile error, see VM log" : "runtime error during compilation, see VM log";
        }

        return "";
    }

    std::vector<char> ScriptRegistry::readAllBytes(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);

        if (not file)
            return {};

        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> bytes(fileSize);
        file.read(bytes.data(), fileSize);

        if (not file)
            return {};

        return bytes;
    }

    bool ScriptRegistry::reloadEntryLocked(const std::string& sourcePath, Entry& entry)
    {
        std::error_code ec;
        entry.lastWrite = std::filesystem::last_write_time(sourcePath, ec);
        entry.changePending = false;

        if (entry.compileOnLoad)
        {
            auto error = compile(sourcePath, entry.handle->compiledPath, entry.compileCtx);

            if (not error.empty())
            {
                LOG_ERROR(DOM, "Hot reload failed for: " << sourcePath << " (" << error << "), keeping previous bytecode");

                pendingSwaps.push_back(PendingSwap{entry.handle, nullptr, sourcePath, error});
                return false;
            }
        }

        auto bytes = readAllBytes(entry.handle->compiledPath);

        if (bytes.empty())
        {
            LOG_ERROR(DOM, "Hot reload failed for: " << sourcePath << " (cannot read bytecode), keeping previous bytecode");

            pendingSwaps.push_back(PendingSwap{entry.handle, nullptr, sourcePath, "cannot read bytecode file: " + entry.handle->compiledPath});
            return false;
        }

        pendingSwaps.push_back(PendingSwap{entry.handle, std::make_shared<const std::vector<char>>(std::move(bytes)), sourcePath, ""});
        return true;
    }

    bool ScriptRegistry::reloadNow(const std::string& sourcePath)
    {
        std::lock_guard<std::mutex> lock(mutex);

        auto it = entries.find(sourcePath);

        if (it == entries.end())
        {
            LOG_ERROR(DOM, "Cannot reload unknown script: " << sourcePath);
            return false;
        }

        return reloadEntryLocked(sourcePath, it->second);
    }

    void ScriptRegistry::pollForChanges()
    {
        std::lock_guard<std::mutex> lock(mutex);

        const auto now = std::chrono::steady_clock::now();

        for (auto& [sourcePath, entry] : entries)
        {
            std::error_code ec;
            auto mtime = std::filesystem::last_write_time(sourcePath, ec);

            // File temporarily missing (editors save via temp file + rename)
            if (ec)
                continue;

            if (not entry.changePending)
            {
                if (mtime != entry.lastWrite)
                {
                    entry.changePending = true;
                    entry.pendingWrite = mtime;
                    entry.pendingObservedAt = now;
                }

                continue;
            }

            // Still being written: restart the debounce timer
            if (mtime != entry.pendingWrite)
            {
                entry.pendingWrite = mtime;
                entry.pendingObservedAt = now;
                continue;
            }

            const auto stableFor = std::chrono::duration_cast<std::chrono::milliseconds>(now - entry.pendingObservedAt).count();

            if (stableFor >= debounceMs)
            {
                LOG_INFO(DOM, "Script changed on disk, reloading: " << sourcePath);

                reloadEntryLocked(sourcePath, entry);
            }
        }
    }

    void ScriptRegistry::applyPendingSwaps()
    {
        std::vector<PendingSwap> swaps;

        {
            std::lock_guard<std::mutex> lock(mutex);
            swaps.swap(pendingSwaps);
        }

        for (auto& swap : swaps)
        {
            if (swap.newCode)
            {
                std::atomic_store(&swap.handle->code, swap.newCode);

                LOG_INFO(DOM, "Hot reloaded script: " << swap.sourcePath);
            }

            ScriptReloadedEvent event{swap.sourcePath, swap.newCode != nullptr, swap.error};

            if (reloadCallback)
                reloadCallback(event);

            ecsRef->sendEvent(event);
        }
    }

    void ScriptRegistry::onSystemRemoved(StandardSystemImpl* sysCtx)
    {
        std::lock_guard<std::mutex> lock(mutex);

        for (auto& [sourcePath, entry] : entries)
        {
            if (entry.compileCtx == sysCtx)
                entry.compileCtx = nullptr;
        }
    }

    size_t ScriptRegistry::nbWatchedScripts() const
    {
        std::lock_guard<std::mutex> lock(mutex);

        return entries.size();
    }

    bool ScriptRegistry::hasPendingSwaps() const
    {
        std::lock_guard<std::mutex> lock(mutex);

        return not pendingSwaps.empty();
    }
}
