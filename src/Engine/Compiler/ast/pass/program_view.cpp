#include "stdafx.h"

#include "program_view.h"

#include "Compiler/vm.h"
#include "Files/filemanager.h"
#include "Interpreter/lexer.h"
#include "Interpreter/parser.h"

#include <mutex>
#include <unordered_map>

namespace pg
{
    namespace ast
    {
        namespace
        {
            template <typename T>
            std::vector<T> listOf(std::queue<T> q)
            {
                std::vector<T> out;

                while (not q.empty())
                {
                    out.push_back(q.front());
                    q.pop();
                }

                return out;
            }

            // The names declared in the scopes around a statement, innermost last: none at a file's
            // top level. A name declared in one is a local there (or an upvalue of a closure): writing
            // it writes no global, so it marks no function of the program
            using Scopes = std::vector<std::set<std::string>>;

            bool isLocal(const Scopes& scopes, const std::string& name)
            {
                for (const auto& scope : scopes)
                {
                    if (scope.count(name))
                        return true;
                }

                return false;
            }

            // A name written: a global unless a scope around declares it
            void written(const std::string& name, const Scopes& scopes, ProgramView& p)
            {
                if (not isLocal(scopes, name))
                    p.poisoned.insert(name);
            }

            void poisonExpr(const ExprPtr& e, ProgramView& p, Scopes& scopes);
            void poisonStmt(const StatementPtr& s, ProgramView& p, Scopes& scopes);

            // A function's body in a scope of its own, its parameters declared there
            void poisonFunction(const std::queue<ExprPtr>& parameters, const StatementPtr& body, ProgramView& p, Scopes& scopes)
            {
                scopes.emplace_back();

                for (const auto& param : listOf(parameters))
                {
                    if (param and param->kind() == AstKind::Var)
                        scopes.back().insert(std::static_pointer_cast<Var>(param)->name.text);
                }

                poisonStmt(body, p, scopes);
                scopes.pop_back();
            }

            void poisonExpr(const ExprPtr& e, ProgramView& p, Scopes& scopes)
            {
                if (not e)
                    return;

                const AstKind type = e->kind();

                if (type == AstKind::Assign)
                {
                    auto a = std::static_pointer_cast<Assign>(e);
                    written(a->name.text, scopes, p);
                    poisonExpr(a->expr, p, scopes);
                }
                else if (type == AstKind::PreFixExpression)
                    written(std::static_pointer_cast<PreFixExpression>(e)->name.text, scopes, p);
                else if (type == AstKind::PostFixExpression)
                    written(std::static_pointer_cast<PostFixExpression>(e)->name.text, scopes, p);
                else if (type == AstKind::BinaryExpression)
                {
                    auto b = std::static_pointer_cast<BinaryExpression>(e);
                    poisonExpr(b->leftExpr, p, scopes);
                    poisonExpr(b->rightExpr, p, scopes);
                }
                else if (type == AstKind::LogicExpression)
                {
                    auto l = std::static_pointer_cast<LogicExpression>(e);
                    poisonExpr(l->leftExpr, p, scopes);
                    poisonExpr(l->rightExpr, p, scopes);
                }
                else if (type == AstKind::UnaryExpression)
                    poisonExpr(std::static_pointer_cast<UnaryExpression>(e)->expr, p, scopes);
                else if (type == AstKind::CompoundAtom)
                    poisonExpr(std::static_pointer_cast<CompoundAtom>(e)->expr, p, scopes);
                else if (type == AstKind::CallExpression)
                {
                    auto c = std::static_pointer_cast<CallExpression>(e);
                    poisonExpr(c->caller, p, scopes);

                    for (const auto& a : listOf(c->args))
                        poisonExpr(a, p, scopes);
                }
                else if (type == AstKind::Get)
                    poisonExpr(std::static_pointer_cast<Get>(e)->object, p, scopes);
                else if (type == AstKind::Set)
                {
                    auto s = std::static_pointer_cast<Set>(e);
                    poisonExpr(s->object, p, scopes);
                    poisonExpr(s->value, p, scopes);
                }
                else if (type == AstKind::IndexGet)
                {
                    auto g = std::static_pointer_cast<IndexGet>(e);
                    poisonExpr(g->object, p, scopes);
                    poisonExpr(g->index, p, scopes);
                }
                else if (type == AstKind::IndexSet)
                {
                    auto g = std::static_pointer_cast<IndexSet>(e);
                    poisonExpr(g->object, p, scopes);
                    poisonExpr(g->index, p, scopes);
                    poisonExpr(g->value, p, scopes);
                }
                else if (type == AstKind::List)
                {
                    for (const auto& entry : listOf(std::static_pointer_cast<List>(e)->entries))
                    {
                        poisonExpr(entry.key, p, scopes);
                        poisonExpr(entry.value, p, scopes);
                    }
                }
                else if (type == AstKind::AnonymousFunction)
                {
                    auto f = std::static_pointer_cast<AnonymousFunction>(e);
                    poisonFunction(f->parameters, f->body, p, scopes);
                }
            }

