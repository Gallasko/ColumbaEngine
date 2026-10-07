#include "factories.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <utility>

#include "ECS/entitysystem.h"
#include "UI/prefab.h"
#include "UI/sizer.h"
#include "UI/themesystem.h"

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
#include "lifeclock.h"
#include "activityrow.h"
#include "windowmeter.h"
#include "resourceledger.h"
#include "eventlog.h"

using namespace pg;

// The kit pieces are attached to their root entity as plain components so a scene reaches them
// with getEntity(name)->get<Panel>(). A component needs an archive form; the pieces are rebuilt
// from their file, never saved, so it is empty.
namespace pg
{
    namespace
    {
        template <typename Piece>
        void serializeEmptyPiece(Archive& archive, const char* name)
        {
            archive.startSerialization(name);
            archive.endSerialization();
        }
    }

    template <> void serialize(Archive& archive, const chronicle::Label& value) { (void)value; serializeEmptyPiece<chronicle::Label>(archive, "Label"); }
    template <> chronicle::Label deserialize(const UnserializedObject&) { return chronicle::Label{}; }
    template <> void serialize(Archive& archive, const chronicle::Mark& value) { (void)value; serializeEmptyPiece<chronicle::Mark>(archive, "Mark"); }
    template <> chronicle::Mark deserialize(const UnserializedObject&) { return chronicle::Mark{}; }
    template <> void serialize(Archive& archive, const chronicle::MarkedLabel& value) { (void)value; serializeEmptyPiece<chronicle::MarkedLabel>(archive, "MarkedLabel"); }
    template <> chronicle::MarkedLabel deserialize(const UnserializedObject&) { return chronicle::MarkedLabel{}; }
    template <> void serialize(Archive& archive, const chronicle::Ornament& value) { (void)value; serializeEmptyPiece<chronicle::Ornament>(archive, "Ornament"); }
    template <> chronicle::Ornament deserialize(const UnserializedObject&) { return chronicle::Ornament{}; }
    template <> void serialize(Archive& archive, const chronicle::Panel& value) { (void)value; serializeEmptyPiece<chronicle::Panel>(archive, "Panel"); }
    template <> chronicle::Panel deserialize(const UnserializedObject&) { return chronicle::Panel{}; }
    template <> void serialize(Archive& archive, const chronicle::Button& value) { (void)value; serializeEmptyPiece<chronicle::Button>(archive, "Button"); }
    template <> chronicle::Button deserialize(const UnserializedObject&) { return chronicle::Button{}; }
    template <> void serialize(Archive& archive, const chronicle::Tabs& value) { (void)value; serializeEmptyPiece<chronicle::Tabs>(archive, "Tabs"); }
    template <> chronicle::Tabs deserialize(const UnserializedObject&) { return chronicle::Tabs{}; }
    template <> void serialize(Archive& archive, const chronicle::Gloss& value) { (void)value; serializeEmptyPiece<chronicle::Gloss>(archive, "Gloss"); }
    template <> chronicle::Gloss deserialize(const UnserializedObject&) { return chronicle::Gloss{}; }
    template <> void serialize(Archive& archive, const chronicle::ProgressRule& value) { (void)value; serializeEmptyPiece<chronicle::ProgressRule>(archive, "ProgressRule"); }
    template <> chronicle::ProgressRule deserialize(const UnserializedObject&) { return chronicle::ProgressRule{}; }
    template <> void serialize(Archive& archive, const chronicle::StatLine& value) { (void)value; serializeEmptyPiece<chronicle::StatLine>(archive, "StatLine"); }
    template <> chronicle::StatLine deserialize(const UnserializedObject&) { return chronicle::StatLine{}; }
    template <> void serialize(Archive& archive, const chronicle::RequirementList& value) { (void)value; serializeEmptyPiece<chronicle::RequirementList>(archive, "RequirementList"); }
    template <> chronicle::RequirementList deserialize(const UnserializedObject&) { return chronicle::RequirementList{}; }
    template <> void serialize(Archive& archive, const chronicle::LifeClock& value) { (void)value; serializeEmptyPiece<chronicle::LifeClock>(archive, "LifeClock"); }
    template <> chronicle::LifeClock deserialize(const UnserializedObject&) { return chronicle::LifeClock{}; }
    template <> void serialize(Archive& archive, const chronicle::WindowMeter& value) { (void)value; serializeEmptyPiece<chronicle::WindowMeter>(archive, "WindowMeter"); }
    template <> chronicle::WindowMeter deserialize(const UnserializedObject&) { return chronicle::WindowMeter{}; }
    template <> void serialize(Archive& archive, const chronicle::ResourceLedger& value) { (void)value; serializeEmptyPiece<chronicle::ResourceLedger>(archive, "ResourceLedger"); }
    template <> chronicle::ResourceLedger deserialize(const UnserializedObject&) { return chronicle::ResourceLedger{}; }
    template <> void serialize(Archive& archive, const chronicle::EventLog& value) { (void)value; serializeEmptyPiece<chronicle::EventLog>(archive, "EventLog"); }
    template <> chronicle::EventLog deserialize(const UnserializedObject&) { return chronicle::EventLog{}; }
}

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
        float numberProp(const ElementMap& p, const std::string& key, const ThemeSystem* theme, float fallback)
        {
            auto it = p.find(key);
            if (it == p.end() or it->second.isEmpty())
                return fallback;

            if (it->second.type == UnionType::STRING)
            {
                const std::string s = it->second.get<std::string>();
                if (s.rfind("space-", 0) == 0)
                    return theme->spacing(s);

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

        // A color token. Empty stays empty (some specs use "" for "the default tone").
        std::string colorProp(const ElementMap& p, const std::string& key, const ThemeSystem* theme, const std::string& fallback)
        {
            const std::string value = stringProp(p, key, fallback);
            if (value.empty() or theme->theme().hasColor(value))
                return value;

            LOG_ERROR(DOM, key << ": unknown color token '" << value << "', using '" << fallback << "'");
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
            schema.entries.push_back({"color",    "ink"});
            schema.entries.push_back({"align",    "left"});
            schema.entries.push_back({"overflow", "grow"});
            schema.entries.push_back({"width",    0.0f});
            schema.entries.push_back({"maxLines", 0});
        }

        // Reads a LabelSpec from a prop map; `base` supplies the fallbacks (the schema-merged
        // node props for a Label node, or the defaults for a nested `label` record).
        LabelSpec labelSpecFrom(const ElementMap& p, const ThemeSystem* theme, const LabelSpec& base)
        {
            LabelSpec s = base;
            s.style    = stringProp(p, "style", base.style);
            s.text     = stringProp(p, "text", base.text);
            s.color    = colorProp(p, "color", theme, base.color);
            s.align    = enumProp(p, "align", ALIGN, base.align);
            s.overflow = enumProp(p, "overflow", OVERFLOW, base.overflow);
            s.width    = numberProp(p, "width", theme, base.width);
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

        FactoryResult leaf(EntityRef entity)
        {
            FactoryResult r;
            r.entity = entity;
            return r;
        }

        // The piece struct, attached as a component on the piece's own entity: the state lives on
        // the entity, and a scene reads it with getEntity(name)->get<Panel>(). Returns the entity.
        template <typename Piece>
        EntityRef keep(EntitySystem* ecs, EntityRef entity, Piece&& piece)
        {
            ecs->attachGeneric<Piece>(entity, std::move(piece));
            return entity;
        }

        // The piece on the prefab a helper was called on. The helpers are stateless conveniences
        // over that component; the Prefab knows the ECS, so the setters take no ecs argument.
        template <typename Piece>
        Piece* pieceOf(Prefab* p)
        {
            auto ent = p->ecsRef->getEntity(p->id);

            if (not ent or not ent->has<Piece>())
                return nullptr;

            return ent->get<Piece>().component;
        }

        // ECS instances whose flag components are registered; keeps a repeat registration a no-op.
        std::unordered_set<EntitySystem*>& piecesRegisteredIn()
        {
            static std::unordered_set<EntitySystem*> instances;
            return instances;
        }

        void registerPieceComponents(EntitySystem* ecs)
        {
            if (not piecesRegisteredIn().insert(ecs).second)
                return;

            ecs->registerFlagComponent<Label>();
            ecs->registerFlagComponent<Mark>();
            ecs->registerFlagComponent<MarkedLabel>();
            ecs->registerFlagComponent<Ornament>();
            ecs->registerFlagComponent<Panel>();
            ecs->registerFlagComponent<Button>();
            ecs->registerFlagComponent<Tabs>();
            ecs->registerFlagComponent<Gloss>();
            ecs->registerFlagComponent<ProgressRule>();
            ecs->registerFlagComponent<StatLine>();
            ecs->registerFlagComponent<RequirementList>();
            ecs->registerFlagComponent<LifeClock>();
            ecs->registerFlagComponent<WindowMeter>();
            ecs->registerFlagComponent<ResourceLedger>();
            ecs->registerFlagComponent<EventLog>();
            registerActivityComponents(ecs);   // Guarded: makeActivityRow registers them too
        }

        // ---- kinds ------------------------------------------------------------------------

        void registerLabel(PrefabFactoryRegistry* registry)
        {
            ParamSchema schema;
            labelSchema(schema);
            schema.entries.push_back({"z", 0});

            registry->registerFactory("Label", std::move(schema),
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    auto* theme = ecs->getSystem<ThemeSystem>();
                    Label label = makeLabel(ecs, labelSpecFrom(spec.props, theme, LabelSpec{}));
                    return leaf(keep(ecs, label.entity, std::move(label)));
                }});
        }

        void registerMark(PrefabFactoryRegistry* registry)
        {
            ParamSchema schema;
            schema.entries = {
                {"name",   "seal"},
                {"glyph",  ""},     // wins over `name`: a named node's `name` is its handle
                {"size",   16},
                {"color",  "ink"},
                {"z",      0},
            };

            registry->registerFactory("Mark", std::move(schema),
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    auto* theme = ecs->getSystem<ThemeSystem>();
                    MarkSpec s;
                    s.name   = stringProp(spec.props, "glyph", "");
                    if (s.name.empty())
                        s.name = stringProp(spec.props, "name", "seal");
                    s.size   = markSizeProp(spec.props, "size", s.size);
                    s.color = colorProp(spec.props, "color", theme, s.color);
                    s.z      = getParamInt(spec.props, "z", s.z);

                    Mark mark = makeMark(ecs, s);
                    return leaf(keep(ecs, mark.entity, std::move(mark)));
                }});
        }

        void registerMarkedLabel(PrefabFactoryRegistry* registry)
        {
            ParamSchema schema;
            labelSchema(schema);
            schema.entries.push_back({"mark",        ""});
            schema.entries.push_back({"reserveMark", false});
            schema.entries.push_back({"gap",         -1.0f});
            schema.entries.push_back({"z",           0});

            registry->registerFactory("MarkedLabel", std::move(schema),
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    auto* theme = ecs->getSystem<ThemeSystem>();
                    MarkedLabelSpec s;
                    s.mark        = stringProp(spec.props, "mark", s.mark);
                    s.reserveMark = getParamBool(spec.props, "reserveMark", s.reserveMark);
                    s.gap         = numberProp(spec.props, "gap", theme, s.gap);
                    s.z           = getParamInt(spec.props, "z", s.z);

                    // The label: flat props (`text`, `style`, ...) overridden by a nested `label` map.
                    LabelSpec flat = labelSpecFrom(spec.props, theme, LabelSpec{});
                    flat.z = 0;   // the label's z is the MarkedLabel's business (root z + 1)
                    if (const RecordList* nested = recordsOf(spec, "label"); nested and not nested->empty())
                        s.label = labelSpecFrom(nested->front(), theme, flat);
                    else
                        s.label = flat;

                    MarkedLabel ml = makeMarkedLabel(ecs, s);
                    auto prefab = ml.root->get<Prefab>();
                    prefab->addHelper("setColor", [](Prefab* p, const std::string& token) { if (auto piece = pieceOf<MarkedLabel>(p)) piece->setColor(p->ecsRef, token); });
                    prefab->addHelper("setText", [](Prefab* p, const std::string& text) { if (auto piece = pieceOf<MarkedLabel>(p)) piece->setText(p->ecsRef, text); });
                    prefab->addHelper("setMark", [](Prefab* p, const std::string& name) { if (auto piece = pieceOf<MarkedLabel>(p)) piece->setMark(p->ecsRef, name); });
                    return leaf(keep(ecs, ml.root, std::move(ml)));
                }});
        }

        void registerOrnament(PrefabFactoryRegistry* registry)
        {
            ParamSchema schema;
            schema.entries = {
                {"ornament", "divider"},
                {"weight",   "hair"},
                {"knot",     true},
                {"width",    0.0f},
                {"ground",   "folio"},
                {"color",    ""},
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
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    auto* theme = ecs->getSystem<ThemeSystem>();
                    OrnamentSpec s;
                    s.kind   = enumProp(spec.props, "ornament", KIND, s.kind);
                    s.weight = enumProp(spec.props, "weight", WEIGHT, s.weight);
                    s.knot   = getParamBool(spec.props, "knot", s.knot);
                    s.width  = numberProp(spec.props, "width", theme, s.width);
                    s.ground = colorProp(spec.props, "ground", theme, s.ground);
                    s.color = colorProp(spec.props, "color", theme, s.color);
                    s.corner = enumProp(spec.props, "corner", CORNER, s.corner);
                    s.letter = stringProp(spec.props, "letter", s.letter);
                    s.tone   = enumProp(spec.props, "tone", TONE, s.tone);
                    s.z      = getParamInt(spec.props, "z", s.z);

                    Ornament o = makeOrnament(ecs, s);
                    auto prefab = o.root->get<Prefab>();
                    prefab->addHelper("setColor", [](Prefab* p, const std::string& token) { if (auto piece = pieceOf<Ornament>(p)) piece->setColor(p->ecsRef, token); });
                    prefab->addHelper("setLetter", [](Prefab* p, const std::string& letter) { if (auto piece = pieceOf<Ornament>(p)) piece->setLetter(p->ecsRef, letter); });
                    return leaf(keep(ecs, o.root, std::move(o)));
                }});
        }

        void registerPanel(PrefabFactoryRegistry* registry)
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
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    auto* theme = ecs->getSystem<ThemeSystem>();
                    PanelSpec s;
                    s.frame    = enumProp(spec.props, "frame", FRAME, s.frame);
                    s.width    = numberProp(spec.props, "width", theme, s.width);
                    s.heading  = stringProp(spec.props, "heading", s.heading);
                    s.glyph    = stringProp(spec.props, "glyph", s.glyph);
                    s.aside    = stringProp(spec.props, "aside", s.aside);
                    s.z        = getParamInt(spec.props, "z", s.z);
                    const int contentZ = getParamInt(spec.props, "contentZ", -1);
                    s.contentZ = contentZ < 0 ? s.z + 10 : contentZ;

                    Panel panel = makePanel(ecs, s);
                    auto prefab = panel.root->get<Prefab>();
                    prefab->addHelper("setHeading", [](Prefab* p, const std::string& text) { if (auto piece = pieceOf<Panel>(p)) piece->setHeading(p->ecsRef, text); });
                    prefab->addHelper("setAside", [](Prefab* p, const std::string& text) { if (auto piece = pieceOf<Panel>(p)) piece->setAside(p->ecsRef, text); });
                    prefab->addHelper("setWidth", [](Prefab* p, float width) { if (auto piece = pieceOf<Panel>(p)) piece->setWidth(p->ecsRef, width); });

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
                    keep(ecs, panel.root, std::move(panel));
                    return r;
                }});
        }

        void registerButton(PrefabFactoryRegistry* registry)
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
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    auto* theme = ecs->getSystem<ThemeSystem>();
                    ButtonSpec s;
                    s.variant  = enumProp(spec.props, "variant", VARIANT, s.variant);
                    s.label    = stringProp(spec.props, "label", s.label);
                    s.glyph    = stringProp(spec.props, "glyph", s.glyph);
                    s.months   = getParamInt(spec.props, "months", s.months);
                    s.disabled = getParamBool(spec.props, "disabled", s.disabled);
                    s.reason   = stringProp(spec.props, "reason", s.reason);
                    s.tag      = stringProp(spec.props, "tag", s.tag);
                    s.z        = getParamInt(spec.props, "z", s.z);

                    Button b = makeButton(ecs, s);
                    auto prefab = b.root->get<Prefab>();
                    prefab->addHelper("setDisabled", [](Prefab* p, bool disabled) { if (auto piece = pieceOf<Button>(p)) piece->setDisabled(p->ecsRef, disabled); });
                    prefab->addHelper("setDisabledReason", [](Prefab* p, bool disabled, const std::string& reason) { if (auto piece = pieceOf<Button>(p)) piece->setDisabled(p->ecsRef, disabled, reason); });
                    prefab->addHelper("setLabel", [](Prefab* p, const std::string& text) { if (auto piece = pieceOf<Button>(p)) piece->setLabel(p->ecsRef, text); });
                    return leaf(keep(ecs, b.root, std::move(b)));
                }});
        }

        void registerTabs(PrefabFactoryRegistry* registry)
        {
            ParamSchema schema;
            schema.entries = {
                {"active", 0},
                {"width",  0.0f},
                {"tag",    ""},
                {"z",      20},
            };

            registry->registerFactory("Tabs", std::move(schema),
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    auto* theme = ecs->getSystem<ThemeSystem>();
                    TabsSpec s;
                    s.active = getParamInt(spec.props, "active", s.active);
                    s.width  = numberProp(spec.props, "width", theme, s.width);
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

                    Tabs t = makeTabs(ecs, s);
                    auto prefab = t.root->get<Prefab>();
                    prefab->addHelper("setActive", [](Prefab* p, int index) { if (auto piece = pieceOf<Tabs>(p)) piece->setActive(p->ecsRef, index); });
                    prefab->addHelper("setBadge", [](Prefab* p, int index, int count) { if (auto piece = pieceOf<Tabs>(p)) piece->setBadge(p->ecsRef, index, count); });
                    return leaf(keep(ecs, t.root, std::move(t)));
                }});
        }

        void registerGloss(PrefabFactoryRegistry* registry)
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
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    auto* theme = ecs->getSystem<ThemeSystem>();
                    GlossSpec s;
                    s.kind     = enumProp(spec.props, "gloss", KIND, s.kind);
                    s.title    = stringProp(spec.props, "title", s.title);
                    s.text     = stringProp(spec.props, "text", s.text);
                    s.footnote = stringProp(spec.props, "footnote", s.footnote);
                    s.width    = numberProp(spec.props, "width", theme, s.width);
                    s.z        = getParamInt(spec.props, "z", s.z);

                    if (const RecordList* rows = recordsOf(spec, "rows"))
                    {
                        for (const auto& rec : *rows)
                            s.rows.push_back({stringProp(rec, "label"), stringProp(rec, "value")});
                    }

                    Gloss g = makeGloss(ecs, s);
                    auto prefab = g.root->get<Prefab>();
                    prefab->addHelper("setText", [](Prefab* p, const std::string& text) { if (auto piece = pieceOf<Gloss>(p)) piece->setText(p->ecsRef, text); });
                    return leaf(keep(ecs, g.root, std::move(g)));
                }});
        }

        void registerProgressRule(PrefabFactoryRegistry* registry)
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
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    auto* theme = ecs->getSystem<ThemeSystem>();
                    ProgressRuleSpec s;
                    s.width           = numberProp(spec.props, "width", theme, s.width);
                    s.small           = getParamBool(spec.props, "small", s.small);
                    s.percent         = getParamFloat(spec.props, "percent", s.percent);
                    s.forecastPercent = getParamFloat(spec.props, "forecast", s.forecastPercent);
                    s.nib             = getParamBool(spec.props, "nib", s.nib);
                    s.caption         = stringProp(spec.props, "caption", s.caption);
                    s.trackHeight     = numberProp(spec.props, "trackHeight", theme, s.trackHeight);
                    s.z               = getParamInt(spec.props, "z", s.z);

                    ProgressRule pr = makeProgressRule(ecs, s);
                    auto prefab = pr.root->get<Prefab>();
                    prefab->addHelper("setPercent", [](Prefab* p, float percent, bool animate) { if (auto piece = pieceOf<ProgressRule>(p)) piece->setPercent(p->ecsRef, percent, animate); });
                    prefab->addHelper("setForecast", [](Prefab* p, float percent) { if (auto piece = pieceOf<ProgressRule>(p)) piece->setForecast(p->ecsRef, percent); });
                    prefab->addHelper("setCaption", [](Prefab* p, const std::string& text) { if (auto piece = pieceOf<ProgressRule>(p)) piece->setCaption(p->ecsRef, text); });
                    prefab->addHelper("setNib", [](Prefab* p, bool on) { if (auto piece = pieceOf<ProgressRule>(p)) piece->setNib(p->ecsRef, on); });
                    prefab->addHelper("setWidth", [](Prefab* p, float width) { if (auto piece = pieceOf<ProgressRule>(p)) piece->setWidth(p->ecsRef, width); });
                    return leaf(keep(ecs, pr.root, std::move(pr)));
                }});
        }

        void registerStatLine(PrefabFactoryRegistry* registry)
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
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    auto* theme = ecs->getSystem<ThemeSystem>();
                    StatLineSpec s;
                    s.width     = numberProp(spec.props, "width", theme, s.width);
                    s.label     = stringProp(spec.props, "label", s.label);
                    s.glyph     = stringProp(spec.props, "glyph", s.glyph);
                    s.value     = getParamInt(spec.props, "value", s.value);
                    s.max       = getParamInt(spec.props, "max", s.max);
                    s.projected = getParamInt(spec.props, "projected", s.projected);
                    s.threshold = getParamInt(spec.props, "threshold", s.threshold);
                    s.note      = stringProp(spec.props, "note", s.note);
                    s.glossKey  = stringProp(spec.props, "glossKey", s.glossKey);
                    s.z         = getParamInt(spec.props, "z", s.z);

                    StatLine sl = makeStatLine(ecs, s);
                    auto prefab = sl.root->get<Prefab>();
                    prefab->addHelper("setValue", [](Prefab* p, int value, bool animate) { if (auto piece = pieceOf<StatLine>(p)) piece->setValue(p->ecsRef, value, animate); });
                    prefab->addHelper("setProjected", [](Prefab* p, int projected) { if (auto piece = pieceOf<StatLine>(p)) piece->setProjected(p->ecsRef, projected); });
                    prefab->addHelper("setThreshold", [](Prefab* p, int threshold) { if (auto piece = pieceOf<StatLine>(p)) piece->setThreshold(p->ecsRef, threshold); });
                    prefab->addHelper("setNote", [](Prefab* p, const std::string& text) { if (auto piece = pieceOf<StatLine>(p)) piece->setNote(p->ecsRef, text); });
                    return leaf(keep(ecs, sl.root, std::move(sl)));
                }});
        }

        void registerRequirementList(PrefabFactoryRegistry* registry)
        {
            ParamSchema schema;
            schema.entries = {
                {"width", 256.0f},
                {"dense", false},
                {"z",     20},
            };

            registry->registerFactory("RequirementList", std::move(schema),
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    auto* theme = ecs->getSystem<ThemeSystem>();
                    RequirementListSpec s;
                    s.width = numberProp(spec.props, "width", theme, s.width);
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

                    RequirementList rl = makeRequirementList(ecs, s);
                    auto prefab = rl.root->get<Prefab>();
                    prefab->addHelper("setItems", [](Prefab* p, const std::vector<Requirement>& items) { if (auto piece = pieceOf<RequirementList>(p)) piece->setItems(p->ecsRef, items); });
                    prefab->addHelper("setItem", [](Prefab* p, size_t index, int current, int needed) { if (auto piece = pieceOf<RequirementList>(p)) piece->setItem(p->ecsRef, index, current, needed); });
                    prefab->addHelper("setMet", [](Prefab* p, size_t index, bool met) { if (auto piece = pieceOf<RequirementList>(p)) piece->setMet(p->ecsRef, index, met); });
                    prefab->addHelper("clearMet", [](Prefab* p, size_t index) { if (auto piece = pieceOf<RequirementList>(p)) piece->clearMet(p->ecsRef, index); });
                    prefab->addHelper("size", [](Prefab* p) -> size_t { auto piece = pieceOf<RequirementList>(p); return piece ? piece->size() : 0; });
                    return leaf(keep(ecs, rl.root, std::move(rl)));
                }});
        }

        void registerLifeClock(PrefabFactoryRegistry* registry)
        {
            ParamSchema schema;
            schema.entries = {
                {"width",         640.0f},
                {"startAge",      7.0f},
                {"endAge",        43.0f},
                {"age",           7.0f},
                {"runningMonths", 0.0f},
                {"nextLabel",     ""},
                {"nextIn",        -1},
                {"z",             20},
            };

            registry->registerFactory("LifeClock", std::move(schema),
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    auto theme = ecs->getSystem<ThemeSystem>();
                    LifeClockSpec s;
                    s.width         = numberProp(spec.props, "width", theme, s.width);
                    s.startAge      = getParamFloat(spec.props, "startAge", s.startAge);
                    s.endAge        = getParamFloat(spec.props, "endAge", s.endAge);
                    s.age           = getParamFloat(spec.props, "age", s.age);
                    s.runningMonths = getParamFloat(spec.props, "runningMonths", s.runningMonths);
                    s.nextLabel     = stringProp(spec.props, "nextLabel", s.nextLabel);
                    s.nextIn        = getParamInt(spec.props, "nextIn", s.nextIn);
                    s.z             = getParamInt(spec.props, "z", s.z);

                    if (const RecordList* milestones = recordsOf(spec, "milestones"))
                    {
                        for (const auto& rec : *milestones)
                            s.milestones.push_back({getParamFloat(rec, "age", 0.0f), stringProp(rec, "label")});
                    }

                    if (const RecordList* windows = recordsOf(spec, "windows"))
                    {
                        for (const auto& rec : *windows)
                            s.windows.push_back({getParamFloat(rec, "from", 0.0f), getParamFloat(rec, "to", 0.0f), stringProp(rec, "label"), getParamBool(rec, "closed", false)});
                    }

                    LifeClock clock = makeLifeClock(ecs, s);
                    auto prefab = clock.root->get<Prefab>();
                    prefab->addHelper("setAge", [](Prefab* p, float age, bool animate) { if (auto piece = pieceOf<LifeClock>(p)) piece->setAge(p->ecsRef, age, animate); });
                    prefab->addHelper("setRunning", [](Prefab* p, float months) { if (auto piece = pieceOf<LifeClock>(p)) piece->setRunning(p->ecsRef, months); });
                    prefab->addHelper("setNext", [](Prefab* p, const std::string& label, int months) { if (auto piece = pieceOf<LifeClock>(p)) piece->setNext(p->ecsRef, label, months); });
                    prefab->addHelper("setWindows", [](Prefab* p, const std::vector<ClockWindow>& windows) { if (auto piece = pieceOf<LifeClock>(p)) piece->setWindows(p->ecsRef, windows); });
                    prefab->addHelper("setWindowClosed", [](Prefab* p, size_t index, bool closed) { if (auto piece = pieceOf<LifeClock>(p)) piece->setWindowClosed(p->ecsRef, index, closed); });
                    prefab->addHelper("setMilestones", [](Prefab* p, const std::vector<ClockMilestone>& milestones) { if (auto piece = pieceOf<LifeClock>(p)) piece->setMilestones(p->ecsRef, milestones); });
                    return leaf(keep(ecs, clock.root, std::move(clock)));
                }});
        }

        void registerActivityRow(PrefabFactoryRegistry* registry)
        {
            ParamSchema schema;
            schema.entries = {
                {"id",       UnionType::STRING, Req::Required},
                {"label",    ""},       // the display name: `name` is the node's handle
                {"glyph",    "training"},
                {"rank",     ""},
                {"count",    ""},
                {"months",   1},
                {"each",     ""},
                {"percent",  0.0f},
                {"caption",  ""},
                {"until",    ""},
                {"urgent",   false},
                {"compact",  false},
                {"state",    "idle"},
                {"stripe",   false},
                {"glossKey", ""},
                {"width",    620.0f},
                {"z",        20},
            };

            static const std::vector<std::pair<std::string, ActivityState>> STATE = {
                {"idle", ActivityState::Idle}, {"running", ActivityState::Running}, {"locked", ActivityState::Locked},
            };

            registry->registerFactory("ActivityRow", std::move(schema),
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    auto theme = ecs->getSystem<ThemeSystem>();
                    ActivityRowSpec s;
                    s.id       = stringProp(spec.props, "id", s.id);
                    s.name     = stringProp(spec.props, "label", s.name);
                    s.glyph    = stringProp(spec.props, "glyph", s.glyph);
                    s.rank     = stringProp(spec.props, "rank", s.rank);
                    s.count    = stringProp(spec.props, "count", s.count);
                    s.months   = getParamInt(spec.props, "months", s.months);
                    s.each     = stringProp(spec.props, "each", s.each);
                    s.percent  = getParamFloat(spec.props, "percent", s.percent);
                    s.caption  = stringProp(spec.props, "caption", s.caption);
                    s.until    = stringProp(spec.props, "until", s.until);
                    s.urgent   = getParamBool(spec.props, "urgent", s.urgent);
                    s.compact  = getParamBool(spec.props, "compact", s.compact);
                    s.state    = enumProp(spec.props, "state", STATE, s.state);
                    s.stripe   = getParamBool(spec.props, "stripe", s.stripe);
                    s.glossKey = stringProp(spec.props, "glossKey", s.glossKey);
                    s.width    = numberProp(spec.props, "width", theme, s.width);
                    s.z        = getParamInt(spec.props, "z", s.z);

                    if (const RecordList* gains = recordsOf(spec, "gains"))
                    {
                        for (const auto& rec : *gains)
                            s.gains.push_back({stringProp(rec, "stat"), getParamInt(rec, "amount", 0)});
                    }

                    if (const RecordList* items = recordsOf(spec, "requirements"))
                    {
                        for (const auto& rec : *items)
                        {
                            Requirement req;
                            req.label   = stringProp(rec, "label");
                            req.current = getParamInt(rec, "current", -1);
                            req.needed  = getParamInt(rec, "needed", 0);
                            req.met     = metProp(rec);
                            s.requirements.push_back(std::move(req));
                        }
                    }

                    // The row attaches itself to its root; nothing to keep here.
                    ActivityRow row = makeActivityRow(ecs, s);
                    if (row.root.empty())
                        return FactoryResult{};

                    auto prefab = row.root->get<Prefab>();
                    prefab->addHelper("setState", [](Prefab* p, ActivityState state) { if (auto piece = pieceOf<ActivityRow>(p)) piece->setState(p->ecsRef, state); });
                    prefab->addHelper("setPercent", [](Prefab* p, float percent, bool animate) { if (auto piece = pieceOf<ActivityRow>(p)) piece->setPercent(p->ecsRef, percent, animate); });
                    prefab->addHelper("setCaption", [](Prefab* p, const std::string& text) { if (auto piece = pieceOf<ActivityRow>(p)) piece->setCaption(p->ecsRef, text); });
                    prefab->addHelper("setRequirement", [](Prefab* p, size_t index, int current, int needed) { if (auto piece = pieceOf<ActivityRow>(p)) piece->setRequirement(p->ecsRef, index, current, needed); });
                    prefab->addHelper("setMonths", [](Prefab* p, int months) { if (auto piece = pieceOf<ActivityRow>(p)) piece->setMonths(p->ecsRef, months); });
                    return leaf(row.root);
                }});
        }

        void registerActivityList(PrefabFactoryRegistry* registry)
        {
            ParamSchema schema;
            schema.entries = {
                {"id",     "activities"},
                {"width",  620.0f},
                {"height", 0.0f},      // > 0: the list keeps that height and its rows scroll
                {"stripes", true},     // false: every row on the same ground
                {"compact", false},    // true: rows of a name, a time and a closing, on a ground that says their kind
                {"tileWidth", 0.0f},   // > 0: the rows given by setRows are tiles, as many to a line as fit at that width or more
                {"z",      20},
            };

            registry->registerFactory("ActivityList", std::move(schema),
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    auto theme = ecs->getSystem<ThemeSystem>();
                    ActivityListSpec s;
                    s.id     = stringProp(spec.props, "id", s.id);
                    s.width  = numberProp(spec.props, "width", theme, s.width);
                    s.height = numberProp(spec.props, "height", theme, s.height);
                    s.stripes = getParamBool(spec.props, "stripes", s.stripes);
                    s.compact = getParamBool(spec.props, "compact", s.compact);
                    s.tileWidth = numberProp(spec.props, "tileWidth", theme, s.tileWidth);
                    s.z      = getParamInt(spec.props, "z", s.z);

                    ActivityList list = makeActivityList(ecs, s);
                    if (list.root.empty())
                        return FactoryResult{};

                    auto prefab = list.root->get<Prefab>();
                    prefab->addHelper("select", [](Prefab* p, const std::string& id) { if (auto piece = pieceOf<ActivityList>(p)) piece->select(p->ecsRef, id); });
                    prefab->addHelper("setRowState", [](Prefab* p, const std::string& id, ActivityState state) { if (auto piece = pieceOf<ActivityList>(p)) piece->setRowState(p->ecsRef, id, state); });

                    // Headings and rows go into the list's body, at its width and in its band;
                    // the list finds them there on the next frame (stripes, last rule, owner).
                    FactoryResult r;
                    r.entity = list.root;
                    r.slot   = list.body;
                    r.childDefaults = {
                        {"width",   ElementType{s.width}},
                        {"z",       ElementType{s.z}},
                        {"compact", ElementType{s.compact}},
                    };
                    keep(ecs, list.root, std::move(list));
                    return r;
                }});
        }

        void registerActivityGroup(PrefabFactoryRegistry* registry)
        {
            ParamSchema schema;
            schema.entries = {
                {"label", ""},
                {"width", 620.0f},
                {"z",     20},
            };

            // A group is its heading: the rows that follow it in the list, up to the next
            // heading, are its rows. It takes no children (a list stacks one level only).
            registry->registerFactory("ActivityGroup", std::move(schema),
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    auto theme = ecs->getSystem<ThemeSystem>();
                    const std::string label = stringProp(spec.props, "label");
                    const float width = numberProp(spec.props, "width", theme, 620.0f);
                    const int z = getParamInt(spec.props, "z", 20);

                    return leaf(makeActivityHeading(ecs, label, width, z));
                }});
        }

        void registerWindowMeter(PrefabFactoryRegistry* registry)
        {
            ParamSchema schema;
            schema.entries = {
                {"label",    "Squire"},   // the display name: `name` is the node's handle
                {"from",     16.0f},
                {"to",       22.0f},
                {"age",      7.0f},
                {"state",    "open"},
                {"note",     ""},
                {"glossKey", ""},
                {"width",    288.0f},
                {"z",        20},
            };

            static const std::vector<std::pair<std::string, WindowState>> STATE = {
                {"upcoming", WindowState::Upcoming}, {"open", WindowState::Open}, {"closed", WindowState::Closed},
            };

            registry->registerFactory("WindowMeter", std::move(schema),
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    auto theme = ecs->getSystem<ThemeSystem>();
                    WindowMeterSpec s;
                    s.name     = stringProp(spec.props, "label", s.name);
                    s.from     = getParamFloat(spec.props, "from", s.from);
                    s.to       = getParamFloat(spec.props, "to", s.to);
                    s.age      = getParamFloat(spec.props, "age", s.age);
                    s.state    = enumProp(spec.props, "state", STATE, s.state);
                    s.note     = stringProp(spec.props, "note", s.note);
                    s.glossKey = stringProp(spec.props, "glossKey", s.glossKey);
                    s.width    = numberProp(spec.props, "width", theme, s.width);
                    s.z        = getParamInt(spec.props, "z", s.z);

                    WindowMeter meter = makeWindowMeter(ecs, s);
                    auto prefab = meter.root->get<Prefab>();
                    prefab->addHelper("setAge", [](Prefab* p, float age, bool animate) { if (auto piece = pieceOf<WindowMeter>(p)) piece->setAge(p->ecsRef, age, animate); });
                    prefab->addHelper("setState", [](Prefab* p, WindowState state) { if (auto piece = pieceOf<WindowMeter>(p)) piece->setState(p->ecsRef, state); });
                    prefab->addHelper("setNote", [](Prefab* p, const std::string& text) { if (auto piece = pieceOf<WindowMeter>(p)) piece->setNote(p->ecsRef, text); });
                    prefab->addHelper("setRange", [](Prefab* p, float from, float to) { if (auto piece = pieceOf<WindowMeter>(p)) piece->setRange(p->ecsRef, from, to); });
                    return leaf(keep(ecs, meter.root, std::move(meter)));
                }});
        }

        // ---- the ledger's build context ------------------------------------------------------
        //
        // A ledger's groups hold rows, which a flat record list cannot say, so in a file they are
        // child nodes: ResourceLedger > LedgerGroup > LedgerRow. A factory never sees its node's
        // children, but the builder realises a node and then its children, depth first and in
        // order, so the ledger a LedgerGroup or LedgerRow belongs to is the last one realised in
        // that ECS. The ledger hands its root id down (childDefaults `ledger`) and a group hands
        // its id (`group`): the child kinds build nothing of their own, they add to that ledger.

        std::unordered_map<EntitySystem*, EntityRef>& openLedgers()
        {
            static std::unordered_map<EntitySystem*, EntityRef> ledgers;
            return ledgers;
        }

        _unique_id idProp(const ElementMap& p, const std::string& key)
        {
            auto it = p.find(key);
            if (it == p.end() or it->second.isEmpty())
                return 0;

            if (it->second.type == UnionType::SIZE_T)
                return static_cast<_unique_id>(it->second.get<size_t>());

            return static_cast<_unique_id>(std::max(0, getParamInt(p, key, 0)));
        }

        // The ledger a child node was built under; nullptr (logged) outside one.
        ResourceLedger* enclosingLedger(EntitySystem* ecs, const NodeSpec& spec)
        {
            const _unique_id id = idProp(spec.props, "ledger");
            auto it = openLedgers().find(ecs);

            if (id == 0 or it == openLedgers().end() or it->second.id != id)
            {
                LOG_ERROR(DOM, spec.kind << " '" << stringProp(spec.props, "id") << "' is not inside a ResourceLedger; ignored");
                return nullptr;
            }

            EntityRef root = it->second;

            if (not root->has<ResourceLedger>())
                return nullptr;

            return root->get<ResourceLedger>().component;
        }

        void registerResourceLedger(PrefabFactoryRegistry* registry)
        {
            ParamSchema schema;
            schema.entries = {
                {"width", 288.0f},
                {"z",     20},
            };

            registry->registerFactory("ResourceLedger", std::move(schema),
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    auto theme = ecs->getSystem<ThemeSystem>();
                    ResourceLedgerSpec s;
                    s.width = numberProp(spec.props, "width", theme, s.width);
                    s.z     = getParamInt(spec.props, "z", s.z);

                    ResourceLedger ledger = makeResourceLedger(ecs, s);
                    auto prefab = ledger.root->get<Prefab>();
                    prefab->addHelper("setValue", [](Prefab* p, const std::string& id, const std::string& value) { if (auto piece = pieceOf<ResourceLedger>(p)) piece->setValue(p->ecsRef, id, value); });
                    prefab->addHelper("setRate", [](Prefab* p, const std::string& id, const std::string& rate) { if (auto piece = pieceOf<ResourceLedger>(p)) piece->setRate(p->ecsRef, id, rate); });
                    prefab->addHelper("setMuted", [](Prefab* p, const std::string& id, bool muted) { if (auto piece = pieceOf<ResourceLedger>(p)) piece->setMuted(p->ecsRef, id, muted); });
                    prefab->addHelper("addRow", [](Prefab* p, const std::string& group, const LedgerRowSpec& row) { if (auto piece = pieceOf<ResourceLedger>(p)) piece->addRow(p->ecsRef, group, row); });
                    prefab->addHelper("removeRow", [](Prefab* p, const std::string& id) { if (auto piece = pieceOf<ResourceLedger>(p)) piece->removeRow(p->ecsRef, id); });

                    // The groups and rows under this node are realised next, into this ledger.
                    openLedgers()[ecs] = ledger.root;

                    FactoryResult r;
                    r.entity = ledger.root;
                    r.childDefaults = {
                        {"ledger", ElementType{static_cast<size_t>(ledger.root.id)}},
                    };
                    keep(ecs, ledger.root, std::move(ledger));
                    return r;
                }});
        }

        void registerLedgerGroup(PrefabFactoryRegistry* registry)
        {
            ParamSchema schema;
            schema.entries = {
                {"id",    UnionType::STRING, Req::Required},
                {"label", ""},
            };

            registry->registerFactory("LedgerGroup", std::move(schema),
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    const std::string id = stringProp(spec.props, "id");

                    if (auto ledger = enclosingLedger(ecs, spec))
                        ledger->addGroup(ecs, id, stringProp(spec.props, "label"));

                    // Nothing of its own: the heading is the ledger's. The rows learn their group.
                    FactoryResult r;
                    r.childDefaults = {
                        {"group", ElementType{id}},
                    };
                    return r;
                }});
        }

        void registerLedgerRow(PrefabFactoryRegistry* registry)
        {
            ParamSchema schema;
            schema.entries = {
                {"id",       UnionType::STRING, Req::Required},
                {"glyph",    "gold"},
                {"label",    ""},       // the display name: `name` is the node's handle
                {"value",    "0"},
                {"rate",     ""},
                {"tone",     "none"},
                {"muted",    false},
                {"glossKey", ""},
            };

            static const std::vector<std::pair<std::string, LedgerTone>> TONE = {
                {"none", LedgerTone::None}, {"coin", LedgerTone::Coin}, {"guild", LedgerTone::Guild}, {"relic", LedgerTone::Relic},
            };

            registry->registerFactory("LedgerRow", std::move(schema),
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    LedgerRowSpec s;
                    s.id       = stringProp(spec.props, "id");
                    s.glyph    = stringProp(spec.props, "glyph", s.glyph);
                    s.name     = stringProp(spec.props, "label", s.name);
                    s.value    = stringProp(spec.props, "value", s.value);
                    s.rate     = stringProp(spec.props, "rate", s.rate);
                    s.tone     = enumProp(spec.props, "tone", TONE, s.tone);
                    s.muted    = getParamBool(spec.props, "muted", s.muted);
                    s.glossKey = stringProp(spec.props, "glossKey", s.glossKey);

                    const std::string group = stringProp(spec.props, "group");

                    if (group.empty())
                    {
                        LOG_ERROR(DOM, "LedgerRow '" << s.id << "' is not inside a LedgerGroup; ignored");
                        return FactoryResult{};
                    }

                    if (auto ledger = enclosingLedger(ecs, spec))
                        ledger->addRow(ecs, group, s);

                    return FactoryResult{};
                }});
        }

        void registerEventLog(PrefabFactoryRegistry* registry)
        {
            ParamSchema schema;
            schema.entries = {
                {"width",      300.0f},
                {"height",     560.0f},
                {"yearPrefix", "IN HIS "},
                {"years",      true},
                {"footnote",   ""},
                {"z",          20},
            };

            static const std::vector<std::pair<std::string, LogKind>> KIND = {
                {"note", LogKind::Note}, {"gain", LogKind::Gain}, {"loss", LogKind::Loss},
                {"coin", LogKind::Coin}, {"milestone", LogKind::Milestone},
            };

            registry->registerFactory("EventLog", std::move(schema),
                PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult
                {
                    auto theme = ecs->getSystem<ThemeSystem>();
                    EventLogSpec s;
                    s.width      = numberProp(spec.props, "width", theme, s.width);
                    s.height     = numberProp(spec.props, "height", theme, s.height);
                    s.yearPrefix = stringProp(spec.props, "yearPrefix", s.yearPrefix);
                    s.years      = getParamBool(spec.props, "years", s.years);
                    s.footnote   = stringProp(spec.props, "footnote", s.footnote);
                    s.z          = getParamInt(spec.props, "z", s.z);

                    if (const RecordList* entries = recordsOf(spec, "entries"))
                    {
                        for (const auto& rec : *entries)
                        {
                            LogEntry entry;
                            entry.age    = getParamFloat(rec, "age", entry.age);
                            entry.text   = stringProp(rec, "text");
                            entry.kind   = enumProp(rec, "kind", KIND, entry.kind);
                            entry.figure = stringProp(rec, "figure");
                            entry.glyph  = stringProp(rec, "glyph");
                            s.entries.push_back(std::move(entry));
                        }
                    }

                    EventLog log = makeEventLog(ecs, s);
                    auto prefab = log.root->get<Prefab>();
                    prefab->addHelper("append", [](Prefab* p, const LogEntry& entry) { if (auto piece = pieceOf<EventLog>(p)) piece->append(p->ecsRef, entry); });
                    prefab->addHelper("clear", [](Prefab* p) { if (auto piece = pieceOf<EventLog>(p)) piece->clear(p->ecsRef); });
                    prefab->addHelper("setFootnote", [](Prefab* p, const std::string& text) { if (auto piece = pieceOf<EventLog>(p)) piece->setFootnote(p->ecsRef, text); });
                    prefab->addHelper("scrollToEnd", [](Prefab* p) { if (auto piece = pieceOf<EventLog>(p)) piece->scrollToEnd(p->ecsRef); });
                    return leaf(keep(ecs, log.root, std::move(log)));
                }});
        }
    }

    const std::vector<std::string>& chronicleKinds()
    {
        static const std::vector<std::string> kinds = {
            "Label", "Mark", "MarkedLabel", "Ornament", "Panel", "Button",
            "Tabs", "Gloss", "ProgressRule", "StatLine", "RequirementList", "LifeClock",
            "ActivityRow", "ActivityList", "ActivityGroup", "WindowMeter",
            "ResourceLedger", "LedgerGroup", "LedgerRow", "EventLog",
        };

        return kinds;
    }

    void registerChronicleFactories(PrefabFactoryRegistry* registry)
    {
        if (not registry)
        {
            LOG_ERROR(DOM, "registerChronicleFactories needs a registry");
            return;
        }

        // The pieces are attached as components; their storage exists before the first build.
        registerPieceComponents(registry->ecsRef);

        registerLabel(registry);
        registerMark(registry);
        registerMarkedLabel(registry);
        registerOrnament(registry);
        registerPanel(registry);
        registerButton(registry);
        registerTabs(registry);
        registerGloss(registry);
        registerProgressRule(registry);
        registerStatLine(registry);
        registerRequirementList(registry);
        registerLifeClock(registry);
        registerActivityRow(registry);
        registerActivityList(registry);
        registerActivityGroup(registry);
        registerWindowMeter(registry);
        registerResourceLedger(registry);
        registerLedgerGroup(registry);
        registerLedgerRow(registry);
        registerEventLog(registry);
    }
}
