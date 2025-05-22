#include "stdafx.h"

#include "pginterpreter.h"

#include <filesystem>
namespace fs = std::filesystem;

#include "lexer.h"
#include "parser.h"
#include "resolver.h"

#include "logger.h"

// Todo to remove
#include "systemfunction.h"

namespace pg
{
    namespace
    {
        const char * const DOM = "PG Interpreter";

        bool stringEndWith(const std::string& fullString, const std::string& ending)
        {
            if (fullString.length() >= ending.length())
                return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
            else
                return false;
        }

    }

    ScriptImport PgInterpreter::interpretFromText(const std::string& scriptText, const CustomSysFunctions& functions)
    {
        auto ast = generateAST(scriptText);

        return _interpret(ast, functions);
        // _interpret(scriptText, tokens);
    }

    ScriptImport PgInterpreter::interpretFromFile(const std::string& scriptFile, const CustomSysFunctions& functions)
    {
        auto ast = generateASTFromFile(scriptFile);

// Todo
#ifdef DEBUG
        auto queue = ast.ast;

        while (queue.size() > 0)
        {
            auto stmt = queue.front();

            // if(stmt)
            //     std::cout << stmt->prettyPrint() << std::endl;

            queue.pop();
        }
#endif

        return _interpret(ast, functions);

        // _interpret(scriptFile, tokens);
    }

    ScriptImport PgInterpreter::interpretFromFile(const TextFile& scriptFile, const CustomSysFunctions& functions)
    {
        auto ast = generateASTFromFile(scriptFile);

        auto queue = ast.ast;

// Todo
#ifdef DEBUG
        while(queue.size() > 0)
        {
            auto stmt = queue.front();

            // if(stmt)
            //     std::cout << stmt->prettyPrint() << std::endl;

            queue.pop();
        }
#endif

        return _interpret(ast, functions);
    }

    ScriptImport PgInterpreter::getAst(const std::string& scriptName, const std::string& filePath)
    {
        // Create path to the script
        std::string fileToOpen = filePath + scriptName;

        // Append the extension if not already present
        if (not stringEndWith(scriptName, ".pg"))
            fileToOpen += ".pg";

        fs::path p {fileToOpen};

        // Check if the script file exist first
        if (not fs::exists(p))
        {
            LOG_MILE(DOM, "Couldn't load module '" << fileToOpen << "' : File doesn't exist, but it may be a system module.");
            return ScriptImport{};
        }

        // Check if an AST is available for the script
        // Checking with the relative path to insure unicity
        const auto& it = importedScripts.find(p.relative_path().string());

        if (it != importedScripts.end())
            return it->second;

        // Ast was not already creating, proceed to create it
        auto ast = generateASTFromFile(fileToOpen);

        return ast;
    }

    ScriptImport PgInterpreter::_interpret(const ScriptImport& script, const CustomSysFunctions& functions)
    {
        // Todo store the interpreter if a system is defined in it
        // Else just destroy it
        Interpreter *interpreter = new Interpreter(script, this);

        for (auto& it : sysFunctionTable)
        {
            it.second(interpreter, it.first);
        }

        for (const auto& it : functions.sysFunctionTable)
        {
            LOG_INFO(DOM, "Adding sys function: " << it.first);
            it.second(interpreter, it.first);
        }

        auto env = interpreter->interpret();

        if (interpreter->hasError())
        {
            LOG_ERROR(DOM, "Interpreter error");
            return script;
        }

        if (importedScripts.find(script.name) == importedScripts.end())
            importedScripts[script.name] = script;

        if (interpreter->hasEcsSys())
            sysInterpreters.push_back(interpreter);
        else
            delete interpreter;

        // Todo remove this save of the env as the interpreter is created locally here and so the env become useless once we leave this function
        importedScripts[script.name].env = env;

        return importedScripts[script.name];
    }

    ScriptImport PgInterpreter::generateAST(const std::string& data)
    {
        ScriptImport script;
        Lexer lexer;

        try
        {
            lexer.readFromText(data);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(DOM, e.what());
            return script;
        }

        auto tokens = lexer.getTokens();

        Parser parser(tokens);

        auto ast = parser.parse();

        Resolver resolver(ast);

        auto symbolTable = resolver.resolve();

        if (parser.hasError())
        {
            LOG_ERROR(DOM, "Parser error");
            return script;
        }

        if (resolver.hasError())
        {
            LOG_ERROR(DOM, "Resolver error");
            return script;
        }

        script.ast = ast;
        script.symbols = symbolTable;

        return script;
    }

    ScriptImport PgInterpreter::generateASTFromFile(const std::string& filename)
    {
        auto file = UniversalFileAccessor::openTextFile(filename);

        return generateASTFromFile(file);
    }