            void poisonStmt(const StatementPtr& s, ProgramView& p, Scopes& scopes)
            {
                if (not s)
                    return;

                const AstKind type = s->kind();

                if (type == AstKind::ExpressionStatement)
                    poisonExpr(std::static_pointer_cast<ExpressionStatement>(s)->expr, p, scopes);
                else if (type == AstKind::VariableStatement)
                {
                    auto v = std::static_pointer_cast<VariableStatement>(s);

                    // Its initializer first: `var x = x + 1` reads the x around it
                    poisonExpr(v->expr, p, scopes);

                    // At a file's top level it is a global of the program; anywhere else a local
                    if (scopes.empty())
                        p.poisoned.insert(v->name.text);
                    else
                        scopes.back().insert(v->name.text);
                }
                else if (type == AstKind::FunctionStatement)
                {
                    auto f = std::static_pointer_cast<FunctionStatement>(s);

                    // A function declared inside another is a local of it
                    if (not scopes.empty())
                        scopes.back().insert(f->name.text);

                    poisonFunction(f->parameters, f->body, p, scopes);
                }
                else if (type == AstKind::ClassStatement)
                {
                    auto c = std::static_pointer_cast<ClassStatement>(s);

                    if (scopes.empty())
                        p.poisoned.insert(c->name.text);
                    else
                        scopes.back().insert(c->name.text);

                    for (const auto& m : listOf(c->methods))
                        poisonStmt(m, p, scopes);
                }
                else if (type == AstKind::BlockStatement)
                {
                    scopes.emplace_back();

                    for (const auto& b : listOf(std::static_pointer_cast<BlockStatement>(s)->statements))
                        poisonStmt(b, p, scopes);

                    scopes.pop_back();
                }
                else if (type == AstKind::IfStatement)
                {
                    auto i = std::static_pointer_cast<IfStatement>(s);
                    poisonExpr(i->condition, p, scopes);
                    poisonStmt(i->thenBranch, p, scopes);
                    poisonStmt(i->elseBranch, p, scopes);
                }
                else if (type == AstKind::WhileStatement)
                {
                    auto w = std::static_pointer_cast<WhileStatement>(s);
                    poisonExpr(w->condition, p, scopes);
                    poisonStmt(w->body, p, scopes);
                }
                else if (type == AstKind::ForStatement)
                {
                    // Its initializer's variable is the loop's own
                    auto f = std::static_pointer_cast<ForStatement>(s);
                    scopes.emplace_back();
                    poisonStmt(f->initializer, p, scopes);
                    poisonExpr(f->condition, p, scopes);
                    poisonExpr(f->increment, p, scopes);
                    poisonStmt(f->body, p, scopes);
                    scopes.pop_back();
                }
                else if (type == AstKind::ForInStatement)
                {
                    auto f = std::static_pointer_cast<ForInStatement>(s);
                    poisonExpr(f->iterable, p, scopes);
                    scopes.emplace_back();
                    scopes.back().insert(f->varName.text);
                    poisonStmt(f->body, p, scopes);
                    scopes.pop_back();
                }
                else if (type == AstKind::ReturnStatement)
                    poisonExpr(std::static_pointer_cast<ReturnStatement>(s)->value, p, scopes);
                else if (type == AstKind::DPrintStatement)
                    poisonExpr(std::static_pointer_cast<DPrintStatement>(s)->expr, p, scopes);
            }

            // A statement of a file's top level, where a variable is a global
            void poisonStmt(const StatementPtr& s, ProgramView& p)
            {
                Scopes scopes;
                poisonStmt(s, p, scopes);
            }

            void readFile(const std::string& baseDir, const std::string& path, ProgramView& p);

            // A module's name as an import statement holds it, without its quotes
            std::string importName(const ExprPtr& import)
            {
                std::string name = import->getName();

                if (name.size() >= 2 and name.front() == '"' and name.back() == '"')
                    name = name.substr(1, name.size() - 2);

                return name;
            }

            // What one file adds to a program, read from its source once: every compile of a
            // script reads the modules it imports, and the same modules come again and again. The
            // functions are only read by the passes, never changed, so they can be shared
            struct FileSummary
            {
                std::string source;                                         // What it was read from
                bool read = true;                                           // Lexed and parsed
                std::vector<std::shared_ptr<FunctionStatement>> functions;  // Its top-level `fun`s
                std::set<std::string> poisoned;
                std::vector<std::string> imports;                           // In the order it imports them
                std::vector<StatementPtr> statements;                       // Its top level, as parsed (shared, never changed)
            };

