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

            void poisonExpr(const ExprPtr& e, ProgramView& p);
            void poisonStmt(const StatementPtr& s, ProgramView& p);

            void poisonExpr(const ExprPtr& e, ProgramView& p)
            {
                if (not e)
                    return;

                const AstKind type = e->kind();

                if (type == AstKind::Assign)
                {
                    auto a = std::static_pointer_cast<Assign>(e);
                    p.poisoned.insert(a->name.text);
                    poisonExpr(a->expr, p);
                }
                else if (type == AstKind::PreFixExpression)
                    p.poisoned.insert(std::static_pointer_cast<PreFixExpression>(e)->name.text);
                else if (type == AstKind::PostFixExpression)
                    p.poisoned.insert(std::static_pointer_cast<PostFixExpression>(e)->name.text);
                else if (type == AstKind::BinaryExpression)
                {
                    auto b = std::static_pointer_cast<BinaryExpression>(e);
                    poisonExpr(b->leftExpr, p);
                    poisonExpr(b->rightExpr, p);
                }
                else if (type == AstKind::LogicExpression)
                {
                    auto l = std::static_pointer_cast<LogicExpression>(e);
                    poisonExpr(l->leftExpr, p);
                    poisonExpr(l->rightExpr, p);
                }
                else if (type == AstKind::UnaryExpression)
                    poisonExpr(std::static_pointer_cast<UnaryExpression>(e)->expr, p);
                else if (type == AstKind::CompoundAtom)
                    poisonExpr(std::static_pointer_cast<CompoundAtom>(e)->expr, p);
                else if (type == AstKind::CallExpression)
                {
                    auto c = std::static_pointer_cast<CallExpression>(e);
                    poisonExpr(c->caller, p);

                    for (const auto& a : listOf(c->args))
                        poisonExpr(a, p);
                }
                else if (type == AstKind::Get)
                    poisonExpr(std::static_pointer_cast<Get>(e)->object, p);
                else if (type == AstKind::Set)
                {
                    auto s = std::static_pointer_cast<Set>(e);
                    poisonExpr(s->object, p);
                    poisonExpr(s->value, p);
                }
                else if (type == AstKind::IndexGet)
                {
                    auto g = std::static_pointer_cast<IndexGet>(e);
                    poisonExpr(g->object, p);
                    poisonExpr(g->index, p);
                }
                else if (type == AstKind::IndexSet)
                {
                    auto g = std::static_pointer_cast<IndexSet>(e);
                    poisonExpr(g->object, p);
                    poisonExpr(g->index, p);
                    poisonExpr(g->value, p);
                }
                else if (type == AstKind::List)
                {
                    for (const auto& entry : listOf(std::static_pointer_cast<List>(e)->entries))
                    {
                        poisonExpr(entry.key, p);
                        poisonExpr(entry.value, p);
                    }
                }
                else if (type == AstKind::AnonymousFunction)
                    poisonStmt(std::static_pointer_cast<AnonymousFunction>(e)->body, p);
            }

            void poisonStmt(const StatementPtr& s, ProgramView& p)
            {
                if (not s)
                    return;

                const AstKind type = s->kind();

                if (type == AstKind::ExpressionStatement)
                    poisonExpr(std::static_pointer_cast<ExpressionStatement>(s)->expr, p);
                else if (type == AstKind::VariableStatement)
                {
                    auto v = std::static_pointer_cast<VariableStatement>(s);
                    p.poisoned.insert(v->name.text);
                    poisonExpr(v->expr, p);
                }
                else if (type == AstKind::FunctionStatement)
                    poisonStmt(std::static_pointer_cast<FunctionStatement>(s)->body, p);
                else if (type == AstKind::ClassStatement)
                {
                    auto c = std::static_pointer_cast<ClassStatement>(s);
                    p.poisoned.insert(c->name.text);

                    for (const auto& m : listOf(c->methods))
                        poisonStmt(m, p);
                }
                else if (type == AstKind::BlockStatement)
                {
                    for (const auto& b : listOf(std::static_pointer_cast<BlockStatement>(s)->statements))
                        poisonStmt(b, p);
                }
                else if (type == AstKind::IfStatement)
                {
                    auto i = std::static_pointer_cast<IfStatement>(s);
                    poisonExpr(i->condition, p);
                    poisonStmt(i->thenBranch, p);
                    poisonStmt(i->elseBranch, p);
                }
                else if (type == AstKind::WhileStatement)
                {
                    auto w = std::static_pointer_cast<WhileStatement>(s);
                    poisonExpr(w->condition, p);
                    poisonStmt(w->body, p);
                }
                else if (type == AstKind::ForStatement)
                {
                    auto f = std::static_pointer_cast<ForStatement>(s);
                    poisonStmt(f->initializer, p);
                    poisonExpr(f->condition, p);
                    poisonExpr(f->increment, p);
                    poisonStmt(f->body, p);
                }
                else if (type == AstKind::ForInStatement)
                {
                    auto f = std::static_pointer_cast<ForInStatement>(s);
                    p.poisoned.insert(f->varName.text);
                    poisonExpr(f->iterable, p);
                    poisonStmt(f->body, p);
                }
                else if (type == AstKind::ReturnStatement)
                    poisonExpr(std::static_pointer_cast<ReturnStatement>(s)->value, p);
                else if (type == AstKind::DPrintStatement)
                    poisonExpr(std::static_pointer_cast<DPrintStatement>(s)->expr, p);
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

                    for (const auto& s : listOf(statements))
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
