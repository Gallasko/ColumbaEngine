#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace pg
{
    class EntitySystem;
    class StandardSystemImpl;

    /**
     * @brief One loaded script, shared by every callback that runs it.
     *
     * Callbacks hold this by shared_ptr and pin the bytecode at call time,
     * so a hot reload can never pull the bytes out from under a running VM.
     */
    class ScriptHandle
    {
        friend class ScriptRegistry;
    public:
        using Bytecode = std::shared_ptr<const std::vector<char>>;

        /**
         * @brief Pin the current version of the bytecode for one execution.
         *
         * @return The current bytecode, or nullptr if the script never
         *         compiled successfully (callers should no-op in that case).
         */
        Bytecode bytecode() const { return std::atomic_load(&code); }

        /** @brief The path the user edits (.pg), or the .pgc if precompiled. */
        const std::string& source() const { return sourcePath; }

        /** @brief The .pgc file actually loaded. */
        const std::string& compiled() const { return compiledPath; }

    private:
        std::string sourcePath;
        std::string compiledPath;
        Bytecode code;
    };

    /** @brief Sent through the ECS after a hot reload attempt was applied. */
    struct ScriptReloadedEvent
    {
        std::string sourcePath;
        bool success = false;
        std::string error;
    };

    /**
     * @brief Owns every script loaded through the ECS and hot reloads them.
     *
     * load() replaces the old getCachedScript() helper: it compiles .pg
     * sources to .pgc, reads the bytecode once, and dedupes by source path so
     * all users of a script share one ScriptHandle.
     *
     * Reload flow: pollForChanges() (called by ScriptWatcherSystem) detects
     * an mtime change, debounces it, recompiles and *stages* the new bytecode.
     * applyPendingSwaps() (called by EntitySystem between frames, while no
     * system is executing) swaps the staged bytecode into the handles and
     * emits ScriptReloadedEvent. A failed compile keeps the old bytecode
     * running and only reports the error.
     */
    class ScriptRegistry
    {
    public:
        explicit ScriptRegistry(EntitySystem* ecsRef) : ecsRef(ecsRef) {}

        /**
         * @brief Compile (if needed), load and register a script.
         *
         * @param scriptPath Path to a .pg source or a precompiled .pgc file.
         * @param sysCtx     Optional system context: registered as the "sys"
         *                   native module during compilation, and reused on
         *                   every hot recompile of this script.
         *
         * @return The shared handle, or nullptr if the extension is invalid.
         *         If compilation fails the handle is still returned (with
         *         null bytecode) so a later successful reload can revive it.
         */
        std::shared_ptr<ScriptHandle> load(const std::string& scriptPath, StandardSystemImpl* sysCtx = nullptr);

        /**
         * @brief Recompile a watched script right now and stage the swap.
         *
         * Used by pollForChanges() after debounce, and directly by tests.
         *
         * @return true if the recompile succeeded (swap staged with new
         *         bytecode), false if it failed (error swap staged).
         */
        bool reloadNow(const std::string& sourcePath);

        /** @brief mtime sweep over all watched scripts. Debounced; stages swaps via reloadNow(). */
        void pollForChanges();

        /**
         * @brief Apply staged swaps and emit ScriptReloadedEvent for each.
         *
         * Must only be called while no system is executing; EntitySystem
         * calls it from the BasicTask, right after the command dispatcher.
         */
        void applyPendingSwaps();

        /** @brief Drop a dangling compile context when a system is destroyed. */
        void onSystemRemoved(StandardSystemImpl* sysCtx);

        /** @brief Debounce delay before a changed file is recompiled (default 100ms, 0 for tests). */
        void setDebounceMs(int64_t ms) { debounceMs = ms; }

        /** @brief Observation hook fired on applyPendingSwaps(), in addition to the ECS event. */
        void setReloadCallback(std::function<void(const ScriptReloadedEvent&)> cb) { reloadCallback = std::move(cb); }

        size_t nbWatchedScripts() const;
        bool hasPendingSwaps() const;

    private:
        struct Entry
        {
            std::shared_ptr<ScriptHandle> handle;
            StandardSystemImpl* compileCtx = nullptr;
            bool compileOnLoad = false;

            std::filesystem::file_time_type lastWrite;

            // Debounce state: a new mtime was seen but not yet acted upon
            bool changePending = false;
            std::filesystem::file_time_type pendingWrite;
            std::chrono::steady_clock::time_point pendingObservedAt;
        };

        struct PendingSwap
        {
            std::shared_ptr<ScriptHandle> handle;
            ScriptHandle::Bytecode newCode;
            std::string sourcePath;
            std::string error;
        };

        // Compile sourcePath to compiledPath. Returns empty string on success, error text on failure.
        std::string compile(const std::string& sourcePath, const std::string& compiledPath, StandardSystemImpl* sysCtx);

        static std::vector<char> readAllBytes(const std::string& path);

        // Assumes the caller holds `mutex`
        bool reloadEntryLocked(const std::string& sourcePath, Entry& entry);

        std::unordered_map<std::string, Entry> entries;
        std::vector<PendingSwap> pendingSwaps;

        mutable std::mutex mutex;

        int64_t debounceMs = 100;

        std::function<void(const ScriptReloadedEvent&)> reloadCallback;

        EntitySystem* ecsRef;
    };
}
