#include "stdafx.h"

#include "module_cache.h"

#include "vm.h"
#include "chunk_serializer.h"
#include "ast/ast_pass.h"

#include "Files/filemanager.h"
#include "logger.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace pg
{
    namespace modulecache
    {
        namespace
        {
            const char* DOM = "ModuleCache";

            // How many compiles of one module are kept: one a program that tells it something else
            constexpr size_t KeptByModule = 8;

            // What a module's compile depends on, gathered while it runs
            struct Reads
            {
                std::vector<std::pair<std::string, std::string>> sources;  // Path, source: the module first
                std::vector<std::string> absent;                           // Imports that are no file: natives
                std::set<std::string> names;                               // Every name the sources call
                std::vector<std::string> natives;                          // The native modules they import
                bool keepable = true;                                      // False: it read bytecode

                void add(const Reads& other)
                {
                    sources.insert(sources.end(), other.sources.begin(), other.sources.end());
                    absent.insert(absent.end(), other.absent.begin(), other.absent.end());
                    names.insert(other.names.begin(), other.names.end());

                    for (const auto& native : other.natives)
                        addNative(native);

                    keepable = keepable and other.keepable;
                }

                void addNative(const std::string& native)
                {
                    if (std::find(natives.begin(), natives.end(), native) == natives.end())
                        natives.push_back(native);
                }
            };

            struct Entry
            {
                Reads reads;
                std::string setup;                              // The VM's passes
                std::string facts;                              // What the passes told it
                std::vector<std::shared_ptr<const void>> keep;  // The nodes the facts name by address
                std::string bytes;                              // Its script function's chunk, serialized
            };

            std::mutex mutex;
            std::unordered_map<std::string, std::vector<std::shared_ptr<const Entry>>> entries;

            std::atomic<size_t> compiledCount{0};
            std::atomic<size_t> takenCount{0};

            // The modules being compiled from source, innermost last. A compile runs on one thread,
            // its imports compiled inside it
            thread_local std::vector<Reads> compiling;

            std::string setupOf(VM* vm)
            {
                std::string setup = vm->enableOptimizations ? "O;" : "-;";

                if (vm->astPassManager)
                    setup += vm->astPassManager->passNames();

                return setup + "|" + vm->passManager.passNames();
            }

            std::string factsOf(VM* vm, const std::set<std::string>& names, std::vector<std::shared_ptr<const void>>& keep)
            {
                // No pass runs without optimizations: nothing they learnt reaches the module
                if (not vm->enableOptimizations or not vm->astPassManager)
                    return "";

                return vm->astPassManager->moduleFacts(names, keep);
            }

            bool importsBytecode(const std::string& fullPath)
            {
                return UniversalFileAccessor::exists(fullPath + "c") or UniversalFileAccessor::exists(fullPath + ".pgc");
            }

            // The files still what they were when the entry was made: the import would read them
            // the same, from the same sources
            bool stillRead(const Reads& reads)
            {
                for (size_t i = 0; i < reads.sources.size(); ++i)
                {
                    const auto& [path, source] = reads.sources[i];

                    // The module itself was just read by the caller, beside no bytecode
                    if (i == 0)
                        continue;

                    if (importsBytecode(path) or not UniversalFileAccessor::exists(path))
                        return false;

                    if (UniversalFileAccessor::openTextFile(path).data != source)
                        return false;
                }

                for (const auto& path : reads.absent)
                {
                    if (UniversalFileAccessor::exists(path) or importsBytecode(path))
                        return false;
                }

                return true;
            }

            // The module's script function in this VM, from its bytecode, as an import of its .pgc makes it
            Value remake(VM* vm, const Entry& entry, const std::string& moduleName)
            {
                std::istringstream in(entry.bytes);
                Chunk chunk;

                if (not ChunkSerializer::deserialize(chunk, in, vm))
                    return 0x0;

                for (const auto& native : entry.reads.natives)
                    vm->loadNativeModule(native);

                Value function = vm->createFunction();
                ObjFunction* object = vm->asFunction(function);

                object->chunk = std::move(chunk);
                object->name = moduleName;
                object->arity = 0;
                object->upvalueCount = 0;
                object->precompiled = true;

                return function;
            }

            void store(const std::string& fullPath, std::shared_ptr<const Entry> entry)
            {
                std::lock_guard<std::mutex> lock(mutex);

                auto& kept = entries[fullPath];

                kept.insert(kept.begin(), std::move(entry));

                if (kept.size() > KeptByModule)
                    kept.resize(KeptByModule);
            }
        }

        Value find(VM* vm, const std::string& fullPath, const std::string& moduleName, const std::string& source)
        {
            std::vector<std::shared_ptr<const Entry>> kept;

            {
                std::lock_guard<std::mutex> lock(mutex);

                const auto it = entries.find(fullPath);

                if (it == entries.end())
                    return 0x0;

                kept = it->second;
            }

            const std::string setup = setupOf(vm);

            for (const auto& entry : kept)
            {
                if (entry->setup != setup or entry->reads.sources.front().second != source or not stillRead(entry->reads))
                    continue;

                std::vector<std::shared_ptr<const void>> keep;

                if (factsOf(vm, entry->reads.names, keep) != entry->facts)
                    continue;

                Value function = remake(vm, *entry, moduleName);

                if (function == 0x0)
                    continue;

                if (not compiling.empty())
                    compiling.back().add(entry->reads);

                ++takenCount;

                LOG_MILE(DOM, "Module '" << moduleName << "' taken from the cache");

                return function;
            }

            return 0x0;
        }

        void begin(const std::string& fullPath, const std::string& source, const std::queue<Token>& tokens)
        {
            Reads reads;
            reads.sources.emplace_back(fullPath, source);

            // The passes look a name up when it is called (`name(`): the names called, then. A
            // method's name counts too, which only asks more of the program than needed
            auto copy = tokens;
            const Token* last = nullptr;
            Token previous;

            while (not copy.empty())
            {
                const Token& token = copy.front();

                if (token.type == TokenType::PENTER and last)
                    reads.names.insert(last->text);

                if (token.type != TokenType::EOL)
                {
                    previous = token;
                    last = previous.type == TokenType::EXPRESSION ? &previous : nullptr;
                }

                copy.pop();
            }

            compiling.push_back(std::move(reads));
        }

        Compiled end(VM* vm, Value compiled, const std::vector<Value>& functions, const std::string& moduleName)
        {
            Reads reads = std::move(compiling.back());
            compiling.pop_back();

            // A module that failed fails the modules around it, and they keep nothing
            if (compiled == 0x0)
            {
                if (not compiling.empty())
                    compiling.back().keepable = false;

                return {};
            }

            ++compiledCount;

            // The bytecode passes, as VM::interpret would run them after the whole compile: a pass
            // sees one chunk at a time. A module made from bytecode inside it went through them
            for (auto value : functions)
            {
                ObjFunction* function = vm->asFunction(value);

                if (function->precompiled)
                    continue;

                if (vm->enableOptimizations and not function->chunk.code.empty())
                    vm->passManager.runAllPasses(vm, function->chunk);

                function->precompiled = true;
            }

            for (const auto& native : vm->asFunction(compiled)->chunk.importedModules)
                reads.addNative(native);

            auto entry = std::make_shared<Entry>();
            std::ostringstream out;

            if (not ChunkSerializer::serialize(vm->asFunction(compiled)->chunk, out, vm))
            {
                // Something in it cannot be written: the module is the compiler's own functions
                LOG_INFO(DOM, "Module '" << moduleName << "' cannot be serialized, not kept");

                reads.keepable = false;

                if (not compiling.empty())
                    compiling.back().add(reads);

                return {compiled, false};
            }

            entry->bytes = out.str();
            entry->setup = setupOf(vm);
            entry->facts = factsOf(vm, reads.names, entry->keep);
            entry->reads = reads;

            Value function = remake(vm, *entry, moduleName);

            if (function == 0x0)
            {
                reads.keepable = false;

                if (not compiling.empty())
                    compiling.back().add(reads);

                return {compiled, false};
            }

            if (reads.keepable)
                store(reads.sources.front().first, entry);

            if (not compiling.empty())
                compiling.back().add(reads);

            return {function, true};
        }

        void readBytecode()
        {
            for (auto& reads : compiling)
                reads.keepable = false;
        }

        void foundNoFile(const std::string& fullPath)
        {
            if (not compiling.empty())
                compiling.back().absent.push_back(fullPath);
        }

        void clear()
        {
            std::lock_guard<std::mutex> lock(mutex);
            entries.clear();

            compiledCount = 0;
            takenCount = 0;
        }

        Counts counts()
        {
            return {compiledCount.load(), takenCount.load()};
        }
    }
}