            std::shared_ptr<const FileSummary> summarize(std::string source)
            {
                auto summary = std::make_shared<FileSummary>();
                summary->source = std::move(source);

                try
                {
                    Lexer lexer;

                    if (not summary->source.empty())
                        lexer.readFromText(summary->source);

                    Parser parser(lexer.getTokens());
                    auto statements = parser.parse();

                    if (parser.hasError())
                    {
                        summary->read = false;
                        return summary;
                    }

                    ProgramView own;

                    summary->statements = listOf(statements);

                    for (const auto& s : summary->statements)
                    {
                        if (not s)
                            continue;

                        if (s->kind() == AstKind::FunctionStatement)
                            summary->functions.push_back(std::static_pointer_cast<FunctionStatement>(s));
                        else if (s->kind() == AstKind::ImportStatement)
                        {
                            for (const auto& import : listOf(std::static_pointer_cast<ImportStatement>(s)->imports))
                                summary->imports.push_back(importName(import));
                        }

                        poisonStmt(s, own);
                    }

                    summary->poisoned = std::move(own.poisoned);
                }
                catch (const std::exception&)
                {
                    summary->read = false;
                }

                return summary;
            }

            // The file's summary, made again only when its source is not the one it was made from
            std::shared_ptr<const FileSummary> summaryOf(const std::string& fullPath)
            {
                static std::mutex mutex;
                static std::unordered_map<std::string, std::shared_ptr<const FileSummary>> summaries;

                std::string source = UniversalFileAccessor::openTextFile(fullPath).data;

                {
                    std::lock_guard<std::mutex> lock(mutex);

                    const auto it = summaries.find(fullPath);

                    if (it != summaries.end() and it->second->source == source)
                        return it->second;
                }

                auto summary = summarize(std::move(source));

                std::lock_guard<std::mutex> lock(mutex);
                summaries[fullPath] = summary;

                return summary;
            }

            // A script's statements: its top-level functions counted, every name it writes poisoned,
            // and the modules it imports read in turn
            void readStatements(const std::string& baseDir, const std::vector<StatementPtr>& statements, ProgramView& p)
            {
                for (const auto& s : statements)
                {
                    if (not s)
                        continue;

                    if (s->kind() == AstKind::FunctionStatement)
                    {
                        auto f = std::static_pointer_cast<FunctionStatement>(s);
                        ++p.defined[f->name.text];
                        p.functions[f->name.text] = f;
                    }
                    else if (s->kind() == AstKind::ImportStatement)
                    {
                        for (const auto& import : listOf(std::static_pointer_cast<ImportStatement>(s)->imports))
                            readFile(baseDir, importName(import), p);
                    }

                    poisonStmt(s, p);
                }
            }

            // As CParser::parseImportFile finds a module: beside the main script. Its source is read
            // even when a .pgc stands beside it (the compiler takes that, compiled from the same
            // source); with neither, it is a native module and defines no function of the program
            void readFile(const std::string& baseDir, const std::string& name, ProgramView& p)
            {
                std::string fileName = name;

                if (fileName.find(".pg") == std::string::npos)
                    fileName += ".pg";

                const std::string fullPath = baseDir + fileName;

                if (p.visited.count(fullPath))
                    return;

                p.visited.insert(fullPath);

                if (not UniversalFileAccessor::exists(fullPath))
                {
                    // Bytecode only: what it defines cannot be read
                    if (UniversalFileAccessor::exists(fullPath + "c") or UniversalFileAccessor::exists(baseDir + name + ".pgc"))
                        p.complete = false;

                    return;
                }

                const auto summary = summaryOf(fullPath);

                if (not summary->read)
                {
                    p.complete = false;
                    return;
                }

                // As readStatements would add it: counts, sets and an and, so in any order
                for (const auto& f : summary->functions)
                {
                    ++p.defined[f->name.text];
                    p.functions[f->name.text] = f;
                }

                p.poisoned.insert(summary->poisoned.begin(), summary->poisoned.end());

                for (const auto& import : summary->imports)
                    readFile(baseDir, import, p);
            }
        }

        namespace
        {
            void filesOfStatements(const std::string& baseDir, const std::vector<StatementPtr>& statements, size_t file, std::set<std::string>& visited, ProgramFiles& out);

