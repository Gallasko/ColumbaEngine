#include "activitygallery.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

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

#include "UI/activityrow.h"
#include "UI/panel.h"
#include "UI/button.h"
#include "Core/motion.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char* File = "res/chronicle/ui/activitygallery.yaml";

        constexpr const char* PercentFact = "activity.running.percent";
        constexpr const char* SquireFact = "activity.train.squire.req.0.current";

        constexpr const char* SquireId = "train.squire";

        constexpr int StartStrength = 15;
        constexpr int NeededStrength = 18;

        float percentAt(int month, int months)
        {
            return months <= 0 ? 100.0f : 100.0f * static_cast<float>(month) / static_cast<float>(months);
        }

        std::string captionAt(int month, int months)
        {
            return "MONTH " + std::to_string(month) + " OF " + std::to_string(months);
        }

        template <typename Piece>
        Piece* pieceOf(EntitySystem* ecs, _unique_id id)
        {
            auto entity = ecs->getEntity(id);

            if (not entity or not entity->template has<Piece>())
                return nullptr;

            return entity->template get<Piece>().component;
        }
    }

    void ActivityGallery::init()
    {
        theme = ecsRef->getSystem<ThemeSystem>();

        auto place = [](EntityRef e, float x, float y) {
            auto p = e->get<PositionComponent>();
            p->setX(x);
            p->setY(y);
        };

        auto caption = [&](float x, float y, const std::string& text, const std::string& color) {
            LabelSpec spec;
            spec.style = "caption";
            spec.text = text;
            spec.color = color;
            spec.z = 15;

            Label label = makeLabel(ecsRef, spec);
            place(label.entity, x, y);

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
        listId = prefab->findEntity("list").id;
        sideId = prefab->findEntity("side").id;
        nowId = prefab->findEntity("now").id;

        echo = caption(740.0f, 420.0f, "ready", "ink-muted");
        motionLabel = caption(740.0f, 440.0f, Motion::reduced() ? "motion: reduced" : "motion: full", "ink-muted");

        // Seed the facts, then listen: the buttons write facts, the rows follow them.
        if (auto facts = ecsRef->getSystem<WorldFacts>())
        {
            facts->setFact(PercentFact, percentAt(month, months));
            facts->setFact(SquireFact, StartStrength);
        }

        listenToEvent<WorldFactsUpdate>([this](const WorldFactsUpdate& event) {
            auto list = pieceOf<ActivityList>(ecsRef, listId);

            if (not list)
                return;

            for (const auto& name : event.changedFacts)
            {
                const auto& it = event.factMap->find(name);

                if (it == event.factMap->end())
                    continue;

                if (name == PercentFact and not runningId.empty())
                {
                    const float percent = it->second.get<float>();

                    if (auto row = list->row(ecsRef, runningId))
                    {
                        row->setPercent(ecsRef, percent);
                        row->setCaption(ecsRef, captionAt(month, months));
                    }

                    // The side row follows unless it is showing a chosen activity
                    if (list->selected().empty())
                        showRunning();
                }
                else if (name == SquireFact)
                {
                    if (auto squire = list->row(ecsRef, SquireId))
                        squire->setRequirement(ecsRef, 0, it->second.get<int>(), NeededStrength);
                }
            }
        });

        // Selecting shows the chosen activity in the side panel; clearing goes back to the one at work
        listenToEvent<ActivitySelectedEvent>([this](const ActivitySelectedEvent& e) {
            auto list = pieceOf<ActivityList>(ecsRef, listId);

            if (e.id.empty() or not list)
            {
                showRunning();
                return;
            }

            if (auto row = list->row(ecsRef, e.id))
            {
                show(row->spec, ActivityState::Idle, "Chosen");
                echo.setText(ecsRef, "selected '" + e.id + "': Confirm, click it again or press Enter to start it");
            }
        });

        listenToEvent<ActivityActivatedEvent>([this](const ActivityActivatedEvent& e) {
            start(e.id);
        });

        listenToEvent<ButtonActivatedEvent>([this](const ButtonActivatedEvent& e) {
            if (e.tag == "activity.advance")
            {
                advanceMonth();
            }
            else if (e.tag == "activity.meet")
            {
                strength = NeededStrength;

                if (auto facts = ecsRef->getSystem<WorldFacts>())
                    facts->setFact(SquireFact, strength);

                echo.setText(ecsRef, "Strength reaches 18: the Squire's first requirement is met");
            }
            else if (e.tag == "activity.confirm")
            {
                auto list = pieceOf<ActivityList>(ecsRef, listId);
                auto activities = ecsRef->getSystem<ActivitySystem>();

                if (not list or not activities or list->selected().empty())
                {
                    echo.setText(ecsRef, "nothing selected to confirm: click an activity first");
                    return;
                }

                if (auto row = list->row(ecsRef, list->selected()))
                    activities->activate(row->root);
            }
        });

        listenToEvent<OnSDLScanCode>([this](const OnSDLScanCode& event) {
            if (event.key == SDL_SCANCODE_T)
            {
                theme->setTheme(theme->currentTheme() == "day" ? "candle" : "day");
            }
            else if (event.key == SDL_SCANCODE_R)
            {
                Motion::setReduced(not Motion::reduced());
                motionLabel.setText(ecsRef, Motion::reduced() ? "motion: reduced" : "motion: full");
            }
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

    void ActivityGallery::show(const ActivityRowSpec& what, ActivityState state, const std::string& heading)
    {
        if (auto side = pieceOf<Panel>(ecsRef, sideId))
            side->setHeading(ecsRef, heading);

        auto now = pieceOf<ActivityRow>(ecsRef, nowId);

        if (not now)
            return;

        // `what` may be the spec of another row: copy what is shown before the setters run
        const ActivityRowSpec shown = what;

        now->setName(ecsRef, shown.name);
        now->setGlyph(ecsRef, shown.glyph);
        now->setMonths(ecsRef, shown.months);
        now->setGains(ecsRef, shown.gains);
        now->setPercent(ecsRef, shown.percent, false);
        now->setCaption(ecsRef, shown.caption);
        now->setState(ecsRef, state);
    }

    void ActivityGallery::showRunning()
    {
        auto list = pieceOf<ActivityList>(ecsRef, listId);

        if (not list or runningId.empty())
        {
            if (auto side = pieceOf<Panel>(ecsRef, sideId))
                side->setHeading(ecsRef, "Nothing at work");

            if (auto now = pieceOf<ActivityRow>(ecsRef, nowId))
                now->setState(ecsRef, ActivityState::Idle);

            return;
        }

        if (auto row = list->row(ecsRef, runningId))
        {
            ActivityRowSpec shown = row->spec;
            shown.percent = percentAt(month, months);
            shown.caption = captionAt(month, months);

            show(shown, ActivityState::Running, "At work now");
        }
    }

    void ActivityGallery::start(const std::string& id)
    {
        auto list = pieceOf<ActivityList>(ecsRef, listId);

        if (not list)
            return;

        auto row = list->row(ecsRef, id);

        if (not row)
            return;

        // One activity at a time: what ran goes back to idle
        if (not runningId.empty() and runningId != id)
            list->setRowState(ecsRef, runningId, ActivityState::Idle);

        runningId = id;
        month = 0;
        months = row->spec.months;

        // Started again: it says what it said before it was done
        if (auto it = eachOf.find(id); it != eachOf.end())
        {
            row->setEach(ecsRef, it->second);
            eachOf.erase(it);
        }

        row->setPercent(ecsRef, 0.0f, false);
        row->setCaption(ecsRef, captionAt(month, months));

        // A running row cannot stay selected: the list clears its selection, which brings
        // the side panel back to the activity at work
        list->setRowState(ecsRef, id, ActivityState::Running);

        if (auto facts = ecsRef->getSystem<WorldFacts>())
            facts->setFact(PercentFact, percentAt(month, months));

        showRunning();

        echo.setText(ecsRef, "activated '" + id + "': it runs for " + std::to_string(months) + " month(s)");
    }

    void ActivityGallery::advanceMonth()
    {
        auto facts = ecsRef->getSystem<WorldFacts>();
        auto list = pieceOf<ActivityList>(ecsRef, listId);

        if (not facts or not list)
            return;

        if (runningId.empty())
        {
            echo.setText(ecsRef, "nothing is at work: choose an activity and confirm it");
            return;
        }

        ++month;

        if (month < months)
        {
            facts->setFact(PercentFact, percentAt(month, months));
            echo.setText(ecsRef, "'" + runningId + "': " + captionAt(month, months));

            return;
        }

        // At term the activity stops: its row goes back to idle and can be chosen again
        const std::string done = runningId;

        runningId.clear();
        list->setRowState(ecsRef, done, ActivityState::Idle);

        if (auto row = list->row(ecsRef, done); row and row->each)
        {
            eachOf[done] = row->spec.each;
            row->setEach(ecsRef, "DONE");
        }

        if (list->selected().empty())
            showRunning();

        echo.setText(ecsRef, "'" + done + "' is done");
    }
}
