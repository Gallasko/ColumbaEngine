#include "application.h"

#include <fstream>

#include "logger.h"

#include "UI/prefab.h"
#include "UI/textinput.h"
#include "UI/sizer.h"
#include "2D/simple2dobject.h"

#include "Helpers/tinyfiledialogs.h"

#include "Systems/basicsystems.h"

using namespace pg;

namespace {
    static const char *const DOM = "App";
}

GameApp::GameApp(const std::string &appName) : appName(appName) {
    LOG_THIS_MEMBER(DOM);
}

GameApp::~GameApp() {
    LOG_THIS_MEMBER(DOM);
}

std::thread *initThread;
pg::Window *mainWindow = nullptr;
std::atomic<bool> initialized = {false};
bool init = false;
bool running = true;

void initWindow(const std::string &appName) {
#ifdef __EMSCRIPTEN__
    mainWindow = new pg::Window(appName, "/save/savedData.sz");
#else
    mainWindow = new pg::Window(appName);
#endif

    LOG_INFO(DOM, "Window init...");

    initialized = true;
}

constant::Vector4D getLineTextBgColor(size_t lineNumber)
{
    return (lineNumber % 2) ? constant::Vector4D{167.f, 167.f, 167.f, 255.f} : constant::Vector4D{218.f, 218.f, 218.f, 255.f};
}

struct PrefabClickedEvent
{
    PrefabClickedEvent(_unique_id id) : id(id) {}
    PrefabClickedEvent(const PrefabClickedEvent& other) : id(other.id) {}

    PrefabClickedEvent& operator=(const PrefabClickedEvent& other)
    {
        id = other.id;
        return *this;
    }

    _unique_id id;
};

CompList<Prefab, Simple2DObject, TTFText> makeLinePrefab(EntitySystem *ecsRef, CompRef<UiAnchor> anchor, size_t lineNumber)
{
    auto prefabEnt = makeAnchoredPrefab(ecsRef);
    auto prefab = prefabEnt.get<Prefab>();
    auto prefabAnchor = prefabEnt.get<UiAnchor>();

    auto color = getLineTextBgColor(lineNumber);

    auto square = makeUiSimple2DShape(ecsRef, Shape2D::Square, 25, 25, color);
    auto squareAnchor = square.get<UiAnchor>();

    prefabAnchor->setHeightConstrain(PosConstrain{square.entity.id, AnchorType::Height});
    prefabEnt.get<PositionComponent>()->setWidth(50);

    squareAnchor->setTopAnchor(prefabAnchor->top);
    squareAnchor->setLeftAnchor(prefabAnchor->left);

    // Todo replace the -50 here by an actual masking until ready of the ttf text component
    auto lineText = makeTTFText(ecsRef, 0, -50, 4, "light", std::to_string(lineNumber), 0.4, constant::Vector4D{0.f, 0.f, 0.f, 255.f});

    auto textAnchor = lineText.get<UiAnchor>();
    textAnchor->centeredIn(squareAnchor);
    textAnchor->setZConstrain(PosConstrain{square.entity.id, AnchorType::Z, PosOpType::Add, 1});

    auto s2 = makeUiSimple2DShape(ecsRef, Shape2D::Square, 25, 25, constant::Vector4D{0.f, 0.f, 0.f, 255.f});
    auto s2Bg = s2.get<Simple2DObject>();
    auto s2Anchor = s2.get<UiAnchor>();

    s2Anchor->setTopAnchor(prefabAnchor->top);
    s2Anchor->setLeftAnchor(prefabAnchor->left);
    s2Anchor->setLeftMargin(30);
    s2Anchor->setRightAnchor(anchor->right);

    prefabAnchor->setWidthConstrain(PosConstrain{s2.entity.id, AnchorType::Width});

    auto inputText = makeTTFText(ecsRef, 0, 0, 4, "light", "", 0.4, constant::Vector4D{255.f, 255.f, 255.f, 255.f});
    auto inputTTFText = inputText.get<TTFText>();
    auto inputTextAnchor = inputText.get<UiAnchor>();

    inputTextAnchor->setLeftAnchor(s2Anchor->left);
    inputTextAnchor->setBottomAnchor(s2Anchor->bottom);
    inputTextAnchor->setBottomMargin(5);

    prefab->addToPrefab(square.entity, "LineTextBg");
    prefab->addToPrefab(lineText.entity, "LineText");
    prefab->addToPrefab(s2.entity, "TextBg");
    prefab->addToPrefab(inputText.entity, "Text");

    prefabEnt.attach<MouseLeftClickComponent>(makeCallable<PrefabClickedEvent>(prefabEnt.entity.id));

    prefab->addHelper("UpdateLineText", [](Prefab *prefab, size_t newLineValue) {
        prefab->getEntity("LineText")->get<TTFText>()->setText(std::to_string(newLineValue));
        prefab->getEntity("LineTextBg")->get<Simple2DObject>()->setColors(getLineTextBgColor(newLineValue));
    });

    prefab->addHelper("GetCurrentText", [](Prefab *prefab) -> std::string {
        return prefab->getEntity("Text")->get<TTFText>()->text;
    });

    prefab->addHelper("SetCurrentText", [](Prefab *prefab, const std::string& newText) {
        prefab->getEntity("Text")->get<TTFText>()->setText(newText);
    });

    prefab->addHelper("SetAsFocusLine", [](Prefab *prefab) {
        prefab->getEntity("TextBg")->get<Simple2DObject>()->setColors(constant::Vector4D{255.f, 0.f, 0.f, 255.f});
    });

    prefab->addHelper("UnfocusLine", [](Prefab *prefab) {
        prefab->getEntity("TextBg")->get<Simple2DObject>()->setColors(constant::Vector4D{0.f, 0.f, 0.f, 255.f});
    });

    return {prefabEnt.entity, prefab, s2Bg, inputTTFText};
    // return prefabEnt.entity;
}