            // The modules of a program in the order an import reaches them, each once: their
            // statements as parsed. Native modules (no file) are none of them. What the compiler would
            // take from a .pgc cannot be read from a source. The file a name adds, negative when none
            int filesOf(const std::string& baseDir, const std::string& name, std::set<std::string>& visited, ProgramFiles& out)
            {
                std::string fileName = name;

                if (fileName.find(".pg") == std::string::npos)
                    fileName += ".pg";

                const std::string fullPath = baseDir + fileName;

                if (visited.count(fullPath))
                    return -1;

                visited.insert(fullPath);

                // The spellings the compiler takes bytecode from (CParser::parseImportFile): a name
                // given as one, `m.pgc`, or `m.pg.pgc`
                if (fileName.find(".pgc") != std::string::npos or UniversalFileAccessor::exists(fullPath + "c") or UniversalFileAccessor::exists(fullPath + ".pgc"))
                {
                    out.complete = false;
                    return -1;
                }

                if (not UniversalFileAccessor::exists(fullPath))
                    return -1;

                const auto summary = summaryOf(fullPath);

                if (not summary->read)
                {
                    out.complete = false;
                    return -1;
                }

                const size_t file = out.files.size();

                out.files.push_back(summary->statements);
                out.imports.emplace_back();

                filesOfStatements(baseDir, summary->statements, file, visited, out);

                return static_cast<int>(file);
            }

            void filesOfStatements(const std::string& baseDir, const std::vector<StatementPtr>& statements, size_t file, std::set<std::string>& visited, ProgramFiles& out)
            {
                size_t index = 0;

                for (const auto& s : statements)
                {
                    if (not s)
                        continue;

                    if (s->kind() == AstKind::ImportStatement)
                    {
                        for (const auto& import : listOf(std::static_pointer_cast<ImportStatement>(s)->imports))
                        {
                            const int imported = filesOf(baseDir, importName(import), visited, out);

                            if (imported >= 0)
                                out.imports[file].push_back({index, static_cast<size_t>(imported)});
                        }
                    }
                    else if (holdsStatement(s, AstKind::ImportStatement, true))
                        out.complete = false;

                    ++index;
                }
            }
        }

        bool holdsStatement(const StatementPtr& s, AstKind kind, bool intoFunctions)
        {
            if (not s)
                return false;

            if (s->kind() == kind)
                return true;

            auto inBody = [&](const StatementPtr& body) { return holdsStatement(body, kind, intoFunctions); };

            switch (s->kind())
            {
                case AstKind::BlockStatement:
                {
                    for (const auto& b : listOf(std::static_pointer_cast<BlockStatement>(s)->statements))
                    {
                        if (inBody(b))
                            return true;
                    }

                    return false;
                }

                case AstKind::IfStatement:
                {
                    auto i = std::static_pointer_cast<IfStatement>(s);
                    return inBody(i->thenBranch) or inBody(i->elseBranch);
                }

                case AstKind::WhileStatement: return inBody(std::static_pointer_cast<WhileStatement>(s)->body);
                case AstKind::ForStatement: return inBody(std::static_pointer_cast<ForStatement>(s)->body);
                case AstKind::ForInStatement: return inBody(std::static_pointer_cast<ForInStatement>(s)->body);

                case AstKind::FunctionStatement:
                    return intoFunctions and inBody(std::static_pointer_cast<FunctionStatement>(s)->body);

                case AstKind::ClassStatement:
                {
                    if (not intoFunctions)
                        return false;

                    for (const auto& method : listOf(std::static_pointer_cast<ClassStatement>(s)->methods))
                    {
                        if (inBody(method->body))
                            return true;
                    }

                    return false;
                }

                default:
                    return false;
            }
        }

        ProgramFiles readProgramFiles(VM* vm, const std::queue<StatementPtr>& statements)
        {
            ProgramFiles program;
            std::set<std::string> visited;
            std::string baseDir;
            const std::string mainFile = vm ? vm->currentFileName : std::string();

            if (not mainFile.empty())
            {
                visited.insert(mainFile);

                const size_t slash = mainFile.find_last_of("/\\");

                if (slash != std::string::npos)
                    baseDir = mainFile.substr(0, slash + 1);
            }

            const auto main = listOf(statements);
            program.files.push_back(main);
            program.imports.emplace_back();

            filesOfStatements(baseDir, main, 0, visited, program);

            return program;
        }

        ProgramView readProgram(VM* vm, const std::queue<StatementPtr>& statements)
        {
            ProgramView program;
            std::string baseDir;
            const std::string mainFile = vm ? vm->currentFileName : std::string();

            if (not mainFile.empty())
            {
                program.visited.insert(mainFile);

                const size_t slash = mainFile.find_last_of("/\\");

                if (slash != std::string::npos)
                    baseDir = mainFile.substr(0, slash + 1);
            }

            readStatements(baseDir, listOf(statements), program);

            return program;
        }
    }
}
