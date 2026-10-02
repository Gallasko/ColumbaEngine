#include "prefabloader.h"

#include "ECS/entitysystem.h"
#include "Compiler/vm.h"
#include "Compiler/vmreaders.h"
#include "2D/position.h"
#include "logger.h"

#include <algorithm>
#include <cctype>

namespace pg
{
namespace
{
    static constexpr const char* const DOM = "Prefab Loader";

    struct Loader
    {
        VM& vm;
        std::vector<std::string>* errors;
        std::string file;

        void fail(const std::string& message)
        {
            LOG_ERROR(DOM, file << ": " << message);
            if (errors)
                errors->push_back(message);
        }

        static std::string lower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        bool isScalar(Value v) const
        {
            return vmread::isScalar(v);
        }

        ElementType scalar(Value v)
        {
            return vmread::scalar(vm, v);
        }

        float number(Value v, const std::string& what, float fallback = 0.0f)
        {
            if (IS_INT(v))
                return static_cast<float>(AS_INT(v));
            if (IS_DOUBLE(v))
                return static_cast<float>(AS_DOUBLE(v));

            fail(what + " must be a number");
            return fallback;
        }

        std::string string(Value v, const std::string& what)
        {
            if (IS_STRING(v))
                return vm.asString(v);

            fail(what + " must be a string");
            return "";
        }

        // "top" / "Top" / "verticalcenter" / "VerticalCenter" ... -> AnchorType
        AnchorType anchorSide(const std::string& name, const std::string& what)
        {
            const std::string wanted = lower(name);

            for (const auto& kv : AnchorTypeToStringMap)
            {
                if (lower(kv.second) == wanted)
                    return kv.first;
            }

            fail(what + ": unknown anchor side '" + name + "'");
            return AnchorType::None;
        }

        // A flat map of scalars (one record).
        bool record(Value table, ElementMap& out, const std::string& what)
        {
            std::vector<std::string> problems;
            const bool ok = vmread::record(vm, table, out, &problems, what);

            for (const auto& problem : problems)
                fail(problem);

            return ok;
        }

        void anchor(ObjInstance* table, NodeSpec& node, const std::string& what)
        {
            AnchorSpec a;

            for (size_t i = 0; i < table->fieldValues.size(); ++i)
            {
                const std::string& key = table->fieldNames[i];
                Value v = table->fieldValues[i];

                if (key == "side")
                    a.side = anchorSide(string(v, what + ".side"), what);
                else if (key == "targetSide")
                    a.targetSide = anchorSide(string(v, what + ".targetSide"), what);
                else if (key == "target")
                    a.target = string(v, what + ".target");
                else if (key == "margin")
                    a.margin = number(v, what + ".margin");
                else if (key.rfind("__", 0) != 0)
                    fail(what + ": unknown anchor key '" + key + "'");
            }

            if (a.side == AnchorType::None)
            {
                fail(what + ": an anchor needs a `side`");
                return;
            }

            node.anchors.push_back(std::move(a));
        }