struct OpenFileAction {};

struct SaveFileAction {};

struct TextHandlingSys : public System<
    QueuedListener<OnSDLTextInput>,
    QueuedListener<OnSDLScanCode>,
    QueuedListener<OnSDLScanCodeReleased>,
    QueuedListener<PrefabClickedEvent>,
    Listener<OpenFileAction>,
    Listener<SaveFileAction>,
    Listener<LayoutScrolledEvent>,
    Listener<TickEvent>,
    InitSys>
{
    virtual void onProcessEvent(const PrefabClickedEvent& event) override
    {
        auto ent = ecsRef->getEntity(event.id);

        if (not ent or not ent->has<Prefab>())
        {
            LOG_ERROR(DOM, "Clicked on a non-prefab entity " << event.id);
            return;
        }

        auto prefab = ent->get<Prefab>();

        auto lineText = prefab->getEntity("LineText");

        auto lineNumber = lineText->get<TTFText>()->text;

        LOG_INFO(DOM, "Clicked on line: " << lineNumber);

        focusLine(std::stoul(lineNumber));
    }

    virtual void onProcessEvent(const OnSDLTextInput& event) override
    {
        auto pf = getLinePrefab(currentLine);
        if (not pf)
            return;

        auto currentText = getLineText(currentLine);
        auto newText = currentText.substr(0, cursorCol) + event.text + currentText.substr(cursorCol);
        cursorCol += event.text.size();
        pf->callHelper("SetCurrentText", newText);

        if (isVirtualMode)
            fileLines[currentLine - 1] = newText;

        resetBlink();
        repositionCursor();
    }

    void focusLine(size_t ln)
    {
        if (isVirtualMode and not isLineInPool(ln))
            scrollPoolToLine(ln);

        auto pfBefore = getLinePrefab(currentLine);
        if (pfBefore)
            pfBefore->callHelper("UnfocusLine");

        currentLine = ln;

        auto pfAfter = getLinePrefab(currentLine);
        if (pfAfter)
            pfAfter->callHelper("SetAsFocusLine");

        // Clamp column to the new line's length
        const std::string lineText = getLineText(currentLine);
        if (cursorCol > lineText.size())
            cursorCol = lineText.size();

        resetBlink();
        repositionCursor();
    }

    virtual void onProcessEvent(const OnSDLScanCode& event) override
    {
        if (event.key == SDL_SCANCODE_RETURN)
        {
            auto oldLine = currentLine++;
            lineNumber++;
            cursorCol = 0;

            if (isVirtualMode)
            {
                fileLines.insert(fileLines.begin() + oldLine, "");

                auto pfBefore = getLinePrefab(oldLine);
                if (pfBefore) pfBefore->callHelper("UnfocusLine");

                for (size_t i = currentLine - 1; i < poolWindowStart + linePool.size() && i < fileLines.size(); i++)
                {
                    size_t pi = i - poolWindowStart;
                    linePool[pi].get<Prefab>()->callHelper("SetCurrentText", fileLines[i]);
                    linePool[pi].get<Prefab>()->callHelper("UpdateLineText", i + 1);
                }

                updateSpacers();

                auto pfAfter = getLinePrefab(currentLine);
                if (pfAfter)
                    pfAfter->callHelper("SetAsFocusLine");

                resetBlink();
                repositionCursor();
            }
            else
            {
                auto anchor = textInputEnt.get<UiAnchor>();
                auto linePrefab = makeLinePrefab(ecsRef, anchor, currentLine);

                auto listViewComp = listViewEnt.get<VerticalLayout>();

                // Remove the old highlighted line
                if (oldLine > 0)
                {
                    auto entBefore = listViewComp->entities[oldLine - 1];
                    entBefore.get<Prefab>()->callHelper("UnfocusLine");
                }

                // Todo fix this
                // Highlight the new line
                linePrefab.get<Simple2DObject>()->setColors(constant::Vector4D{255.f, 0.f, 0.f, 255.f});

                // Update the line text for all the lines after the inserted line
                for (size_t i = currentLine - 1; i < lineNumber - 2; ++i)
                {
                    auto ent = listViewComp->entities[i];
                    ent.get<Prefab>()->callHelper("UpdateLineText", i + 2);
                }

                listViewComp->insertEntity(linePrefab.entity, currentLine - 1);

                resetBlink();

                // Edge case: the insertion into the list view is deferred, so
                // getLinePrefab(currentLine) still resolves to the previous line,
                // and prefab->getEntity("Text"/"TextBg") won't work on a freshly
                // created prefab either. Use the CompRefs returned by
                // makeLinePrefab directly: the Simple2DObject ref points to the
                // TextBg entity and the TTFText ref points to the Text entity.
                auto textBgId = linePrefab.get<Simple2DObject>().entityId;
                auto textId   = linePrefab.get<TTFText>().entityId;

                auto ca = cursorEntityRef.get<UiAnchor>();
                ca->setVerticalCenter({textBgId, AnchorType::VerticalCenter});
                ca->setLeftAnchor({textBgId, AnchorType::Left});
                ca->setZConstrain(PosConstrain{textId, AnchorType::Z, PosOpType::Add, 1.0f});
                ca->setLeftMargin(0.0f);

                cursorActive = true;
                cursorEntityRef.get<PositionComponent>()->setVisible(cursorVisible);
            }
        }
        else if (event.key == SDL_SCANCODE_BACKSPACE)
        {
            auto pf = getLinePrefab(currentLine);
            if (!pf) return;

            auto text = getLineText(currentLine);

            if (cursorCol > 0)
            {
                if (lcontrolPressed or rcontrolPressed)
                {
                    // Delete back to previous word boundary
                    size_t newCol = cursorCol;
                    while (newCol > 0 && text[newCol - 1] == ' ') newCol--;
                    while (newCol > 0 && text[newCol - 1] != ' ') newCol--;
                    text = text.substr(0, newCol) + text.substr(cursorCol);
                    cursorCol = newCol;
                }
                else
                {
                    text = text.substr(0, cursorCol - 1) + text.substr(cursorCol);
                    cursorCol--;
                }

                pf->callHelper("SetCurrentText", text);

                if (isVirtualMode)
                    fileLines[currentLine - 1] = text;

                resetBlink();
                repositionCursor();
                return;
            }
            // else do the line removal logic down here

            if (currentLine <= 1)
                return;

            if (isVirtualMode)
            {
                pf->callHelper("UnfocusLine");

                fileLines.erase(fileLines.begin() + currentLine - 1);
                lineNumber--;
                currentLine--;

                // Shift pool contents back from currentLine - 1 onwards
                for (size_t i = currentLine - 1; i < poolWindowStart + linePool.size() && i < fileLines.size(); i++)
                {
                    size_t pi = i - poolWindowStart;
                    linePool[pi].get<Prefab>()->callHelper("SetCurrentText", fileLines[i]);
                    linePool[pi].get<Prefab>()->callHelper("UpdateLineText", i + 1);
                }

                // Clear the last pool entry if it now falls beyond fileLines
                size_t poolWindowEnd = poolWindowStart + linePool.size();
                if (fileLines.size() < poolWindowEnd && fileLines.size() >= poolWindowStart)
                {
                    size_t pi = fileLines.size() - poolWindowStart;
                    linePool[pi].get<Prefab>()->callHelper("SetCurrentText", "");
                }

                updateSpacers();

                auto pfAfter = getLinePrefab(currentLine);
                if (pfAfter) pfAfter->callHelper("SetAsFocusLine");
            }
            else
            {
                auto listViewComp = listViewEnt.get<VerticalLayout>();

                // Unfocus the line that is about to be deleted.
                pf->callHelper("UnfocusLine");

                // Update the line text for all the lines after the removed line
                for (size_t i = currentLine; i < lineNumber - 1; ++i)
                {
                    auto ent = listViewComp->entities[i];
                    ent.get<Prefab>()->callHelper("UpdateLineText", i);
                }

                listViewComp->removeAt(currentLine - 1);

                lineNumber--;
                currentLine--;

                // Focus the new current line. Unlike the insert case, the
                // previous line already lives in entities[currentLine - 1],
                // so getLinePrefab() resolves correctly even while the
                // deferred removeAt hasn't been applied yet.
                auto pfAfter = getLinePrefab(currentLine);
                if (pfAfter)
                    pfAfter->callHelper("SetAsFocusLine");

                // Clamp cursor column to the new line's length.
                const std::string lineText = getLineText(currentLine);
                if (cursorCol > lineText.size())
                    cursorCol = lineText.size();

                resetBlink();
                repositionCursor();
            }
        }
        else if (event.key == SDL_SCANCODE_LEFT)
        {
            if (cursorCol > 0)
            {
                cursorCol--;
                resetBlink();
                repositionCursor();
            }
            else if (currentLine > 1)
            {
                focusLine(currentLine - 1);
                cursorCol = getLineText(currentLine).size();
                repositionCursor();
            }
        }
        else if (event.key == SDL_SCANCODE_RIGHT)
        {
            const std::string lineText = getLineText(currentLine);
            if (cursorCol < lineText.size())
            {
                cursorCol++;
                resetBlink();
                repositionCursor();
            }
            else if (currentLine < lineNumber - 1)
            {
                cursorCol = 0;
                focusLine(currentLine + 1);
                repositionCursor();
            }
        }
        else if (event.key == SDL_SCANCODE_UP)
        {
            if (currentLine > 1)
            {
                focusLine(currentLine - 1);
            }
        }
        else if (event.key == SDL_SCANCODE_DOWN)
        {
            if (currentLine < lineNumber - 1)
            {
                focusLine(currentLine + 1);
            }
        }
        else if (event.key == SDL_SCANCODE_LCTRL)
        {
            lcontrolPressed = true;
        }
        else if (event.key == SDL_SCANCODE_RCTRL)
        {
            rcontrolPressed = true;
        }
    }

    virtual void onProcessEvent(const OnSDLScanCodeReleased& event) override
    {
        if (event.key == SDL_SCANCODE_LCTRL)
        {
            lcontrolPressed = false;
        }
        else if (event.key == SDL_SCANCODE_RCTRL)
        {
            rcontrolPressed = false;
        }
    }

    virtual void onEvent(const TickEvent& event) override
    {
        blinkTimer += event.tick / 1000.0f;
        if (blinkTimer < BLINK_INTERVAL)
            return;

        blinkTimer -= BLINK_INTERVAL;
        cursorVisible = !cursorVisible;

        if (cursorActive)
            cursorEntityRef.get<PositionComponent>()->setVisible(cursorVisible);
    }

    void resetBlink()
    {
        blinkTimer = 0.0f;
        cursorVisible = true;
        if (cursorActive)
            cursorEntityRef.get<PositionComponent>()->setVisible(true);
    }

    std::string getLineText(size_t ln) const
    {
        if (isVirtualMode)
        {
            if (ln == 0 || ln - 1 >= fileLines.size()) return "";
            return fileLines[ln - 1];
        }
        auto& ents = listViewEnt.get<VerticalLayout>()->entities;
        if (ln == 0 || ln - 1 >= ents.size()) return "";
        auto pf = ents[ln - 1].get<Prefab>();
        if (!pf) return "";
        return pf->getEntity("Text")->get<TTFText>()->text;
    }

    void repositionCursor()
    {
        auto pf = getLinePrefab(currentLine);
        if (!pf)
        {
            cursorActive = false;
            cursorEntityRef.get<PositionComponent>()->setVisible(false);
            return;
        }

        auto textEnt = pf->getEntity("Text");
        auto textBg  = pf->getEntity("TextBg");
        auto textBgAnchor = textBg->get<UiAnchor>();
        auto ttfComp = textEnt->get<TTFText>();
        auto ttfSystem = ecsRef->getSystem<TTFTextSystem>();

        float cursorX = 0.0f;
        if (ttfSystem)
        {
            auto mapIt = ttfSystem->charactersMap.find(ttfComp->fontPath);
            if (mapIt != ttfSystem->charactersMap.end())
            {
                const auto& fontChars = mapIt->second;
                const std::string lineText = getLineText(currentLine);
                for (size_t i = 0; i < cursorCol && i < lineText.size(); i++)
                {
                    auto it = fontChars.find(lineText[i]);
                    if (it != fontChars.end())
                        cursorX += (it->second.advance >> 6) * ttfComp->scale;
                }
            }
        }

        auto ca = cursorEntityRef.get<UiAnchor>();
        ca->setVerticalCenter(textBgAnchor->verticalCenter);
        // ca->setTopAnchor({textBg->id, AnchorType::Top});
        ca->setLeftAnchor({textBg->id, AnchorType::Left});
        ca->setZConstrain(PosConstrain{textEnt->id, AnchorType::Z, PosOpType::Add, 1.0f});
        ca->setLeftMargin(cursorX);

        cursorActive = true;
        cursorEntityRef.get<PositionComponent>()->setVisible(cursorVisible);
    }

    virtual void init() override
    {
        textInputEnt = ecsRef->createEntity();

        auto ui = ecsRef->attach<PositionComponent>(textInputEnt);

        auto anchor = ecsRef->attach<UiAnchor>(textInputEnt);

        auto windowAnchor = ecsRef->getEntity("__MainWindow")->get<UiAnchor>();

        anchor->fillIn(windowAnchor);

        auto focused = ecsRef->attach<FocusableComponent>(textInputEnt);

        ecsRef->attach<MouseLeftClickComponent>(textInputEnt, makeCallable<OnFocus>(OnFocus{textInputEnt.id}) );

        StandardEvent event {"TerminalNewLine"};

        auto textInputComp = ecsRef->attach<TextInputComponent>(textInputEnt, event, "");

        auto listView = makeVerticalLayout(ecsRef, 0, 0, 500, 500, true);

        // listView.attach<Simple2DObject>(Shape2D::Square, constant::Vector4D{0.f, 192.f, 0.f, 255.f});

        auto listViewAnchor = listView.get<UiAnchor>();
        listViewAnchor->setTopMargin(90);

        listViewAnchor->fillIn(anchor);

        auto listViewComp = listView.get<VerticalLayout>();

        auto linePrefab = makeLinePrefab(ecsRef, anchor, lineNumber++);
        auto linePrefab2 = makeLinePrefab(ecsRef, anchor, lineNumber++);
        auto linePrefab3 = makeLinePrefab(ecsRef, anchor, lineNumber++);

        linePrefab3.get<Prefab>()->callHelper("SetAsFocusLine");

        // listViewComp->addEntity(linePrefab);

        // auto testCube = makeUiSimple2DShape(ecsRef, Shape2D::Square, 50, 50, constant::Vector4D{192.f, 0.f, 0.f, 255.f});

        // listViewComp->addEntity(testCube.entity);

        listViewComp->addEntity(linePrefab.entity);
        listViewComp->addEntity(linePrefab2.entity);
        listViewComp->addEntity(linePrefab3.entity);

        currentLine = 3;

        listViewEnt = listView.entity;

        // Single global cursor entity - positioned dynamically via repositionCursor()
        auto cursorShape = makeUiSimple2DShape(ecsRef, Shape2D::Square, 2.0f, 20.0f,
            constant::Vector4D{255.f, 255.f, 255.f, 255.f});
        cursorShape.get<PositionComponent>()->setVisible(false);
        cursorEntityRef = cursorShape.entity;

        cursorCol = 0;
        repositionCursor();

        auto file = makeTTFText(ecsRef, 10.0f, 5.0f, 12.0f, "light", "Open", 0.5);
        ecsRef->attach<MouseLeftClickComponent>(file.entity, makeCallable<OpenFileAction>());

        auto save = makeTTFText(ecsRef, 70.0f, 5.0f, 12.0f, "light", "Save", 0.5);
        ecsRef->attach<MouseLeftClickComponent>(save.entity, makeCallable<SaveFileAction>());
    }

    virtual void onEvent(const OpenFileAction& event) override
    {
        LOG_INFO("Context Menu", "Open file");

        char * lTheOpenFileName;
    	// char const * lFilterPatterns[1] = { "*.sc" };

        lTheOpenFileName = tinyfd_openFileDialog(
            "Open a scene file",
            "", // Starting path
            0, // Number of patterns
            nullptr, // List of patterns
            "file",
            1);

        if (lTheOpenFileName)
        {
            LOG_INFO("Context Menu", lTheOpenFileName);

            auto listViewComp = listViewEnt.get<VerticalLayout>();
            listViewComp->clear();

            fileLines.clear();
            linePool.clear();
            topSpacerEnt = {};
            bottomSpacerEnt = {};
            poolWindowStart = 0;
            isVirtualMode = false;
            lineNumber = 1;
            currentLine = 1;

            auto anchor = textInputEnt.get<UiAnchor>();

            // Read all lines into memory (strings only — fast)
            std::ifstream fileStream(lTheOpenFileName);
            std::string line;
            while (std::getline(fileStream, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();

                std::string expandedLine;
                expandedLine.reserve(line.size());
                for (char ch : line)
                {
                    if (ch == '\t')
                        expandedLine += "    ";
                    else
                        expandedLine += ch;
                }

                fileLines.push_back(std::move(expandedLine));
            }

            if (fileLines.size() > VIRTUAL_THRESHOLD)
            {
                isVirtualMode = true;

                // Top spacer: zero height — pool starts at the top of the file
                auto topSpacer = ecsRef->createEntity();
                ecsRef->attach<PositionComponent>(topSpacer)->setHeight(0.0f);
                ecsRef->attach<UiAnchor>(topSpacer);
                listViewComp->addEntity(topSpacer);
                topSpacerEnt = topSpacer;

                // Create fixed pool of prefab entities
                size_t poolSize = (fileLines.size() < POOL_SIZE) ? fileLines.size() : POOL_SIZE;
                linePool.reserve(poolSize);
                for (size_t i = 0; i < poolSize; i++)
                {
                    auto lp = makeLinePrefab(ecsRef, anchor, i + 1);
                    lp.get<Prefab>()->callHelper("SetCurrentText", fileLines[i]);
                    listViewComp->addEntity(lp.entity);
                    linePool.push_back(lp.entity);
                }
                poolWindowStart = 0;

                // Bottom spacer: covers lines not yet in the pool
                auto bottomSpacer = ecsRef->createEntity();
                float bottomH = static_cast<float>(fileLines.size() - poolSize) * LINE_HEIGHT;
                ecsRef->attach<PositionComponent>(bottomSpacer)->setHeight(bottomH);
                ecsRef->attach<UiAnchor>(bottomSpacer);
                listViewComp->addEntity(bottomSpacer);
                bottomSpacerEnt = bottomSpacer;

                lineNumber = fileLines.size() + 1;
                currentLine = 1;
                linePool[0].get<Prefab>()->callHelper("SetAsFocusLine");
            }
            else
            {
                // Small file: create all prefabs directly
                for (size_t i = 0; i < fileLines.size(); i++)
                {
                    auto lp = makeLinePrefab(ecsRef, anchor, i + 1);
                    lp.get<Prefab>()->callHelper("SetCurrentText", fileLines[i]);
                    listViewComp->addEntity(lp.entity);
                }

                lineNumber = fileLines.size() + 1;
                currentLine = 1;
            }
            // ecsRef->sendEvent(LoadScene{lTheOpenFileName});
        }
    }

    void fillText(std::string_view sv)
    {
        auto anchor = textInputEnt.get<UiAnchor>();

        auto listViewComp = listViewEnt.get<VerticalLayout>();

        while (not sv.empty())
        {
            // Find the next newline character
            size_t pos = sv.find_first_of("\n\r");

            // Extract the line (no copy, still a view)
            std::string_view line = sv.substr(0, pos);

            // Expand tabs into 4 spaces (requires building a string)
            std::string expandedLine;
            expandedLine.reserve(line.size() + 4); // rough guess to avoid frequent reallocs

            for (char ch : line)
            {
                if (ch == '\t') {
                    expandedLine += "    "; // replace tab with 4 spaces
                } else {
                    expandedLine += ch;
                }
            }

            auto linePrefab = makeLinePrefab(ecsRef, anchor, lineNumber++);

            linePrefab.get<Prefab>()->callHelper("SetCurrentText", expandedLine);

            listViewComp->addEntity(linePrefab.entity);

            if (pos == std::string_view::npos) break;

            // Handle CRLF (\r\n): if \r is found and followed by \n, skip both
            if (sv[pos] == '\r' && pos + 1 < sv.size() && sv[pos + 1] == '\n') {
                sv.remove_prefix(pos + 2);
            } else {
                sv.remove_prefix(pos + 1);
            }
        }

    }

    virtual void onEvent(const LayoutScrolledEvent& event) override
    {
        if (updatingPool || !isVirtualMode || event.id != listViewEnt.id)
            return;

        auto listViewComp = listViewEnt.get<VerticalLayout>();
        float yOffset = listViewComp->yOffset < 0.0f ? 0.0f : listViewComp->yOffset;

        size_t firstVisible = static_cast<size_t>(yOffset / LINE_HEIGHT);
        size_t targetStart = (firstVisible > SCROLL_BUFFER) ? firstVisible - SCROLL_BUFFER : 0;
        size_t maxStart = (fileLines.size() > linePool.size()) ? fileLines.size() - linePool.size() : 0;
        if (targetStart > maxStart) targetStart = maxStart;

        if (targetStart == poolWindowStart)
            return;

        poolWindowStart = targetStart;

        for (size_t i = 0; i < linePool.size(); i++)
        {
            size_t lineIdx = poolWindowStart + i;
            linePool[i].get<Prefab>()->callHelper("SetCurrentText", fileLines[lineIdx]);
            linePool[i].get<Prefab>()->callHelper("UpdateLineText", lineIdx + 1);
            linePool[i].get<Prefab>()->callHelper("UnfocusLine");
        }

        // Re-apply focus highlight if the cursor line is in the new window
        if (isLineInPool(currentLine))
        {
            linePool[currentLine - 1 - poolWindowStart].get<Prefab>()->callHelper("SetAsFocusLine");
            repositionCursor();
        }
        else
        {
            cursorActive = false;
            cursorEntityRef.get<PositionComponent>()->setVisible(false);
        }

        updateSpacers();
    }

    Prefab* getLinePrefab(size_t ln)
    {
        if (isVirtualMode)
        {
            size_t idx = ln - 1;
            if (idx < poolWindowStart || idx >= poolWindowStart + linePool.size())
                return nullptr;
            return linePool[idx - poolWindowStart].get<Prefab>();
        }
        auto& ents = listViewEnt.get<VerticalLayout>()->entities;
        if (ln == 0 || ln - 1 >= ents.size()) return nullptr;
        return ents[ln - 1].get<Prefab>();
    }

    bool isLineInPool(size_t ln) const
    {
        size_t idx = ln - 1;
        return idx >= poolWindowStart && idx < poolWindowStart + linePool.size();
    }

    void updateSpacers()
    {
        size_t poolWindowEnd = poolWindowStart + linePool.size();
        topSpacerEnt.get<PositionComponent>()->setHeight(static_cast<float>(poolWindowStart) * LINE_HEIGHT);
        float bottomH = (fileLines.size() > poolWindowEnd)
            ? static_cast<float>(fileLines.size() - poolWindowEnd) * LINE_HEIGHT
            : 0.0f;
        bottomSpacerEnt.get<PositionComponent>()->setHeight(bottomH);
    }

    void scrollPoolToLine(size_t ln)
    {
        size_t idx = ln - 1;
        size_t halfPool = linePool.size() / 2;
        size_t newStart = (idx > halfPool) ? idx - halfPool : 0;
        size_t maxStart = (fileLines.size() > linePool.size()) ? fileLines.size() - linePool.size() : 0;
        if (newStart > maxStart) newStart = maxStart;

        if (newStart == poolWindowStart)
            return;

        poolWindowStart = newStart;

        listViewEnt.get<VerticalLayout>()->yOffset = static_cast<float>(poolWindowStart) * LINE_HEIGHT;

        for (size_t i = 0; i < linePool.size(); i++)
        {
            size_t lineIdx = poolWindowStart + i;
            linePool[i].get<Prefab>()->callHelper("SetCurrentText", fileLines[lineIdx]);
            linePool[i].get<Prefab>()->callHelper("UpdateLineText", lineIdx + 1);
        }

        updateSpacers();

        // Notify LayoutSystem to update scrollbar and visibility culling
        updatingPool = true;
        ecsRef->sendEvent(LayoutScrolledEvent{listViewEnt.id});
        updatingPool = false;
    }

    virtual void onEvent(const SaveFileAction& event) override
    {
        LOG_INFO("Context Menu", "Save file");

        char * lTheSaveFileName;
    	// char const * lFilterPatterns[1] = { "*.sc" };

        lTheSaveFileName = tinyfd_saveFileDialog(
            "Save a scene file",
            "./file.tx",
            0,
            NULL,
            NULL);

        if (lTheSaveFileName)
        {
            LOG_INFO("Context Menu", lTheSaveFileName);
            // ecsRef->sendEvent(SaveScene{lTheSaveFileName});
        }
    }

    EntityRef textInputEnt;
    EntityRef listViewEnt;

    bool lcontrolPressed = false;
    bool rcontrolPressed = false;

    size_t lineNumber = 1;
    size_t currentLine = 1;

    // Virtual scroll state
    std::vector<std::string> fileLines;   // All file lines (strings only, no UI)
    std::vector<EntityRef> linePool;      // Fixed pool of reused prefab entities
    EntityRef topSpacerEnt;               // Spacer above the visible pool window
    EntityRef bottomSpacerEnt;            // Spacer below the visible pool window
    size_t poolWindowStart = 0;           // fileLines index of linePool[0]
    bool isVirtualMode = false;           // True when file is large enough to virtualise
    bool updatingPool = false;            // Guard against re-entrant LayoutScrolledEvent

    static constexpr size_t POOL_SIZE         = 80;    // Prefab entities in the pool
    static constexpr float  LINE_HEIGHT       = 25.0f; // Height of each line prefab (px)
    static constexpr size_t VIRTUAL_THRESHOLD = 500;   // Lines needed to trigger virtual mode
    static constexpr size_t SCROLL_BUFFER    = 20;    // Extra lines to keep above viewport
    static constexpr float  BLINK_INTERVAL   = 0.5f;  // Seconds per cursor on/off half-cycle

    // Cursor state
    EntityRef cursorEntityRef;       // Single global cursor entity
    size_t    cursorCol    = 0;      // 0-based char index within current line
    bool      cursorActive = false;  // True when the focused line is visible in the pool

    // Cursor blink state
    float blinkTimer    = 0.0f;
    bool  cursorVisible = true;
};

void initGame() {
    printf("Initializing engine ...\n");

#ifdef __EMSCRIPTEN__
        EM_ASM(
            console.error("Syncing... !");
            FS.mkdir('/save');
            console.error("Syncing... !");
            FS.mount(IDBFS, {autoPersist: true}, '/save');
            console.error("Syncing... !");
            FS.syncfs(true, function (err) {
                console.error("Synced !");
                if (err) {
                    console.error("Initial sync error:", err);
                }
            });
            console.error("Syncing... !");
        );
#endif

    mainWindow->initEngine();

    printf("Engine initialized ...\n");

    auto ttfSys = mainWindow->ecs->createSystem<TTFTextSystem>(mainWindow->masterRenderer);

    // Need to fix this
    ttfSys->registerFont("res/font/DejaVuSans/DejaVuSansMono.ttf", "light");
    ttfSys->registerFont("res/font/DejaVuSans/DejaVuSansMono-Bold.ttf", "bold");
    ttfSys->registerFont("res/font/DejaVuSans/DejaVuSansMono-Oblique.ttf", "italic");

    // mainWindow->masterRenderer->processTextureRegister();

    mainWindow->ecs->succeed<MasterRenderer, TTFTextSystem>();

    mainWindow->ecs->createSystem<TextHandlingSys>();

    mainWindow->ecs->dumbTaskflow();

    mainWindow->render();

    mainWindow->resize(820, 640);

    mainWindow->ecs->start();

    printf("Engine initialized\n");
}

// New function for syncing manually when needed
void syncFilesystem() {
#ifdef __EMSCRIPTEN__
    EM_ASM(
        FS.syncfs(false, function (err) {
            if (err) {
                console.error("Sync error:", err);
            } else {
                console.log("Filesystem synced.");
            }
        });
    );
#endif
}

void mainloop(void *arg) {
    if (not initialized.load())
        return;

    if (not init) {
        if (initThread) {
            printf("Joining thread...\n");

            initThread->join();

            delete initThread;

            printf("Thread joined...\n");
        }

        init = true;

        mainWindow->init(820, 640, false, static_cast<SDL_Window *>(arg));

        printf("Window init done !\n");

        initGame();
    }

    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        mainWindow->processEvents(event);
    }

    mainWindow->render();

#ifdef __EMSCRIPTEN__
    // Sync file system at a specific point instead of every frame
    if (event.type == SDL_QUIT)
    {
        syncFilesystem();
    }
#endif

    if (mainWindow->requestQuit()) {
        LOG_ERROR("Window", "RequestQuit");
        std::terminate();
    }
}

int GameApp::exec() {
#ifdef __EMSCRIPTEN__
    printf("Start init thread...\n");
    initThread = new std::thread(initWindow, appName);
    printf("Detach init thread...\n");

    SDL_Window *pWindow =
    SDL_CreateWindow("Hello Triangle Minimal",
                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                        820, 640,
                        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);

    emscripten_set_main_loop_arg(mainloop, pWindow, 0, 1);

#else
    LOG_THIS_MEMBER(DOM);

    initWindow(appName);

    mainWindow->init(820, 640, false);

    LOG_INFO(DOM, "Window init done !");

    initGame();

    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            mainWindow->processEvents(event);
        }

        mainWindow->render();

        if (mainWindow->requestQuit())
            break;
    }

    delete mainWindow;
#endif

    return 0;
}
