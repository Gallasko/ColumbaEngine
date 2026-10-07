#include "ledgergallery.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <cstdlib>
#include <string>
#include <vector>

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_fwd.h"
#include "Input/sdlevents.h"
#include "2D/position.h"
#include "UI/prefab.h"
#include "UI/prefabloader.h"
#include "UI/prefabbuilder.h"
#include "Systems/gamefacts.h"

#include "UI/resourceledger.h"
#include "UI/button.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char* File = "res/chronicle/ui/ledgergallery.yaml";

        constexpr const char* CoinPath = "resources.coin.value";
        constexpr const char* RationsPath = "resources.rations.value";
        constexpr const char* TimberPath = "resources.timber.value";
        constexpr const char* RelicPath = "resources.relic.sunstone.held";

        const char* const LedgerNames[] = {"ledger", "ledgerWide"};

        const std::string Minus = "\xE2\x88\x92";   // U+2212

        constexpr int StartCoin = 412;
        constexpr int StartRations = 18;
        constexpr int FirstTimber = 14;
        constexpr int TimberFind = 3;

        // What rules/ will hand the scene: figures grouped by threes with a space, a real minus.
        std::string figure(int value)
        {
            std::string digits = std::to_string(std::abs(value));

            for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3)
                digits.insert(static_cast<size_t>(i), " ");

            return (value < 0 ? Minus : "") + digits;
        }

        ResourceLedger* ledgerOf(EntitySystem* ecs, _unique_id id)
        {
            auto entity = ecs->getEntity(id);

            if (not entity or not entity->has<ResourceLedger>())
                return nullptr;

            return entity->get<ResourceLedger>().component;
        }

        LedgerRowSpec timberRow(const std::string& value)
        {
            LedgerRowSpec row;
            row.id = "timber";
            row.glyph = "work";
            row.name = "Timber";
            row.value = value;
            row.rate = "+3 / mo";

            return row;
        }

        LedgerRowSpec relicRow()
        {
            LedgerRowSpec row;
            row.id = "relic.sunstone";
            row.glyph = "relic";
            row.name = "Sunstone relic";
            row.value = "1 of 3";
            row.tone = LedgerTone::Relic;

            return row;
        }
    }

    void LedgerGallery::init()
    {
        theme = ecsRef->getSystem<ThemeSystem>();

        auto caption = [&](float x, float y, const std::string& text, const std::string& color) {
            LabelSpec spec;
            spec.style = "caption";
            spec.text = text;
            spec.color = color;
            spec.z = 15;

            Label label = makeLabel(ecsRef, spec);
            auto p = label.entity->get<PositionComponent>();
            p->setX(x);
            p->setY(y);

            return label;
        };

        const float margin = theme->space(7);   // 48

        std::vector<std::string> errors;
        PrefabLoadOptions options;
        options.errors = &errors;

        auto spec = loadNodeSpec(ecsRef, File, options);

        if (not spec)
        {
            caption(margin, 68.0f, std::string("could not load ") + File + " (see the log)", "status-loss");
            return;
        }

        EntityRef page = buildTree(ecsRef, *spec);

        if (page.empty())
        {
            caption(margin, 68.0f, std::string("could not build ") + File + " (see the log)", "status-loss");
            return;
        }

        if (not errors.empty())
            caption(margin, 68.0f, std::to_string(errors.size()) + " problem(s) in the file, see the log", "status-loss");

        auto prefab = page->get<Prefab>();
        backgroundId = prefab->getEntity("page").id;

        for (const char* name : LedgerNames)
            ledgers.push_back(prefab->findEntity(name).id);

        echo = caption(960.0f, 344.0f, "ready", "ink-muted");

        // Listen, then seed the facts: the buttons write facts, the ledgers follow them.
        listenToEvent<WorldFactsUpdate>([this](const WorldFactsUpdate& event) {
            for (const auto& name : event.changedFacts)
            {
                const auto& it = event.factMap->find(name);

                if (it == event.factMap->end())
                    continue;

                for (auto id : ledgers)
                {
                    auto ledger = ledgerOf(ecsRef, id);

                    if (not ledger)
                        continue;

                    if (name == CoinPath)
                    {
                        ledger->setValue(ecsRef, "coin", it->second.get<std::string>());
                    }
                    else if (name == RationsPath)
                    {
                        ledger->setValue(ecsRef, "rations", it->second.get<std::string>());
                    }
                    else if (name == TimberPath)
                    {
                        const std::string value = it->second.get<std::string>();

                        // Never earned: no row. Earned once: the row stays for the life.
                        if (value.empty())
                        {
                            if (ledger->row("timber"))
                                ledger->removeRow(ecsRef, "timber");
                        }
                        else if (ledger->row("timber"))
                        {
                            ledger->setValue(ecsRef, "timber", value);
                        }
                        else
                        {
                            ledger->addRow(ecsRef, "stores", timberRow(value), "STORES");
                        }
                    }
                    else if (name == RelicPath)
                    {
                        const bool held = it->second.get<bool>();

                        if (held and not ledger->row("relic.sunstone"))
                            ledger->addRow(ecsRef, "kept", relicRow(), "KEPT BETWEEN LIVES");
                        else if (not held and ledger->row("relic.sunstone"))
                            ledger->removeRow(ecsRef, "relic.sunstone");
                    }
                }
            }
        });

        writeAll();

        listenToEvent<ButtonActivatedEvent>([this](const ButtonActivatedEvent& e) {
            auto facts = ecsRef->getSystem<WorldFacts>();

            if (not facts)
                return;

            if (e.tag == "ledger.earn")
            {
                coin += 100;
                facts->setFact(CoinPath, figure(coin));
                echo.setText(ecsRef, "Coin " + figure(coin) + ": the figure widens leftward, the leader gives way");
            }
            else if (e.tag == "ledger.eat")
            {
                rations -= 6;
                facts->setFact(RationsPath, figure(rations));
                echo.setText(ecsRef, rations < 0 ? "Rations " + figure(rations) + ": a debt keeps its row and takes a minus" : "Rations " + figure(rations));
            }
            else if (e.tag == "ledger.timber")
            {
                const bool first = timber == 0;
                timber = first ? FirstTimber : timber + TimberFind;
                facts->setFact(TimberPath, figure(timber));
                echo.setText(ecsRef, first ? "Timber found: a row joins STORES, nothing above it moves" : "Timber " + figure(timber));
            }
            else if (e.tag == "ledger.relic")
            {
                if (relicHeld)
                {
                    relicHeld = false;
                    facts->setFact(RelicPath, false);
                    echo.setText(ecsRef, "The relic is lost: its row goes, its heading stays");
                }
                else
                {
                    echo.setText(ecsRef, "The relic is already lost (Reset brings it back)");
                }
            }
            else if (e.tag == "ledger.reset")
            {
                coin = StartCoin;
                rations = StartRations;
                timber = 0;
                relicHeld = true;
                writeAll();
                echo.setText(ecsRef, "Reset: 412 coin, 18 rations, no timber, the relic kept");
            }
        });

        listenToEvent<OnSDLScanCode>([this](const OnSDLScanCode& event) {
            if (event.key == SDL_SCANCODE_T)
                theme->setTheme(theme->currentTheme() == "day" ? "candle" : "day");
        });

        listenToEvent<ResizeEvent>([this](const ResizeEvent& event) {
            auto entity = ecsRef->getEntity(backgroundId);

            if (entity and entity->has<PositionComponent>())
            {
                entity->get<PositionComponent>()->setWidth(event.width);
                entity->get<PositionComponent>()->setHeight(event.height);
            }
        });
    }

    void LedgerGallery::writeAll()
    {
        auto facts = ecsRef->getSystem<WorldFacts>();

        if (not facts)
            return;

        facts->setFact(CoinPath, figure(coin));
        facts->setFact(RationsPath, figure(rations));
        facts->setFact(TimberPath, timber > 0 ? figure(timber) : std::string());
        facts->setFact(RelicPath, relicHeld);
    }
}
