#include "factories.h"

#include <algorithm>
#include <any>
#include <cctype>
#include <utility>

#include "ECS/entitysystem.h"

#include "label.h"
#include "mark.h"
#include "ornament.h"
#include "panel.h"
#include "button.h"
#include "tabs.h"
#include "gloss.h"
#include "progressrule.h"
#include "statline.h"
#include "requirementlist.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char* DOM = "Chronicle Factories";

        using Req = ParamSchema::Requirement;

        std::string lower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        std::string elementToString(const ElementType& v)
        {
            switch (v.type)
            {
                case UnionType::STRING: return v.get<std::string>();
                case UnionType::INT:    return std::to_string(v.get<int>());
                case UnionType::SIZE_T: return std::to_string(v.get<size_t>());
                case UnionType::FLOAT:  return std::to_string(v.get<float>());
                case UnionType::DOUBLE: return std::to_string(v.get<double>());
                case UnionType::BOOL:   return v.get<bool>() ? "true" : "false";
            }

            return "";
        }

        // ---- prop readers ---------------------------------------------------------------

        // A number, or a spacing token ("space-3" -> 12).
        float numberProp(const ElementMap& p, const std::string& key, const Tokens& tokens, float fallback)
        {
            auto it = p.find(key);
            if (it == p.end() or it->second.isEmpty())
                return fallback;

            if (it->second.type == UnionType::STRING)
            {
                const std::string s = it->second.get<std::string>();
                if (s.rfind("space-", 0) == 0)
                    return tokens.spacing(s);

                LOG_ERROR(DOM, key << ": '" << s << "' is neither a number nor a space-N token");
                return fallback;
            }

            return getParamFloat(p, key, fallback);
        }

        std::string stringProp(const ElementMap& p, const std::string& key, const std::string& fallback = "")
        {
            auto it = p.find(key);
            if (it == p.end() or it->second.isEmpty())
                return fallback;

            return elementToString(it->second);
        }

        // A colour token. Empty stays empty (some specs use "" for "the default tone").
        std::string colourProp(const ElementMap& p, const std::string& key, const Tokens& tokens, const std::string& fallback)
        {
            const std::string value = stringProp(p, key, fallback);
            if (value.empty() or tokens.hasColour(value))
                return value;

            LOG_ERROR(DOM, key << ": unknown colour token '" << value << "', using '" << fallback << "'");
            return fallback;
        }

        template <typename E>
        E enumProp(const ElementMap& p, const std::string& key,
                   const std::vector<std::pair<std::string, E>>& table, E fallback)
        {
            const std::string raw = stringProp(p, key);
            if (raw.empty())
                return fallback;

            const std::string wanted = lower(raw);
            for (const auto& [name, value] : table)
            {
                if (name == wanted)
                    return value;
            }

            std::string options;
            for (const auto& [name, value] : table)
                options += (options.empty() ? "" : ", ") + name;

            LOG_ERROR(DOM, key << ": unknown value '" << raw << "' (expected one of: " << options << ")");
            return fallback;
        }

        // `met` may be written as a bool or as the spec's int.
        int metProp(const ElementMap& rec)
        {
            auto it = rec.find("met");
            if (it == rec.end() or it->second.isEmpty())
                return -1;

            if (it->second.type == UnionType::BOOL)
                return it->second.get<bool>() ? 1 : 0;

            return getParamInt(rec, "met", -1);
        }

        const RecordList* recordsOf(const NodeSpec& spec, const std::string& key)
        {
            auto it = spec.records.find(key);
            return it == spec.records.end() ? nullptr : &it->second;
        }

        // ---- shared spec builders -------------------------------------------------------

        const std::vector<std::pair<std::string, Align>> ALIGN = {
            {"left", Align::Left}, {"centre", Align::Centre}, {"center", Align::Centre}, {"right", Align::Right},
        };

        const std::vector<std::pair<std::string, Overflow>> OVERFLOW = {
            {"grow", Overflow::Grow}, {"wrap", Overflow::Wrap}, {"ellipsis", Overflow::Ellipsis},
        };

        void labelSchema(ParamSchema& schema)
        {
            schema.entries.push_back({"style",    "body"});
            schema.entries.push_back({"text",     ""});
            schema.entries.push_back({"colour",   "ink"});
            schema.entries.push_back({"align",    "left"});
            schema.entries.push_back({"overflow", "grow"});
            schema.entries.push_back({"width",    0.0f});
            schema.entries.push_back({"maxLines", 0});
        }

        // Reads a LabelSpec from a prop map; `base` supplies the fallbacks (the schema-merged
        // node props for a Label node, or the defaults for a nested `label` record).
        LabelSpec labelSpecFrom(const ElementMap& p, const Tokens& tokens, const LabelSpec& base)
        {
            LabelSpec s = base;
            s.style    = stringProp(p, "style", base.style);
            s.text     = stringProp(p, "text", base.text);
            s.colour   = colourProp(p, "colour", tokens, base.colour);
            s.align    = enumProp(p, "align", ALIGN, base.align);
            s.overflow = enumProp(p, "overflow", OVERFLOW, base.overflow);
            s.width    = numberProp(p, "width", tokens, base.width);
            s.maxLines = getParamInt(p, "maxLines", base.maxLines);
            s.z        = getParamInt(p, "z", base.z);
            return s;
        }

        MarkSize markSizeProp(const ElementMap& p, const std::string& key, MarkSize fallback)
        {
            const int px = getParamInt(p, key, static_cast<int>(fallback));
            switch (px)
            {
                case 14: return MarkSize::S14;
                case 16: return MarkSize::S16;
                case 18: return MarkSize::S18;
                case 24: return MarkSize::S24;
                case 48: return MarkSize::S48;
                default:
                    LOG_ERROR(DOM, key << ": a mark is drawn at 14, 16, 18, 24 or 48 px, not " << px);
                    return fallback;
            }
        }

        template <typename Handle>
        FactoryResult result(EntityRef entity, Handle&& handle)
        {
            FactoryResult r;
            r.entity = entity;
            r.handle = std::make_any<std::decay_t<Handle>>(std::forward<Handle>(handle));
            return r;
        }

        // ---- kinds ------------------------------------------------------------------------

        void registerLabel(PrefabFactoryRegistry* registry, const Tokens* tokens, const TextStyles* styles)
        {
            ParamSchema schema;
            labelSchema(schema);
            schema.entries.push_back({"z", 0});

            registry->registerFactory("Label", std::move(schema),
                PrefabFactoryFnEx{[tokens, styles](EntitySystem* ecs, const NodeSpec& spec, BuildContext&) -> FactoryResult
                {
                    Label label = makeLabel(ecs, *tokens, *styles, labelSpecFrom(spec.props, *tokens, LabelSpec{}));
                    return result(label.entity, std::move(label));
                }});
        }

        void registerMark(PrefabFactoryRegistry* registry, const Tokens* tokens)
        {
            ParamSchema schema;
            schema.entries = {
                {"name",   "seal"},
                {"size",   16},
                {"colour", "ink"},
                {"z",      0},
            };

            registry->registerFactory("Mark", std::move(schema),
                PrefabFactoryFnEx{[tokens](EntitySystem* ecs, const NodeSpec& spec, BuildContext&) -> FactoryResult
                {
                    MarkSpec s;
                    s.name   = stringProp(spec.props, "name", s.name);
                    s.size   = markSizeProp(spec.props, "size", s.size);
                    s.colour = colourProp(spec.props, "colour", *tokens, s.colour);
                    s.z      = getParamInt(spec.props, "z", s.z);

                    Mark mark = makeMark(ecs, *tokens, s);
                    return result(mark.entity, std::move(mark));
                }});
        }

        void registerMarkedLabel(PrefabFactoryRegistry* registry, const Tokens* tokens, const TextStyles* styles)
        {
            ParamSchema schema;
            labelSchema(schema);
            schema.entries.push_back({"mark",        ""});
            schema.entries.push_back({"reserveMark", false});
            schema.entries.push_back({"gap",         -1.0f});
            schema.entries.push_back({"z",           0});

            registry->registerFactory("MarkedLabel", std::move(schema),
                PrefabFactoryFnEx{[tokens, styles](EntitySystem* ecs, const NodeSpec& spec, BuildContext&) -> FactoryResult
                {
                    MarkedLabelSpec s;
                    s.mark        = stringProp(spec.props, "mark", s.mark);
                    s.reserveMark = getParamBool(spec.props, "reserveMark", s.reserveMark);
                    s.gap         = numberProp(spec.props, "gap", *tokens, s.gap);
                    s.z           = getParamInt(spec.props, "z", s.z);

                    // The label: flat props (`text`, `style`, ...) overridden by a nested `label` map.
                    LabelSpec flat = labelSpecFrom(spec.props, *tokens, LabelSpec{});
                    flat.z = 0;   // the label's z is the MarkedLabel's business (root z + 1)
                    if (const RecordList* nested = recordsOf(spec, "label"); nested and not nested->empty())
                        s.label = labelSpecFrom(nested->front(), *tokens, flat);
                    else
                        s.label = flat;

                    MarkedLabel ml = makeMarkedLabel(ecs, *tokens, *styles, s);
                    return result(ml.root, std::move(ml));
                }});
        }

        void registerOrnament(PrefabFactoryRegistry* registry, const Tokens* tokens, const TextStyles* styles)
        {
            ParamSchema schema;
            schema.entries = {
                {"ornament", "divider"},
                {"weight",   "hair"},
                {"knot",     true},
                {"width",    0.0f},
                {"ground",   "folio"},
                {"colour",   ""},
                {"corner",   "tl"},
                {"letter",   "A"},
                {"tone",     "vermilion"},
                {"z",        0},
            };

            static const std::vector<std::pair<std::string, OrnamentKind>> KIND = {
                {"divider", OrnamentKind::Divider}, {"flourish", OrnamentKind::Flourish},
                {"corner", OrnamentKind::Corner}, {"versal", OrnamentKind::Versal},
            };
            static const std::vector<std::pair<std::string, DividerWeight>> WEIGHT = {
                {"hair", DividerWeight::Hair}, {"rule", DividerWeight::Rule},
            };
            static const std::vector<std::pair<std::string, CornerPos>> CORNER = {
                {"tl", CornerPos::TL}, {"tr", CornerPos::TR}, {"bl", CornerPos::BL}, {"br", CornerPos::BR},
            };
            static const std::vector<std::pair<std::string, VersalTone>> TONE = {
                {"vermilion", VersalTone::Vermilion}, {"gold", VersalTone::Gold}, {"lapis", VersalTone::Lapis},
            };

            registry->registerFactory("Ornament", std::move(schema),
                PrefabFactoryFnEx{[tokens, styles](EntitySystem* ecs, const NodeSpec& spec, BuildContext&) -> FactoryResult
                {
                    OrnamentSpec s;
                    s.kind   = enumProp(spec.props, "ornament", KIND, s.kind);
                    s.weight = enumProp(spec.props, "weight", WEIGHT, s.weight);
                    s.knot   = getParamBool(spec.props, "knot", s.knot);
                    s.width  = numberProp(spec.props, "width", *tokens, s.width);
                    s.ground = colourProp(spec.props, "ground", *tokens, s.ground);
                    s.colour = colourProp(spec.props, "colour", *tokens, s.colour);
                    s.corner = enumProp(spec.props, "corner", CORNER, s.corner);
                    s.letter = stringProp(spec.props, "letter", s.letter);
                    s.tone   = enumProp(spec.props, "tone", TONE, s.tone);
                    s.z      = getParamInt(spec.props, "z", s.z);

                    Ornament o = makeOrnament(ecs, *tokens, *styles, s);
                    return result(o.root, std::move(o));
                }});
        }

        void registerPanel(PrefabFactoryRegistry* registry, const Tokens* tokens, const TextStyles* styles)
        {
            ParamSchema schema;
            schema.entries = {
                {"frame",    "ruled"},
                {"width",    320.0f},
                {"heading",  ""},
                {"glyph",    ""},
                {"aside",    ""},
                {"z",        10},
                {"contentZ", -1},      // -1 -> z + 10
            };

            static const std::vector<std::pair<std::string, PanelFrame>> FRAME = {
                {"hair", PanelFrame::Hair}, {"ruled", PanelFrame::Ruled},
                {"plain", PanelFrame::Plain}, {"illuminated", PanelFrame::Illuminated},
            };

            registry->registerFactory("Panel", std::move(schema),
                PrefabFactoryFnEx{[tokens, styles](EntitySystem* ecs, const NodeSpec& spec, BuildContext&) -> FactoryResult
                {
                    PanelSpec s;
                    s.frame    = enumProp(spec.props, "frame", FRAME, s.frame);
                    s.width    = numberProp(spec.props, "width", *tokens, s.width);
                    s.heading  = stringProp(spec.props, "heading", s.heading);
                    s.glyph    = stringProp(spec.props, "glyph", s.glyph);
                    s.aside    = stringProp(spec.props, "aside", s.aside);
                    s.z        = getParamInt(spec.props, "z", s.z);
                    const int contentZ = getParamInt(spec.props, "contentZ", -1);
                    s.contentZ = contentZ < 0 ? s.z + 10 : contentZ;

                    Panel panel = makePanel(ecs, *tokens, *styles, s);

                    // The generalised part of makePanel's caller contract: children draw in the
                    // content band and span the inner width unless they say otherwise, and they
                    // go into the body layout.
                    FactoryResult r;
                    r.entity = panel.root;
                    r.slot   = panel.body;
                    r.childDefaults = {
                        {"width", ElementType{panel.innerWidth()}},
                        {"z",     ElementType{panel.spec.contentZ}},
                    };
                    r.handle = std::make_any<Panel>(std::move(panel));
                    return r;
                }});
        }

        void registerButton(PrefabFactoryRegistry* registry, const Tokens* tokens, const TextStyles* styles)
        {
            ParamSchema schema;
            schema.entries = {
                {"variant",  "quiet"},
                {"label",    UnionType::STRING, Req::Required},
                {"glyph",    ""},
                {"months",   -1},
                {"disabled", false},
                {"reason",   ""},
                {"tag",      ""},
                {"z",        20},
            };

            static const std::vector<std::pair<std::string, ButtonVariant>> VARIANT = {
                {"quiet", ButtonVariant::Quiet}, {"study", ButtonVariant::Study}, {"seal", ButtonVariant::Seal},
            };

            registry->registerFactory("Button", std::move(schema),
                PrefabFactoryFnEx{[tokens, styles](EntitySystem* ecs, const NodeSpec& spec, BuildContext&) -> FactoryResult
                {
                    ButtonSpec s;
                    s.variant  = enumProp(spec.props, "variant", VARIANT, s.variant);
                    s.label    = stringProp(spec.props, "label", s.label);
                    s.glyph    = stringProp(spec.props, "glyph", s.glyph);
                    s.months   = getParamInt(spec.props, "months", s.months);
                    s.disabled = getParamBool(spec.props, "disabled", s.disabled);
                    s.reason   = stringProp(spec.props, "reason", s.reason);
                    s.tag      = stringProp(spec.props, "tag", s.tag);
                    s.z        = getParamInt(spec.props, "z", s.z);

                    Button b = makeButton(ecs, *tokens, *styles, s);
                    return result(b.root, std::move(b));
                }});
        }

        void registerTabs(PrefabFactoryRegistry* registry, const Tokens* tokens, const TextStyles* styles)
        {
            ParamSchema schema;
            schema.entries = {
                {"active", 0},
                {"width",  0.0f},
                {"tag",    ""},
                {"z",      20},
            };

            registry->registerFactory("Tabs", std::move(schema),
                PrefabFactoryFnEx{[tokens, styles](EntitySystem* ecs, const NodeSpec& spec, BuildContext&) -> FactoryResult
                {
                    TabsSpec s;
                    s.active = getParamInt(spec.props, "active", s.active);
                    s.width  = numberProp(spec.props, "width", *tokens, s.width);
                    s.tag    = stringProp(spec.props, "tag", s.tag);
                    s.z      = getParamInt(spec.props, "z", s.z);

                    if (const RecordList* items = recordsOf(spec, "items"))
                    {
                        for (const auto& rec : *items)
                        {
                            TabItem item;
                            item.label = stringProp(rec, "label");
                            item.glyph = stringProp(rec, "glyph");
                            item.badge = getParamInt(rec, "badge", 0);
                            s.items.push_back(std::move(item));
                        }
                    }

                    if (s.items.empty())
                        LOG_ERROR(DOM, "Tabs: `items` is empty; a tab row needs 2 to 6 items");

                    Tabs t = makeTabs(ecs, *tokens, *styles, s);
                    return result(t.root, std::move(t));
                }});
        }

        void registerGloss(PrefabFactoryRegistry* registry, const Tokens* tokens, const TextStyles* styles)
        {
            ParamSchema schema;
            schema.entries = {
                {"gloss",    "margin"},
                {"title",    ""},
                {"text",     ""},
                {"footnote", ""},
                {"width",    0.0f},
                {"z",        20},
            };

            static const std::vector<std::pair<std::string, GlossKind>> KIND = {
                {"margin", GlossKind::Margin}, {"tooltip", GlossKind::Tooltip},
            };

            registry->registerFactory("Gloss", std::move(schema),
                PrefabFactoryFnEx{[tokens, styles](EntitySystem* ecs, const NodeSpec& spec, BuildContext&) -> FactoryResult
                {
                    GlossSpec s;
                    s.kind     = enumProp(spec.props, "gloss", KIND, s.kind);
                    s.title    = stringProp(spec.props, "title", s.title);
                    s.text     = stringProp(spec.props, "text", s.text);
                    s.footnote = stringProp(spec.props, "footnote", s.footnote);
                    s.width    = numberProp(spec.props, "width", *tokens, s.width);
                    s.z        = getParamInt(spec.props, "z", s.z);

                    if (const RecordList* rows = recordsOf(spec, "rows"))
                    {
                        for (const auto& rec : *rows)
                            s.rows.push_back({stringProp(rec, "label"), stringProp(rec, "value")});
                    }

                    Gloss g = makeGloss(ecs, *tokens, *styles, s);
                    return result(g.root, std::move(g));
                }});
        }

        void registerProgressRule(PrefabFactoryRegistry* registry, const Tokens* tokens, const TextStyles* styles)
        {
            ParamSchema schema;
            schema.entries = {
                {"width",       240.0f},
                {"small",       false},
                {"percent",     0.0f},
                {"forecast",    0.0f},
                {"nib",         true},
                {"caption",     ""},
                {"trackHeight", 0.0f},
                {"z",           20},
            };

            registry->registerFactory("ProgressRule", std::move(schema),
                PrefabFactoryFnEx{[tokens, styles](EntitySystem* ecs, const NodeSpec& spec, BuildContext&) -> FactoryResult
                {
                    ProgressRuleSpec s;
                    s.width           = numberProp(spec.props, "width", *tokens, s.width);
                    s.small           = getParamBool(spec.props, "small", s.small);
                    s.percent         = getParamFloat(spec.props, "percent", s.percent);
                    s.forecastPercent = getParamFloat(spec.props, "forecast", s.forecastPercent);
                    s.nib             = getParamBool(spec.props, "nib", s.nib);
                    s.caption         = stringProp(spec.props, "caption", s.caption);
                    s.trackHeight     = numberProp(spec.props, "trackHeight", *tokens, s.trackHeight);
                    s.z               = getParamInt(spec.props, "z", s.z);

                    ProgressRule pr = makeProgressRule(ecs, *tokens, *styles, s);
                    return result(pr.root, std::move(pr));
                }});
        }

        void registerStatLine(PrefabFactoryRegistry* registry, const Tokens* tokens, const TextStyles* styles)
        {
            ParamSchema schema;
            schema.entries = {
                {"width",     288.0f},
                {"label",     "Strength"},
                {"glyph",     "strength"},
                {"value",     0},
                {"max",       30},
                {"projected", -1},
                {"threshold", 0},
                {"note",      ""},
                {"glossKey",  ""},
                {"z",         20},
            };

            registry->registerFactory("StatLine", std::move(schema),
                PrefabFactoryFnEx{[tokens, styles](EntitySystem* ecs, const NodeSpec& spec, BuildContext&) -> FactoryResult
                {
                    StatLineSpec s;
                    s.width     = numberProp(spec.props, "width", *tokens, s.width);
                    s.label     = stringProp(spec.props, "label", s.label);
                    s.glyph     = stringProp(spec.props, "glyph", s.glyph);
                    s.value     = getParamInt(spec.props, "value", s.value);
                    s.max       = getParamInt(spec.props, "max", s.max);
                    s.projected = getParamInt(spec.props, "projected", s.projected);
                    s.threshold = getParamInt(spec.props, "threshold", s.threshold);
                    s.note      = stringProp(spec.props, "note", s.note);
                    s.glossKey  = stringProp(spec.props, "glossKey", s.glossKey);
                    s.z         = getParamInt(spec.props, "z", s.z);

                    StatLine sl = makeStatLine(ecs, *tokens, *styles, s);
                    return result(sl.root, std::move(sl));
                }});
        }

        void registerRequirementList(PrefabFactoryRegistry* registry, const Tokens* tokens, const TextStyles* styles)
        {
            ParamSchema schema;
            schema.entries = {
                {"width", 256.0f},
                {"dense", false},
                {"z",     20},
            };

            registry->registerFactory("RequirementList", std::move(schema),
                PrefabFactoryFnEx{[tokens, styles](EntitySystem* ecs, const NodeSpec& spec, BuildContext&) -> FactoryResult
                {
                    RequirementListSpec s;
                    s.width = numberProp(spec.props, "width", *tokens, s.width);
                    s.dense = getParamBool(spec.props, "dense", s.dense);
                    s.z     = getParamInt(spec.props, "z", s.z);

                    if (const RecordList* items = recordsOf(spec, "items"))
                    {
                        for (const auto& rec : *items)
                        {
                            Requirement req;
                            req.label   = stringProp(rec, "label");
                            req.current = getParamInt(rec, "current", -1);
                            req.needed  = getParamInt(rec, "needed", 0);
                            req.met     = metProp(rec);
                            s.items.push_back(std::move(req));
                        }
                    }

                    RequirementList rl = makeRequirementList(ecs, *tokens, *styles, s);
                    return result(rl.root, std::move(rl));
                }});
        }
    }

    const std::vector<std::string>& chronicleKinds()
    {
        static const std::vector<std::string> kinds = {
            "Label", "Mark", "MarkedLabel", "Ornament", "Panel", "Button",
            "Tabs", "Gloss", "ProgressRule", "StatLine", "RequirementList",
        };

        return kinds;
    }

    void registerChronicleFactories(PrefabFactoryRegistry* registry, const Tokens* tokens, const TextStyles* styles)
    {
        if (not registry or not tokens or not styles)
        {
            LOG_ERROR(DOM, "registerChronicleFactories needs a registry, tokens and text styles");
            return;
        }

        registerLabel(registry, tokens, styles);
        registerMark(registry, tokens);
        registerMarkedLabel(registry, tokens, styles);
        registerOrnament(registry, tokens, styles);
        registerPanel(registry, tokens, styles);
        registerButton(registry, tokens, styles);
        registerTabs(registry, tokens, styles);
        registerGloss(registry, tokens, styles);
        registerProgressRule(registry, tokens, styles);
        registerStatLine(registry, tokens, styles);
        registerRequirementList(registry, tokens, styles);
    }
}