        bool node(Value v, NodeSpec& out, const std::string& what)
        {
            if (not IS_INSTANCE(v))
            {
                fail(what + ": a node must be a map");
                return false;
            }

            ObjInstance* table = vm.asInstance(v);

            for (size_t i = 0; i < table->fieldValues.size(); ++i)
            {
                const std::string& key = table->fieldNames[i];
                Value value = table->fieldValues[i];

                if (key.empty() or key.rfind("__", 0) == 0)
                    continue;

                const std::string path = what + "." + key;

                if (key == "kind")
                {
                    out.kind = string(value, path);
                }
                else if (key == "name")
                {
                    out.name = string(value, path);
                }
                else if (key == "theme")
                {
                    out.theme = string(value, path);
                }
                else if (key == "flow")
                {
                    const std::string f = lower(string(value, path));
                    if (f == "horizontal")      out.flow = Flow::Horizontal;
                    else if (f == "vertical")   out.flow = Flow::Vertical;
                    else if (f == "none" or f.empty()) out.flow = Flow::None;
                    else fail(path + ": unknown flow '" + f + "'");
                }
                else if (key == "padding")
                {
                    out.padding = number(value, path);
                    out.props[key] = scalar(value);
                }
                else if (key == "spacing")
                {
                    out.spacing = number(value, path);
                    out.props[key] = scalar(value);
                }
                else if (key == "anchors")
                {
                    if (not IS_VECTOR(value))
                    {
                        fail(path + ": must be a list of {side, target, ...}");
                        continue;
                    }

                    size_t n = 0;
                    for (Value item : vm.asVector(value)->fields)
                    {
                        const std::string itemPath = path + "[" + std::to_string(n++) + "]";
                        if (IS_INSTANCE(item))
                            anchor(vm.asInstance(item), out, itemPath);
                        else
                            fail(itemPath + ": an anchor must be a map");
                    }
                }
                else if (key == "children")
                {
                    if (not IS_VECTOR(value))
                    {
                        fail(path + ": must be a list of nodes");
                        continue;
                    }

                    size_t n = 0;
                    for (Value item : vm.asVector(value)->fields)
                    {
                        const std::string itemPath = path + "[" + std::to_string(n++) + "]";
                        NodeSpec child;
                        if (node(item, child, itemPath))
                            out.children.push_back(std::move(child));
                    }
                }
                else if (isScalar(value))
                {
                    out.props[key] = scalar(value);
                }
                else if (IS_VECTOR(value))
                {
                    RecordList records;
                    size_t n = 0;
                    bool ok = true;

                    for (Value item : vm.asVector(value)->fields)
                    {
                        const std::string itemPath = path + "[" + std::to_string(n++) + "]";
                        if (not IS_INSTANCE(item))
                        {
                            fail(itemPath + ": a list under a prop key must hold maps (records)");
                            ok = false;
                            break;
                        }

                        ElementMap rec;
                        if (not record(item, rec, itemPath))
                        {
                            ok = false;
                            break;
                        }

                        records.push_back(std::move(rec));
                    }

                    if (ok)
                        out.records[key] = std::move(records);
                }
                else if (IS_INSTANCE(value))
                {
                    // A single nested map is a one-element record list (e.g. a MarkedLabel's `label`).
                    ElementMap rec;
                    if (record(value, rec, path))
                        out.records[key] = RecordList{std::move(rec)};
                }
                else
                {
                    fail(path + ": unsupported value");
                }
            }

            return true;
        }
    };
}

std::optional<NodeSpec> loadNodeSpec(EntitySystem* ecs, const std::string& yamlPath, const PrefabLoadOptions& options)
{
    if (not ecs)
    {
        LOG_ERROR(DOM, "No EntitySystem to set the VM up with");
        return std::nullopt;
    }

    VM vm;
    ecs->setupVm(vm);

    vm.defineGlobal("uiFile", vm.createString(yamlPath));

    const InterpretResult result = vm.interpretFromFile(options.loaderScript);
    if (result != InterpretResult::OK)
    {
        LOG_ERROR(DOM, yamlPath << ": the loader script '" << options.loaderScript << "' failed");
        if (options.errors)
            options.errors->push_back("loader script failed");
        return std::nullopt;
    }

    Loader loader{vm, options.errors, yamlPath};

    // Parser diagnostics first: they explain any structural oddity below.
    if (VM::GlobalCell* errs = vm.findDefinedGlobal("errors"); errs != nullptr and IS_VECTOR(errs->value))
    {
        for (Value e : vm.asVector(errs->value)->fields)
        {
            if (IS_STRING(e))
                loader.fail(vm.asString(e));
        }
    }

    VM::GlobalCell* doc = vm.findDefinedGlobal("doc");
    if (doc == nullptr)
    {
        loader.fail("the loader script defined no `doc`");
        return std::nullopt;
    }

    if (not IS_INSTANCE(doc->value))
    {
        loader.fail("the file must hold a map at the top level (a single node)");
        return std::nullopt;
    }

    NodeSpec spec;
    if (not loader.node(doc->value, spec, "doc"))
        return std::nullopt;

    return spec;
}
}
