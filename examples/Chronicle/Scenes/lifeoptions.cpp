#include "lifescene.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

#include "logger.h"

#include "ECS/entitysystem.h"
#include "ECS/callable.h"
#include "2D/position.h"
#include "UI/prefab.h"
#include "UI/sizer.h"
#include "UI/prefabloader.h"
#include "UI/prefabbuilder.h"
#include "UI/themesystem.h"
#include "Systems/gamefacts.h"
#include "Helpers/filedialog.h"

#include "UI/label.h"
#include "UI/mark.h"
#include "UI/panel.h"
#include "UI/button.h"
#include "UI/tabs.h"
#include "UI/activityrow.h"
#include "UI/focusorder.h"

// The game's version, as its changelog heads it (CMakeLists.txt reads it from CHANGELOG.md)
#ifndef CHRONICLE_VERSION
#define CHRONICLE_VERSION "dev"
#endif

using namespace pg;

// The Options page, as the Life scene keeps it: this file is LifeScene's, the part of it that is
// the Options tab. How the game is played and lit is the player's and this machine's (Settings):
// it is kept in the save's file beside the life, read when the scene starts and written with the
// life at every change, and neither a save loaded nor a wipe touches it. The save itself is shown, written out to a file
// of the player's (Export), taken back from one (Load), or burnt with everything the world kept
// (Wipe). A save loaded and a wipe make the scene again in its own place: a new life is a new
// scene, with nothing of the last one's facts, deeds or guide left over.
namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Options";

        constexpr const char * const ActivitiesList = "life.activities";
        constexpr const char * const OptionsName = "options";
        constexpr const char * const SaveExtension = ".sz";
        constexpr const char * const TownCalled = "Bellmoor";

        // Its buttons and its dialogs', by tag
        const std::string OptionsTag = "options.";
        constexpr const char * const ExportTag = "options.export";
        constexpr const char * const LoadTag = "options.load";
        constexpr const char * const WipeTag = "options.wipe";
        constexpr const char * const KeepTag = "options.keep";           // The wipe's dialog: keep the chronicle
        constexpr const char * const BurnTag = "options.burn";           // And burn it
        constexpr const char * const KeepSaveTag = "options.keepSave";   // The load's dialog: keep this chronicle
        constexpr const char * const ReplaceTag = "options.replace";     // And take the file's in its place
        constexpr const char * const BurnWord = "burn";                  // What the wipe asks to be typed, in any case

        constexpr float RowGap = 16.0f;            // Between a row's text and its control (the yaml's spacing)
        constexpr float ExportedMs = 4000.0f;      // How long "Exported to" stays
        constexpr float DialogMargin = 12.0f;      // A dialog narrower than the window by this, either side
        constexpr float LedgerFigure = 120.0f;     // The width of a figure in the wipe's ledger
        constexpr float ButtonGap = 12.0f;         // Between the dialogs' buttons

        struct DialogNoOp {};

        std::string lower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        std::string trimmed(const std::string& s)
        {
            const auto first = s.find_first_not_of(" \t");

            if (first == std::string::npos)
                return "";

            return s.substr(first, s.find_last_not_of(" \t") - first + 1);
        }

        // "one", "two"... up to twelve, then the figure: the chronicle writes its small numbers out
        std::string spelled(int n, bool capital)
        {
            static const char* Words[] = {"no", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven", "twelve"};

            std::string word = n >= 0 and n <= 12 ? Words[n] : std::to_string(n);

            if (capital and not word.empty())
                word[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(word[0])));

            return word;
        }

        std::string counted(int n, const std::string& one, const std::string& many)
        {
            return std::to_string(n) + " " + (n == 1 ? one : many);
        }

        void windowSize(EntitySystem* ecs, float& width, float& height)
        {
            width = 1320.0f;
            height = 1020.0f;

            if (auto window = ecs->getEntity("__MainWindow"); window and window->has<PositionComponent>())
            {
                auto pos = window->get<PositionComponent>();

                if (pos->width > 0.0f and pos->height > 0.0f)
                {
                    width = pos->width;
                    height = pos->height;
                }
            }
        }

        // In a file a piece stands in its panel's body inside a wrap, and the wrap is what the
        // layout holds: to take the piece out of the stack it is the wrap that is hidden
        EntityRef wrapIn(EntityRef body, EntityRef piece)
        {
            if (body.empty() or not body->has<VerticalLayout>())
                return piece;

            for (auto& child : body->get<VerticalLayout>()->entities)
            {
                if (child.id == piece.id)
                    return child;

                if (not child->has<Prefab>())
                    continue;

                auto prefab = child->get<Prefab>();

                if (prefab->namedChildrenIds.count("MainEntity") > 0 and prefab->getEntity("MainEntity").id == piece.id)
                    return child;
            }

            return piece;
        }

        NodeSpec* childNamed(NodeSpec& node, const std::string& name)
        {
            for (auto& child : node.children)
            {
                if (child.name == name)
                    return &child;

                if (NodeSpec* inner = childNamed(child, name))
                    return inner;
            }

            return nullptr;
        }

        float widthOf(EntityRef entity)
        {
            return entity.empty() ? 0.0f : entity->get<PositionComponent>()->width;
        }

        float heightOf(EntityRef entity)
        {
            return entity.empty() ? 0.0f : entity->get<PositionComponent>()->height;
        }

        // The hour a save was written, as the page says it: "19 Oct, 06:41"
        std::string hourOf(size_t seconds)
        {
            const std::time_t when = static_cast<std::time_t>(seconds);
            std::tm local{};

#ifdef _WIN32
            localtime_s(&local, &when);
#else
            localtime_r(&when, &local);
#endif

            char text[32];
            std::strftime(text, sizeof(text), "%d %b, %H:%M", &local);

            std::string said = text;

            // "9 Oct", not "09 Oct"
            if (said.size() > 1 and said[0] == '0')
                said.erase(0, 1);

            return said;
        }
    }

    // ---- the page ---------------------------------------------------------------------------------------------

    int LifeScene::optionsTab() const
    {
        auto tabs = piece<Tabs>("tabs");

        if (not tabs)
            return -1;

        for (size_t i = 0; i < tabs->spec.items.size(); ++i)
        {
            if (lower(tabs->spec.items[i].label) == OptionsName)
                return static_cast<int>(i);
        }

        return -1;
    }

    void LifeScene::showOptions(bool shown)
    {
        optionsShown = shown and not named(OptionsName).empty();

        // The pages of the middle column take each other's place: showTown lays them
        if (optionsShown)
            adventureShown = false;

        // What the page said of a file the last time it was open is said no more
        if (optionsShown)
        {
            exportFailed.clear();
            loadFailed.clear();
        }

        showTown(optionsShown ? false : townShown);

        if (optionsShown)
            fillOptions();
    }

    void LifeScene::fitOptions(float width, float height)
    {
        // The line over the button says how a work begins, on every page
        applyClickMode();

        EntityRef body = named(OptionsName);

        if (body.empty())
            return;

        body->get<PositionComponent>()->setWidth(width);
        body->get<PositionComponent>()->setHeight(height);

        // Each panel as wide as the column, and what stands in it as wide as the panel leaves it
        for (const char* name : {"playPanel", "savesPanel", "burnPanel"})
            widen(piece<Panel>(name), width);

        if (auto foot = piece<Label>("optionsFoot"))
            foot->setWidth(ecsRef, width);

        fillOptions();
    }

    void LifeScene::fillOptions()
    {
        EntitySystem* ecs = ecsRef;

        if (named(OptionsName).empty())
            return;

        auto write = [this, ecs](const char* name, const std::string& text) {
            if (auto label = piece<Label>(name); label and label->spec.text != text)
                label->setText(ecs, text);
        };

        // How he plays and how the page is lit, as the settings say
        if (auto click = piece<Switch>("clickSwitch"))
            click->setOn(ecs, settings.beginOnFirstClick);

        if (auto cards = piece<RadioCards>("lightCards"))
        {
            auto theme = ecs->getSystem<ThemeSystem>();
            const std::string lit = theme ? theme->currentTheme() : settings.theme;

            if (cards->selected() != lit)
                cards->select(ecs, lit);
        }

        // This chronicle: who, how old, which life, and when it was last written
        write("chronicleWho", save.name + " · " + std::to_string(static_cast<int>(save.age)) + " yr");

        std::string written;

        if (exportedLeft > 0.0f)
            written = "exported to " + exportedTo;
        else if (opt.noSave)
            written = "never written (--no-save)";
        else if (save.savedAt == 0)
            written = "not written yet";
        else
            written = "last written " + hourOf(save.savedAt);

        write("chronicleWhere", "Life " + std::to_string(save.lives) + " · " + TownCalled + " · " + written);

        // Under Export: the file it writes. Under it, and under Load, why a file could not be
        // written or was refused, in red with a cross; nothing otherwise
        write("exportNote", exportName());

        auto sayFailed = [this, ecs](const char* name, const std::string& why) {
            EntityRef note = named(name);

            if (note.empty())
                return;

            const bool said = not why.empty();

            if (said and note->has<MarkedLabel>() and note->get<MarkedLabel>()->label.spec.text != why)
                note->get<MarkedLabel>()->setText(ecs, why);

            if (auto panel = piece<Panel>("savesPanel"))
                wrapIn(panel->body, note)->get<PositionComponent>()->setVisibility(said);

            note->get<PositionComponent>()->setVisibility(said);
        };

        sayFailed("exportFail", exportFailed);
        sayFailed("loadNote", loadFailed);

        // What cannot be undone is headed in red lead
        if (auto burn = piece<Panel>("burnPanel"); burn and burn->title)
        {
            if (burn->title->spec.color != "vermilion")
                burn->title->setColor(ecs, "vermilion");
        }

        write("optionsFoot", std::string("Alpha ") + CHRONICLE_VERSION + " · settings are kept in the save, and stay when another is loaded or the chronicle is burnt.");

        layOptions();
    }

    void LifeScene::layOptions()
    {
        EntitySystem* ecs = ecsRef;

        // A row: its text at the left, as wide as the control at its right leaves it, and the row as
        // tall as the taller of the two
        auto row = [this, ecs](const char* panelName, const char* rowName, const char* lineName, const char* controlName) {
            auto panel = piece<Panel>(panelName);
            EntityRef line = named(lineName);
            EntityRef control = named(controlName);
            EntityRef holder = named(rowName);

            if (not panel or line.empty() or control.empty() or holder.empty() or not line->has<Label>())
                return;

            const float inner = panel->innerWidth();
            const float room = std::max(80.0f, inner - widthOf(control) - RowGap);
            auto label = line->get<Label>();

            if (std::abs(label->spec.width - room) > 0.5f)
                label->setWidth(ecs, room);

            const float tall = std::max(heightOf(line), heightOf(control));
            auto pos = holder->get<PositionComponent>();

            if (std::abs(pos->width - inner) > 0.5f)
                pos->setWidth(inner);

            if (std::abs(pos->height - tall) > 0.5f)
                pos->setHeight(tall);
        };

        row("playPanel", "clickRow", "clickLine", "clickSwitch");
        row("savesPanel", "exportRow", "exportLine", "exportButton");
        row("savesPanel", "loadRow", "loadLine", "loadButton");
        row("burnPanel", "burnRow", "burnLine", "wipeButton");

        // The theme's cards share their panel's width
        if (auto panel = piece<Panel>("playPanel"))
        {
            if (auto cards = piece<RadioCards>("lightCards"); cards and std::abs(cards->spec.width - panel->innerWidth()) > 0.5f)
                cards->setWidth(ecs, panel->innerWidth());
        }
    }

    void LifeScene::enableOptionsFocus(bool on)
    {
        auto order = ecsRef->getSystem<FocusOrderSystem>();

        if (not order)
            return;

        // Out of view, the page's controls are not in the way of Tab
        if (auto click = piece<Switch>("clickSwitch"); click and not click->face.empty())
            order->setEnabled(click->face.id, on);

        if (auto cards = piece<RadioCards>("lightCards"))
        {
            for (const auto& card : cards->cards)
                order->setEnabled(card.face.id, on);
        }

        for (const char* name : {"exportButton", "loadButton", "wipeButton"})
        {
            if (auto button = piece<Button>(name); button and not button->face.empty())
                order->setEnabled(button->face.id, on);
        }
    }

    // ---- the settings -----------------------------------------------------------------------------------------

    void LifeScene::readSettings()
    {
        // From the save's file, beside the life. A life never written (--no-save) reads none:
        // the defaults
        settings = Settings{};

        if (not opt.noSave)
            settings.load(opt.savePath);

        applyClickMode();
    }

    void LifeScene::writeSettings()
    {
        // They are written with the life, in its file: never by a life that is never written
        if (opt.noSave or leaving)
            return;

        if (not writeSave())
            LOG_ERROR(DOM, "Could not write the settings to " << opt.savePath);
    }

    void LifeScene::applyClickMode()
    {
        if (auto activities = ecsRef->getSystem<ActivitySystem>())
        {
            if (settings.beginOnFirstClick)
                activities->beginOnFirstClick.insert(ActivitiesList);
            else
                activities->beginOnFirstClick.erase(ActivitiesList);
        }

        // The line over the button of a life that has done nothing yet says how a work begins
        const std::string said = settings.beginOnFirstClick ? "Click a work to begin it" : "Choose a work, then press Begin";

        if (auto guide = piece<Label>("guide"); guide and guide->spec.text != said)
            guide->setText(ecsRef, said);
    }

    void LifeScene::chooseTheme(const std::string& id)
    {
        auto theme = ecsRef->getSystem<ThemeSystem>();

        if (not theme or not theme->theme().hasThemeId(id))
            return;

        // Every themed thing on every screen is painted again on the next pass
        if (theme->currentTheme() != id)
            theme->setTheme(id);

        if (settings.theme != id)
        {
            settings.theme = id;
            writeSettings();
        }

        if (auto cards = piece<RadioCards>("lightCards"); cards and cards->selected() != id)
            cards->select(ecsRef, id);
    }

    // ---- the save: out, in, burnt -------------------------------------------------------------------------------

    bool LifeScene::onOptionsButton(const std::string& tag)
    {
        if (tag.rfind(OptionsTag, 0) != 0)
            return false;

        // The page's own buttons are under the dialog while one is up
        if (tag == ExportTag and dialog.empty())
            exportSave();
        else if (tag == LoadTag and dialog.empty())
            askLoad();
        else if (tag == WipeTag and dialog.empty())
            openDialog("burn");
        else if (tag == KeepTag or tag == KeepSaveTag)
            closeDialog();
        else if (tag == BurnTag and dialogKind == "burn")
        {
            // The button is greyed until the word is typed; a key that reached it anyway asks again
            EntityRef field = dialog.empty() ? EntityRef{} : dialog->get<Prefab>()->findEntity("burnWord");

            if (not field.empty() and field->has<WordField>() and lower(trimmed(field->get<WordField>()->text(ecsRef))) == BurnWord)
                wipe();
        }
        else if (tag == ReplaceTag and dialogKind == "replace" and toReplace)
        {
            LifeSave loaded = *toReplace;

            replaceWith(std::move(loaded));
        }

        return true;
    }

    std::string LifeScene::exportName() const
    {
        // His first name, in small letters and nothing else: "Aldren of Bellmoor" -> "aldren"
        std::string first;

        for (char c : save.name)
        {
            if (c == ' ')
            {
                if (not first.empty())
                    break;

                continue;
            }

            if (std::isalnum(static_cast<unsigned char>(c)))
                first += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        if (first.empty())
            first = "life";

        return "chronicle-" + first + "-life" + std::to_string(save.lives) + SaveExtension;
    }

    void LifeScene::exportSave()
    {
        exportFailed.clear();

        // The chronicle written as it stands, then copied out: the file is the save of this moment
        if (not opt.noSave)
            writeSave();

        const std::string name = exportName();
        bool written = false;

        if (filedialog::hasNativeDialogs())
        {
            const std::string path = filedialog::askSavePath("Export the chronicle", name, {std::string("*") + SaveExtension}, "Chronicle saves");

            // The dialog held the game still: the frame after it is no time played
            skipTick = true;

            // Cancelled: nothing is said
            if (path.empty())
                return;

            written = save.save(path);

            if (written)
                exportedTo = std::filesystem::path(path).filename().string();
        }
        else
        {
            // A browser has no path to ask for: the file is offered as a download under its name
            const std::string temp = (std::filesystem::path(opt.savePath).parent_path() / "export.sz").string();

            written = save.save(temp) and filedialog::offerDownload(temp, name);

            std::error_code ec;
            std::filesystem::remove(temp, ec);

            if (written)
                exportedTo = name;
        }

        if (written)
            exportedLeft = ExportedMs;
        else
            exportFailed = "The file could not be written.";

        fillOptions();
    }

    void LifeScene::askLoad()
    {
        loadFailed.clear();

        if (filedialog::hasNativeDialogs())
        {
            const std::string path = filedialog::askOpenPath("Load a chronicle", {std::string("*") + SaveExtension}, "Chronicle saves");

            skipTick = true;

            if (not path.empty())
                takeSave(path);
            else
                fillOptions();

            return;
        }

        // A browser's picker: the file comes later (runUpload)
        uploading = filedialog::pickUpload(SaveExtension);

        fillOptions();
    }

    void LifeScene::runUpload()
    {
        const std::string into = (std::filesystem::path(opt.savePath).parent_path() / "import.sz").string();

        switch (filedialog::pollUpload(into))
        {
        case filedialog::Upload::Waiting:
            return;

        case filedialog::Upload::None:
            uploading = false;
            return;

        case filedialog::Upload::Failed:
            uploading = false;
            loadFailed = "The file could not be read.";
            fillOptions();
            return;

        case filedialog::Upload::Ready:
        {
            uploading = false;

            takeSave(into);

            std::error_code ec;
            std::filesystem::remove(into, ec);
            return;
        }
        }
    }

    bool LifeScene::untouched() const
    {
        // A first life that has done nothing, in a world that has nothing: there is nothing to lose
        return save.lives <= 1 and save.termsDone() == 0 and save.running.empty() and not save.townKnown and save.town.empty()
           and save.knowledge.empty() and save.recipes.empty() and not onWalk();
    }

    void LifeScene::takeSave(const std::string& path)
    {
        LifeSave read;

        // Checked before anything is touched: a file refused leaves the chronicle as it is
        switch (LifeSave::open(path, read))
        {
        case LifeSave::Opened::NotASave:
            loadFailed = "Not a Chronicle save.";
            fillOptions();
            return;

        case LifeSave::Opened::Newer:
            loadFailed = "Save from a newer version.";
            fillOptions();
            return;

        case LifeSave::Opened::Ok:
            break;
        }

        loadFailed.clear();

        if (untouched())
        {
            replaceWith(std::move(read));
            return;
        }

        toReplace = std::move(read);

        if (not openDialog("replace"))
            toReplace.reset();
    }

    void LifeScene::replaceWith(LifeSave loaded)
    {
        // The time since the file was written is not time this chronicle stayed shut: a save
        // loaded writes no time of its own
        loaded.savedAt = static_cast<size_t>(wallClock());

        if (not opt.noSave)
        {
            std::error_code ec;

            // One copy of the chronicle it replaces, beside it, replaced each time
            if (std::filesystem::exists(opt.savePath, ec))
                std::filesystem::copy_file(opt.savePath, opt.savePath + ".bak", std::filesystem::copy_options::overwrite_existing, ec);

            if (ec)
                LOG_ERROR(DOM, "Could not keep " << opt.savePath << ".bak: " << ec.message());

            // The life is the file's; the settings stay this machine's
            if (not loaded.save(opt.savePath, &settings))
                LOG_ERROR(DOM, "Could not write the save loaded to " << opt.savePath);
        }

        LifeSceneOptions next = opt;
        next.opened = std::move(loaded);
        next.fresh = false;

        remake(std::move(next));
    }

    void LifeScene::wipe()
    {
        // The life and its copy go; the file keeps the settings alone, and every file the player
        // exported is left as it is. A life of --no-save has written nothing to take back
        if (not opt.noSave)
        {
            std::error_code ec;

            std::filesystem::remove(opt.savePath + ".bak", ec);

            if (not settings.save(opt.savePath))
                LOG_ERROR(DOM, "Could not write the settings alone to " << opt.savePath);
        }

        // The first page of the game: a first life, at six, led by its guide
        LifeSceneOptions next = opt;
        next.opened.reset();
        next.fresh = true;
        next.lives = 1;
        next.guide = 0;
        next.age = 0.0f;

        remake(std::move(next));
    }

    void LifeScene::remake(LifeSceneOptions next)
    {
        closeDialog();

        // A life of its own: not one opened on a walk or of a build
        next.walk.clear();
        next.build.clear();
        next.sets.clear();
        next.saveAsked = false;
        next.clearFacts = true;

        // Until it goes, no month runs here and nothing is written over what was just written
        leaving = true;
        paused = true;

        if (auto scenes = ecsRef->getSystem<SceneElementSystem>())
            scenes->loadSystemScene<LifeScene>(next);
    }

    // ---- the dialogs ------------------------------------------------------------------------------------------

    bool LifeScene::openDialog(const std::string& kind)
    {
        // The bug report's dialog up (the browser's own, which is not this slot's) asks first
        if (reportOpen and kind != "report")
            return false;

        // One dialog at a time; the save waiting for an answer is this one's
        std::optional<LifeSave> waiting = std::move(toReplace);

        closeDialog();

        toReplace = std::move(waiting);

        if (kind == "replace" and not toReplace)
            return false;

        const std::string& file = kind == "burn" ? opt.burnFile : kind == "report" ? opt.reportFile : opt.replaceFile;

        std::vector<std::string> errors;
        PrefabLoadOptions options;
        options.errors = &errors;

        auto spec = loadNodeSpec(ecsRef, file, options);

        if (not spec)
        {
            LOG_ERROR(DOM, "Could not load the dialog " << file);
            return false;
        }

        for (const auto& e : errors)
            LOG_ERROR(DOM, file << ": " << e);

        // A leaf wider than the window (a phone's) is the window's width, less a margin
        float room, tall;
        windowSize(ecsRef, room, tall);
        room -= 2.0f * DialogMargin;

        if (NodeSpec* leaf = childNamed(*spec, "leaf"))
        {
            auto drawn = leaf->props.find("width");

            if (drawn != leaf->props.end() and drawn->second.type == UnionType::INT and static_cast<float>(drawn->second.get<int>()) > room)
                leaf->props["width"] = std::max(240.0f, room);
            else if (drawn != leaf->props.end() and drawn->second.type == UnionType::FLOAT and drawn->second.get<float>() > room)
                leaf->props["width"] = std::max(240.0f, room);
        }

        dialog = buildTree(ecsRef, *spec);

        if (dialog.empty() or not dialog->has<Prefab>())
        {
            LOG_ERROR(DOM, "Could not build the dialog " << file);
            dialog = EntityRef{};

            return false;
        }

        dialogKind = kind;

        // It leaves with the scene, like the page
        ecsRef->attach<SceneElement>(dialog);

        auto prefab = dialog->get<Prefab>();
        EntitySystem* ecs = ecsRef;

        auto find = [&prefab](const std::string& name) { return prefab->findEntity(name); };

        auto write = [&find, ecs](const std::string& name, const std::string& text) {
            if (EntityRef ent = find(name); not ent.empty() and ent->has<Label>())
                ent->get<Label>()->setText(ecs, text);
        };

        auto leaf = find("leaf");
        const float inner = not leaf.empty() and leaf->has<Panel>() ? leaf->get<Panel>()->innerWidth() : 512.0f;

        // Each line of the dialog as wide as its leaf leaves it
        auto fitTo = [&find, ecs](const std::string& name, float width) {
            if (EntityRef ent = find(name); not ent.empty())
            {
                if (ent->has<Label>())
                    ent->get<Label>()->setWidth(ecs, std::max(1.0f, width));
                else if (ent->has<WordField>())
                    ent->get<WordField>()->setWidth(ecs, width);
                else
                    ent->get<PositionComponent>()->setWidth(width);
            }
        };

        // The buttons at the right end of their line, what is left of it before them
        auto buttonsRow = [&find, &fitTo, inner](const std::string& row, const std::string& spacer, const std::string& keep, const std::string& confirm) {
            fitTo(row, inner);
            fitTo(spacer, std::max(1.0f, inner - widthOf(find(keep)) - widthOf(find(confirm)) - 2.0f * ButtonGap));
        };

        auto faceOf = [&find](const std::string& name) -> _unique_id {
            EntityRef ent = find(name);

            if (not ent.empty() and ent->has<Button>())
                return ent->get<Button>()->face.id;

            if (not ent.empty() and ent->has<WordField>())
                return ent->get<WordField>()->face.id;

            return 0;
        };

        if (kind == "burn")
        {
            // What is lost, in real figures: the lives of the world, the places of the town raised,
            // what is known of the lands outside it
            int buildings = 0;

            for (const auto& [place, level] : save.town)
                buildings += level > 0 ? 1 : 0;

            int known = 0;

            for (const auto& [key, value] : save.knowledge)
                known += value > 0 ? 1 : 0;

            const int lives = std::max(1, save.lives);

            write("burnGloss", spelled(lives, true) + (lives == 1 ? " life" : " lives") + ", a town, and everything the Guild had learned — gone to ash.");
            write("lostLivesFigure", "− " + std::to_string(lives));
            write("lostTown", std::string("Town of ") + TownCalled + ", buildings and stores");
            write("lostTownFigure", "− " + counted(buildings, "building", "buildings"));
            write("lostKnownFigure", "− " + counted(known, "entry", "entries"));

            for (const char* line : {"lostLivesRow", "lostTownRow", "lostKnownRow"})
                fitTo(line, inner);

            for (const char* name : {"lostLives", "lostTown", "lostKnown"})
                fitTo(name, inner - LedgerFigure - RowGap);

            fitTo("burnWord", inner);
            buttonsRow("burnButtons", "burnSpacer", "burnKeep", "burnConfirm");

            dialogFaces = {faceOf("burnWord"), faceOf("burnKeep"), faceOf("burnConfirm")};
        }
        else if (kind == "report")
        {
            // The bug report's (lifereport.cpp fills its line and greys its Send)
            for (const char* line : {"reportTitle", "reportNote", "reportLine"})
                fitTo(line, inner);

            fitTo("reportWords", inner);
            buttonsRow("reportButtons", "reportSpacer", "reportCancel", "reportSend");

            dialogFaces = {faceOf("reportWords"), faceOf("reportCancel"), faceOf("reportSend")};
        }
        else
        {
            const LifeSave& held = *toReplace;

            write("replaceLine", "The file holds " + held.name + " at " + std::to_string(static_cast<int>(held.age)) + ", in life " + std::to_string(held.lives)
                                 + " of the world. It takes the place of this chronicle, which is not kept. Export first if this one is worth keeping.");

            buttonsRow("replaceButtons", "replaceSpacer", "replaceKeep", "replaceConfirm");

            dialogFaces = {faceOf("replaceKeep"), faceOf("replaceConfirm")};
        }

        dialogFaces.erase(std::remove(dialogFaces.begin(), dialogFaces.end(), _unique_id{0}), dialogFaces.end());

        // Tab goes round the dialog's own controls while it is up
        if (auto order = ecs->getSystem<FocusOrderSystem>())
            order->setScope(dialogFaces);

        // The veil takes the mouse: what is under it is neither hovered nor clicked
        if (EntityRef veil = find("veil"); not veil.empty())
        {
            if (not veil->has<MouseEnterComponent>())
                ecs->attach<MouseEnterComponent>(veil, makeCallable<DialogNoOp>(DialogNoOp{}));

            if (not veil->has<MouseLeaveComponent>())
                ecs->attach<MouseLeaveComponent>(veil, makeCallable<DialogNoOp>(DialogNoOp{}));
        }

        fitDialog(windowWidth, windowHeight);

        // The word is typed at once, without a click in its field first; and so are a report's words
        if (kind == "burn" or kind == "report")
        {
            if (EntityRef field = find(kind == "burn" ? "burnWord" : "reportWords"); not field.empty() and field->has<WordField>())
                field->get<WordField>()->focus(ecs);
        }

        return true;
    }

    void LifeScene::closeDialog()
    {
        auto order = ecsRef->getSystem<FocusOrderSystem>();

        if (order)
        {
            order->setScope({});

            // Its controls leave the Tab order with it
            for (auto face : dialogFaces)
                order->remove(face);
        }

        dialogFaces.clear();

        if (not dialog.empty())
            ecsRef->removeEntity(dialog);

        // The bug report's dialog shut with it (lifereport.cpp)
        if (dialogKind == "report")
            reportOpen = false;

        dialog = EntityRef{};
        dialogKind.clear();
        toReplace.reset();
    }

    void LifeScene::fitDialog(float width, float height)
    {
        if (dialog.empty() or not dialog->has<Prefab>())
            return;

        // The veil is the window's size, from its corner: nothing of the page shows past it
        if (EntityRef veil = dialog->get<Prefab>()->findEntity("veil"); not veil.empty())
        {
            auto pos = veil->get<PositionComponent>();

            pos->setX(0.0f);
            pos->setY(0.0f);
            pos->setWidth(width);
            pos->setHeight(height);
        }
    }

    void LifeScene::burnWordChanged(const std::string& word)
    {
        if (dialogKind != "burn" or dialog.empty())
            return;

        EntityRef confirm = dialog->get<Prefab>()->findEntity("burnConfirm");

        if (confirm.empty() or not confirm->has<Button>())
            return;

        const bool matches = lower(trimmed(word)) == BurnWord;

        confirm->get<Button>()->setDisabled(ecsRef, not matches);
    }
}