    ScriptImport PgInterpreter::generateASTFromFile(const TextFile& file)
    {
        auto name = UniversalFileAccessor::getRelativePath(file);

        auto ast = generateAST(file.data);
        ast.name = name;

        importedScripts[name] = ast;

        return ast;
    }

    void addNewAttribute(const Function *caller, const std::string& text, const std::string& type, std::string& value, std::shared_ptr<ClassInstance> currentList)
    {
        if (type == "int")
        {
            int v = 0;
            std::stringstream sstream(value);
            sstream >> v;

            addToList(currentList, caller->getToken(), {text, v});
        }
        else if (type == "bool")
        {
            bool v = false;

            if (value == "true")
                v = true;

            addToList(currentList, caller->getToken(), {text, v});
        }
        // Todo this is casted to a size_t (Should not be !)
        else if (type == "unsigned int")
        {
            unsigned int v = 0;
            std::stringstream sstream(value);
            sstream >> v;

            addToList(currentList, caller->getToken(), {text, static_cast<size_t>(v)});
        }
        else if (type == "float")
        {
            float v = 0;
            std::stringstream sstream(value);
            sstream >> v;

            addToList(currentList, caller->getToken(), {text, v});
        }
        // Todo this is casted to a float (Should not be !)
        else if (type == "double")
        {
            double v = 0;
            std::stringstream sstream(value);
            sstream >> v;

            addToList(currentList, caller->getToken(), {text, static_cast<float>(v)});
        }
        else if (type == "size_t")
        {
            size_t v = 0;
            std::stringstream sstream(value);
            sstream >> v;

            addToList(currentList, caller->getToken(), {text, v});
        }
        else if (type == "string")
        {
            addToList(currentList, caller->getToken(), {text, value});
        }
        else
        {
            LOG_ERROR(DOM, "Unsupported type for interpreter serialization: " << type);
        }
    }

    void archiveToListHelper(const Function *caller, SerializedInfoHolder& parent, size_t indentLevel, std::shared_ptr<ClassInstance> currentList, const std::string& parentName)
    {
        if (parentName != "")
        {
            addToList(currentList, caller->getToken(), {"__className", parentName});
        }

        // If no class name then we got an attribute
        if (parent.className == "" and indentLevel > 0)
        {
            addNewAttribute(caller, parent.name, parent.type, parent.value, currentList);
        }

        if (parent.children.size() > 0)
        {
            std::shared_ptr<ClassInstance> childList = indentLevel > 0 ? makeList(caller, {}) : currentList;

            for (auto& child : parent.children)
            {
                archiveToListHelper(caller, child, indentLevel + 1, childList, parent.className);
            }

            auto className = parent.className == "" ? "__children" : parent.name == "" ? parent.className : parent.name;

            if (indentLevel > 0)
                addToList(currentList, caller->getToken(), {className, childList});
        }
    }

    void deserializeToHelper(UnserializedObject& holder, std::vector<ClassInstance::Field>& fields, const std::string& className)
    {
        auto it = std::find(fields.begin(), fields.end(), "__className");

        // Got a class name, so we can put this name in the unserialized object and parse it correctly as a class
        if (it != fields.end())
        {
            UnserializedObject klass(className, it->value->getElement().toString(), std::string(""));
            fields.erase(it);

            deserializeToHelper(klass, fields);

            holder.children.push_back(klass);
        }
        else
        {
            // Parse all the field of the interpreted struct
            for (const auto& field : fields)
            {
                if (field.value->getType() == "Variable")
                {
                    // If it is a variable we can convert it from element type to basic type (it is an attribute)
                    const auto& element = field.value->getElement();

                    std::string str;

                    if (strcmp(ARCHIVEVERSION, "1.0.0") == 0)
                        str = ATTRIBUTECONST + " " + element.getTypeString() + " {" + element.toString() + "}";

                    UnserializedObject attribute(str, field.key, false);

                    holder.children.push_back(attribute);
                }
                else if (field.value->getType() == "ClassInstance")
                {
                    // If it is a class instance, it is a complexe type and we recursively parse it to get all the attributes
                    auto nextClass = std::static_pointer_cast<ClassInstance>(field.value);
                    auto nextFields = nextClass->getFields();

                    auto it = std::find(nextFields.begin(), nextFields.end(), "__className");

                    // If no class name is provided, we insert "InterpretedStruct" as the class name to avoid parsing and struct hierachy missmatch
                    if (it == nextFields.end())
                    {
                        nextFields.emplace_back("__className", makeVar("InterpretedStruct"));
                    }

                    deserializeToHelper(holder, nextFields, field.key);
                }
                // Todo
                // else if (field.value->getType() == "Function")
                else
                {
                    LOG_ERROR(DOM, "Field [" << field.key << "] type is not available for deserialization (" << field.value->getType() << ")");
                }
            }
        }
    }
}