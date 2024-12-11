#pragma once

#include "ECS/entitysystem.h"

#include "interpreter.h"

#include <map>
#include <unordered_map>
#include <functional>

namespace pg
{
    class SysModule
    {
    friend class PgInterpreter;
    public:
        virtual ~SysModule() { }

    protected:
        SysModule() {};

        template <typename Functional>
        void addSystemFunction(const std::string& name)
        {
            auto func = [](VisitorInterpreter *visitor, const std::string& sysName) -> std::shared_ptr<Valuable> {
                std::queue<ExprPtr> emptyQueue;
                Token token;
                token.text = sysName;

                auto function = std::make_shared<Functional>(visitor->globalContext, sysName, token, visitor, emptyQueue, nullptr);
                // Todo check staticly if type Functional as a setUp function using SFINAE
                function->setUp();

                return function;
            };

            sysFunctionTable.emplace(name, func);
        }

        template <typename Functional, typename... Args>
        void addSystemFunction(const std::string& name, Args... args)
        {
            auto func = [args...](VisitorInterpreter *visitor, const std::string& sysName) -> std::shared_ptr<Valuable> {
                std::queue<ExprPtr> emptyQueue;
                Token token;
                token.text = sysName;

                auto function = std::make_shared<Functional>(visitor->globalContext, sysName, token, visitor, emptyQueue, nullptr);
                function->setUp(args...);

                return function;
            };

            sysFunctionTable.emplace(name, func);
        }

        template<typename Value>
        void addSystemVar(const std::string& name, const Value& valuable)
        {
            auto val = [valuable](VisitorInterpreter*, const std::string&) -> std::shared_ptr<Valuable> {
                return makeVar(valuable);
            };

            sysFunctionTable.emplace(name, val);
        }

    private:
        std::map<std::string, std::function<std::shared_ptr<Valuable>(VisitorInterpreter *visitor, const std::string& sysName)>> sysFunctionTable;
    };

    struct CustomSysFunctions
    {
        CustomSysFunctions() {}
        CustomSysFunctions(const CustomSysFunctions& other) : sysFunctionTable(other.sysFunctionTable) {}

        CustomSysFunctions& operator=(const CustomSysFunctions& other)
        {
            sysFunctionTable = other.sysFunctionTable;

            return *this;
        }

        template <typename Functional>
        void addSystemFunction(const std::string& name)
        {
            sysFunctionTable.emplace(name, [](Interpreter *interpreter, const std::string& sysName){ interpreter->defineSystemFunction<Functional>(sysName); });
        }

        template <typename Functional, typename... Args>
        void addSystemFunction(const std::string& name, const Args&... args)
        {
            sysFunctionTable.emplace(name, [args...](Interpreter *interpreter, const std::string& sysName){ interpreter->defineSystemFunction<Functional>(sysName, args...); });
        }

        std::map<std::string, std::function<void(Interpreter*, const std::string&)>> sysFunctionTable;
    };

    struct ExecuteFileScriptEvent
    {
        ExecuteFileScriptEvent(const std::string& filename) : filename(filename) {}
        ExecuteFileScriptEvent(const std::string& filename, const CustomSysFunctions& functions) : filename(filename), functions(functions) {}
        ExecuteFileScriptEvent(const ExecuteFileScriptEvent& other) : filename(other.filename), functions(other.functions) {}

        ExecuteFileScriptEvent& operator=(const ExecuteFileScriptEvent& other)
        {
            filename = other.filename;
            functions = other.functions;

            return *this;
        }

        std::string filename;

        CustomSysFunctions functions;
    };

    struct ExecuteCodeScriptEvent
    {
        ExecuteCodeScriptEvent(const std::string& data) : data(data) {}
        ExecuteCodeScriptEvent(const std::string& data, const CustomSysFunctions& functions) : data(data), functions(functions) {}
        ExecuteCodeScriptEvent(const ExecuteCodeScriptEvent& other) : data(other.data), functions(other.functions) {}

        ExecuteCodeScriptEvent& operator=(const ExecuteCodeScriptEvent& other)
        {
            data = other.data;
            functions = other.functions;

            return *this;
        }
    
        std::string data;

        CustomSysFunctions functions;
    };

    class PgInterpreter : public System<Listener<ExecuteFileScriptEvent>, Listener<ExecuteCodeScriptEvent>, NamedSystem>
    {
    friend class Interpreter;
    friend class VisitorInterpreter;
    public:
        virtual ~PgInterpreter() { for(auto interpreter : sysInterpreters) delete interpreter; }

