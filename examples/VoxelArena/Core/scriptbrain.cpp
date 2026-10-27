#include "stdafx.h"

#include "scriptbrain.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>

#include "Compiler/vm.h"
#include "Helpers/algorithmmodule.h"
#include "Helpers/mathmodule.h"
#include "Helpers/randommodule.h"
#include "Helpers/stringmodule.h"
#include "logger.h"

namespace fs = std::filesystem;

using namespace pg;

namespace arena
{
    namespace
    {
        constexpr const char* DOM = "ScriptBrain";

        // The VM reports compile and runtime errors through the logger only. While a script
        // runs on this thread, its errors are collected here so the game can show them.
        thread_local std::vector<std::string>* captured = nullptr;

        class CaptureSink : public Logger::LogSink
        {
        public:
            void processLog(const Logger::Info& log) override
            {
                if (captured and log.level >= Logger::InfoLevel::error)
                    captured->push_back(log.message);
            }
        };

        void ensureSink()
        {
            static std::once_flag once;
            std::call_once(once, [] { Logger::registerSink<CaptureSink>(); });
        }

        struct Capture
        {
            std::vector<std::string> lines;
            std::vector<std::string>* previous;

            Capture() : previous(captured) { ensureSink(); captured = &lines; }
            ~Capture() { captured = previous; }

            std::string text() const
            {
                std::string out;
                for (const auto& l : lines)
                    out += (out.empty() ? "" : " | ") + l;
                return out;
            }
        };

        /** The only modules a mod script gets. */
        void sandbox(VM& vm)
        {
            vm.addNativeModule("math", MathModule{});
            vm.addNativeModule("algorithm", AlgorithmModule{});
            vm.addNativeModule("string", StringModule{});
            vm.addNativeModule("random", RandomModule{});

            auto print = [](VM* vm, int argCount, Value* args) -> Value {
                std::string line;

                for (int i = 0; i < argCount; ++i)
                {
                    if (IS_STRING(args[i]))
                        line += vm->asString(args[i]);
                    else if (IS_INT(args[i]))
                        line += std::to_string(AS_INT(args[i]));
                    else if (IS_DOUBLE(args[i]))
                        line += std::to_string(AS_DOUBLE(args[i]));
                    else if (IS_BOOL(args[i]))
                        line += AS_BOOL(args[i]) ? "true" : "false";
                    else
                        line += "<value>";

                    if (i + 1 < argCount)
                        line += " ";
                }

                LOG_INFO("script", line);
                return makeNoneValue();
            };

            vm.registerNative("print", print);
        }

        Value toValue(VM& vm, const Json& j)
        {
            switch (j.type)
            {
                case Json::Type::Bool: return makeBoolValue(j.boolean);
                case Json::Type::Number: return makeDoubleValue(j.number);
                case Json::Type::String: return vm.createString(j.string);
                case Json::Type::Array:
                {
                    Value list = vm.createVector();
                    for (const auto& e : j.array)
                        vm.asVector(list)->fields.push_back(toValue(vm, e));
                    return list;
                }
                case Json::Type::Object:
                {
                    Value table = vm.createTable();
                    for (const auto& [k, v] : j.object)
                        vm.asInstance(table)->setField(k, toValue(vm, v));
                    return table;
                }
                case Json::Type::Null:
                default:
                    return makeNoneValue();
            }
        }

        /** The first "line N" in a VM message, so the error can point at file:N. */
        int firstLine(const std::string& text)
        {
            const auto at = text.find("line ");

            if (at == std::string::npos)
                return 0;

            return std::atoi(text.c_str() + at + 5);
        }

        double number(Value v, double fallback)
        {
            if (IS_DOUBLE(v))
                return AS_DOUBLE(v);
            if (IS_INT(v))
                return static_cast<double>(AS_INT(v));
            return fallback;
        }
    }

    struct ScriptBrain::Script
    {
        std::string id;
        std::string path;
        std::unique_ptr<VM> vm;
        ObjFunction* fn = nullptr;
        Value think = 0;
        bool ok = false;
        std::string error;

        // One `me` table per enemy, kept between calls
        std::map<int, Value> tables;
        // Last thing written to each string field, to skip re-creating identical strings
        std::map<int, std::string> lastTargetAction;

        ~Script()
        {
            if (vm)
            {
                for (auto& [id, t] : tables)
                    vm->releaseValue(t);

                if (fn)
                    vm->cleanupFunction(fn);
            }
        }
    };

    ScriptBrain::ScriptBrain()
    {
        cacheDir = (fs::temp_directory_path() / "voxelarena-scripts").string();
    }

    ScriptBrain::~ScriptBrain() = default;