        ScriptImport interpretFromText(const std::string& scriptText, const CustomSysFunctions& functions = CustomSysFunctions());
        ScriptImport interpretFromFile(const std::string& scriptFile, const CustomSysFunctions& functions = CustomSysFunctions());
        ScriptImport interpretFromFile(const TextFile& scriptFile, const CustomSysFunctions& functions = CustomSysFunctions());

        virtual std::string getSystemName() const override { return "Pg Interpreter"; }

        virtual void onEvent(const ExecuteFileScriptEvent& event) override
        {
            scriptQueue.emplace(ScriptCallType::FromFile, event.filename, event.functions);
        }

        virtual void onEvent(const ExecuteCodeScriptEvent& event) override
        {
            scriptQueue.emplace(ScriptCallType::FromText, event.data, event.functions);
        }

        virtual void execute() override
        {
            std::queue<ScriptCall> queue;
            queue.swap(scriptQueue);
            
            while (not queue.empty())
            {
                auto script = queue.front();

                switch (script.type)
                {
                    case ScriptCallType::FromText:
                        interpretFromText(script.data, script.functions);
                        break;
                    
                    case ScriptCallType::FromFile:
                        interpretFromFile(script.data, script.functions);
                        break;
                }

                queue.pop();
            }

        }

    public:
        template <typename Functional>
        void addSystemFunction(const std::string& name)
        {
            sysFunctionTable.emplace(name, [](Interpreter *interpreter, const std::string& sysName){ interpreter->defineSystemFunction<Functional>(sysName); });
        }

        template <typename Functional, typename... Args>
        void addSystemFunction(const std::string& name, const Args&... args)
        {
            sysFunctionTable.emplace(name, [args...](Interpreter *interpreter, const std::string& sysName){ interpreter->defineSystemFunction<Functional>(sysName, args...); });
        }

        void addSystemModule(const std::string& name, const SysModule& module)
        {
            sysModuleTable.emplace(name, module.sysFunctionTable);
        }

    protected:
        std::map<std::string, std::function<void(Interpreter*, const std::string&)>> sysFunctionTable;
        std::map<std::string, std::map<std::string, std::function<std::shared_ptr<Valuable>(VisitorInterpreter *visitor, const std::string& sysName)>>> sysModuleTable;

    private:
        enum class ScriptCallType : uint8_t { FromText = 0, FromFile = 1 };

        struct ScriptCall
        {
            ScriptCall(const ScriptCallType& type, const std::string& data, const CustomSysFunctions& functions) : type(type), data(data), functions(functions) { }
            ScriptCall(const ScriptCall& rhs) : type(rhs.type), data(rhs.data), functions(rhs.functions) { }
            ~ScriptCall() { }

            ScriptCallType type = ScriptCallType::FromText;
            std::string data;
            CustomSysFunctions functions;
        };

    private:
        inline bool isSysModule(const std::string& name) const { return sysModuleTable.find(name) != sysModuleTable.end(); }

        ScriptImport getAst(const std::string& script, const std::string& filePath = "");

        ScriptImport generateAST(const std::string& data);
        ScriptImport generateASTFromFile(const std::string& filename);
        ScriptImport generateASTFromFile(const TextFile& file);

        ScriptImport _interpret(const ScriptImport& script, const CustomSysFunctions& function);

        // Todo store the absolute path of the custom made script file
        // to avoid using the AST of another file when trying to import a script
        std::unordered_map<std::string, ScriptImport> importedScripts;

        // Hold all the scripts defining some system in the ecs to not invalidate execute functions and such
        std::vector<Interpreter*> sysInterpreters;

        std::queue<ScriptCall> scriptQueue;
    };

    void archiveToListHelper(const Function *caller, SerializedInfoHolder& parent, size_t indentLevel, std::shared_ptr<ClassInstance> currentList, const std::string& parentName = "");

    template <typename Type>
    std::shared_ptr<ClassInstance> serializeToInterpreter(const Function *caller, const Type& arg)
    {
        InspectorArchive archive;

        serialize(archive, arg);
                                
        auto list = makeList(caller, {});

        if (archive.mainNode.children.size() > 0)
        {
            archiveToListHelper(caller, archive.mainNode.children[0], 0, list, archive.mainNode.children[0].className);
        }

        return list;
    }
}