    void ScriptBrain::load(const Content& content, ContentReport& report)
    {
        scripts.clear();

        std::error_code ec;
        fs::create_directories(cacheDir, ec);

        for (const auto& [id, path] : content.scripts)
        {
            auto script = std::make_unique<Script>();
            script->id = id;
            script->path = path;

            // Compile to bytecode with a throwaway VM, then run it on the one the enemies use
            std::vector<char> bytes;
            {
                Capture capture;
                auto compiler = std::make_unique<VM>();
                sandbox(*compiler);

                std::string name = id;
                std::replace(name.begin(), name.end(), ':', '_');
                const std::string out = (fs::path(cacheDir) / (name + ".pgc")).string();

                const auto result = compiler->interpretFromFile(path, true, out);

                if (result != InterpretResult::OK)
                {
                    report.error(path, firstLine(capture.text()), "does not compile: " + (capture.text().empty() ? std::string("see the log") : capture.text()));
                    scripts[id] = std::move(script);
                    continue;
                }

                std::ifstream in(out, std::ios::binary);
                bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
                in.close();
                fs::remove(out, ec);
            }

            Capture capture;
            script->vm = std::make_unique<VM>();
            sandbox(*script->vm);
            script->fn = script->vm->prepareCachedFunction(bytes, path);

            if (not script->fn)
            {
                report.error(path, 0, "its bytecode could not be prepared");
                scripts[id] = std::move(script);
                continue;
            }

            // The top level defines the functions (and runs any setup the script has)
            if (script->vm->runPreparedFunction(script->fn, 0, path) != InterpretResult::OK)
            {
                script->vm->frameCount = 0;
                report.error(path, 0, "failed while starting: " + capture.text());
                scripts[id] = std::move(script);
                continue;
            }

            script->think = script->vm->findGlobal("think");

            if (not IS_CLOSURE(script->think) or script->vm->asClosure(script->think)->function->arity != 1)
            {
                report.error(path, 0, "must define fun think(me)");
                scripts[id] = std::move(script);
                continue;
            }

            // Keep it: the global could be overwritten by the script itself
            script->vm->retainValue(script->think);
            script->ok = true;

            report.info.push_back("script " + id + " ready");
            scripts[id] = std::move(script);
        }
    }

    void ScriptBrain::forget(const std::vector<int>& liveIds)
    {
        for (auto& [id, s] : scripts)
        {
            if (not s->vm)
                continue;

            for (auto it = s->tables.begin(); it != s->tables.end();)
            {
                if (std::find(liveIds.begin(), liveIds.end(), it->first) == liveIds.end())
                {
                    s->vm->releaseValue(it->second);
                    s->lastTargetAction.erase(it->first);
                    it = s->tables.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    bool ScriptBrain::think(const std::string& scriptId, const BrainView& view, BrainOrder& order, std::string& error)
    {
        auto it = scripts.find(scriptId);

        if (it == scripts.end())
        {
            error = "no script " + scriptId;
            return false;
        }

        Script& s = *it->second;

        if (not s.ok)
        {
            error = s.path + ": not loaded";
            return false;
        }

        const auto start = std::chrono::steady_clock::now();
        VM& vm = *s.vm;
        const Fighter& self = *view.self;
        const Fighter& target = *view.target;

        auto tableIt = s.tables.find(self.id);

        if (tableIt == s.tables.end())
        {
            Value t = vm.createTable();
            ObjInstance* me = vm.asInstance(t);
            me->setField("id", makeIntValue(self.id));
            me->setField("type", vm.createString(self.type));
            me->setField("params", view.def ? toValue(vm, view.def->params) : vm.createTable());
            tableIt = s.tables.emplace(self.id, t).first;
        }

        Value table = tableIt->second;
        ObjInstance* me = vm.asInstance(table);

        // What the enemy sees
        me->setField("hp", makeDoubleValue(self.health));
        me->setField("maxHp", makeDoubleValue(self.maxHealth));
        me->setField("poise", makeDoubleValue(self.poise));
        me->setField("distance", makeDoubleValue(view.distance));
        me->setField("angle", makeDoubleValue(view.angle));
        me->setField("time", makeDoubleValue(view.time));
        me->setField("dt", makeDoubleValue(STEP_SECONDS));
        me->setField("sinceAttack", makeDoubleValue(self.sinceAttack));
        me->setField("sinceHit", makeDoubleValue(self.sinceHit));
        me->setField("targetHp", makeDoubleValue(target.health));
        me->setField("targetAttacking", makeBoolValue(target.action == Action::Attack));
        me->setField("targetDodging", makeBoolValue(target.action == Action::Dodge));
        me->setField("targetStaggered", makeBoolValue(target.action == Action::Stagger));

        const std::string action = actionName(target.action);

        if (s.lastTargetAction[self.id] != action)
        {
            me->setField("targetAction", vm.createString(action), &vm, true);
            s.lastTargetAction[self.id] = action;
        }

        // The answer starts from "do nothing" every call
        me->setField("move", vm.createString("hold"), &vm, true);
        me->setField("attack", vm.createString(""), &vm, true);
        me->setField("strafe", makeDoubleValue(1.0));
        me->setField("speed", makeDoubleValue(1.0));
        me->setField("face", makeBoolValue(true));

        Capture capture;
        InterpretResult result = InterpretResult::RUNTIME_ERROR;

        try
        {
            vm.push(vm.retainValue(s.think));
            vm.push(vm.retainValue(table));

            if (vm.call(vm.asClosure(s.think), 1))
                result = vm.run();
        }
        catch (const std::exception& e)
        {
            capture.lines.push_back(e.what());
        }

        if (result != InterpretResult::OK)
        {
            vm.resetStack();
            vm.frameCount = 0;
        }

        stats.calls++;
        stats.seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

        if (result != InterpretResult::OK)
        {
            error = s.path + ": " + (capture.text().empty() ? std::string("runtime error") : capture.text());
            return false;
        }

        // The answer
        const Value move = me->getField("move");
        const Value attack = me->getField("attack");

        order.move = IS_STRING(move) ? vm.asString(move) : "hold";
        order.attack = IS_STRING(attack) ? vm.asString(attack) : "";
        order.strafe = static_cast<float>(number(me->getField("strafe"), 1.0));
        order.speed = static_cast<float>(number(me->getField("speed"), 1.0));

        const Value face = me->getField("face");
        order.face = IS_BOOL(face) ? AS_BOOL(face) : true;

        return true;
    }
}
