#include "application.h"

#include <fstream>
#include <memory>

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

// Cached component references for a single line prefab. We hold CompRefs grabbed
// at creation time so view refresh code can drive the components directly
// instead of going through Prefab::getEntity(name), which is unreliable on
// freshly-created prefabs (deferred component attachment means the entity
// lookup may not yet find TTFText/Simple2DObject right after attach()).
struct LineView
{
    EntityRef               entity;         // The prefab entity
    CompRef<Prefab>         prefab;         // Prefab component (for helpers on existing prefabs)
    CompRef<TTFText>        textRef;        // Input (editable) TTFText
    CompRef<Simple2DObject> textBgRef;      // Dark bg behind input text (focus highlight)
    CompRef<UiAnchor>       textBgAnchor;   // textBg's UiAnchor (used for cursor positioning)
    CompRef<TTFText>        lineNumTextRef; // Line-number label TTFText
    CompRef<Simple2DObject> lineNumBgRef;   // Alternating line-number background
};

LineView makeLinePrefab(EntitySystem *ecsRef, CompRef<UiAnchor> anchor, size_t lineNumber)
{
    auto prefabEnt = makeAnchoredPrefab(ecsRef);
    auto prefab = prefabEnt.get<Prefab>();
    auto prefabAnchor = prefabEnt.get<UiAnchor>();

    auto color = getLineTextBgColor(lineNumber);

    auto square = makeUiSimple2DShape(ecsRef, Shape2D::Square, 25, 25, color);
    auto squareBg = square.get<Simple2DObject>();
    auto squareAnchor = square.get<UiAnchor>();

    prefabAnchor->setHeightConstrain(PosConstrain{square.entity.id, AnchorType::Height});
    prefabEnt.get<PositionComponent>()->setWidth(50);

    squareAnchor->setTopAnchor(prefabAnchor->top);
    squareAnchor->setLeftAnchor(prefabAnchor->left);

    // Todo replace the -50 here by an actual masking until ready of the ttf text component
    auto lineText = makeTTFText(ecsRef, 0, -50, 4, "light", std::to_string(lineNumber), 0.4, constant::Vector4D{0.f, 0.f, 0.f, 255.f});
    auto lineTextComp = lineText.get<TTFText>();

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

    return LineView{prefabEnt.entity, prefab, inputTTFText, s2Bg, s2Anchor, lineTextComp, squareBg};
}

struct OpenFileAction {};

struct SaveFileAction {};

// ---------------------------------------------------------------------------
// Editor model & command pattern
// ---------------------------------------------------------------------------

struct CursorPosition
{
    size_t line = 0; // 0-based line index
    size_t col  = 0; // 0-based column index (in bytes)
};

struct EditorState
{
    std::vector<std::string> lines = {""};
    CursorPosition            cursor;
};

class IEditorCommand
{
public:
    virtual ~IEditorCommand() = default;

    virtual void execute(EditorState& state) = 0;
    virtual void undo(EditorState& state)    = 0;
};

// Insert `text` at (line, col). Cursor ends up just after the inserted text.
class InsertTextCommand : public IEditorCommand
{
public:
    InsertTextCommand(size_t line, size_t col, std::string text)
        : line(line), col(col), text(std::move(text)) {}

    void execute(EditorState& state) override
    {
        auto& target = state.lines[line];
        target.insert(col, text);
        state.cursor = {line, col + text.size()};
    }

    void undo(EditorState& state) override
    {
        auto& target = state.lines[line];
        target.erase(col, text.size());
        state.cursor = {line, col};
    }

private:
    size_t      line;
    size_t      col;
    std::string text;
};

// Delete `deletedText` starting at (line, startCol). Cursor ends up at startCol.
// Undo restores the text and moves the cursor to just after the re-inserted range.
class DeleteRangeCommand : public IEditorCommand
{
public:
    DeleteRangeCommand(size_t line, size_t startCol, std::string deletedText)
        : line(line), startCol(startCol), deletedText(std::move(deletedText)) {}

    void execute(EditorState& state) override
    {
        auto& target = state.lines[line];
        target.erase(startCol, deletedText.size());
        state.cursor = {line, startCol};
    }

    void undo(EditorState& state) override
    {
        auto& target = state.lines[line];
        target.insert(startCol, deletedText);
        state.cursor = {line, startCol + deletedText.size()};
    }

private:
    size_t      line;
    size_t      startCol;
    std::string deletedText;
};

// Split the line at (line, col). The portion after `col` becomes a new line
// below. Cursor moves to the start of the new line.
class SplitLineCommand : public IEditorCommand
{
public:
    SplitLineCommand(size_t line, size_t col) : line(line), col(col) {}

    void execute(EditorState& state) override
    {
        auto& current  = state.lines[line];
        std::string rightPart = current.substr(col);
        current.erase(col);
        state.lines.insert(state.lines.begin() + line + 1, std::move(rightPart));
        state.cursor = {line + 1, 0};
    }

    void undo(EditorState& state) override
    {
        state.lines[line] += state.lines[line + 1];
        state.lines.erase(state.lines.begin() + line + 1);
        state.cursor = {line, col};
    }

private:
    size_t line;
    size_t col;
};

// Merge line (line+1) into line. `joinCol` is the length of `line` before the
// merge — that's where the cursor lands. Undo restores both lines.
class MergeLinesCommand : public IEditorCommand
{
public:
    MergeLinesCommand(size_t line, size_t joinCol) : line(line), joinCol(joinCol) {}

    void execute(EditorState& state) override
    {
        state.lines[line] += state.lines[line + 1];
        state.lines.erase(state.lines.begin() + line + 1);
        state.cursor = {line, joinCol};
    }

    void undo(EditorState& state) override
    {
        auto& current = state.lines[line];
        std::string rightPart = current.substr(joinCol);
        current.erase(joinCol);
        state.lines.insert(state.lines.begin() + line + 1, std::move(rightPart));
        state.cursor = {line + 1, 0};
    }

private:
    size_t line;
    size_t joinCol;
};

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

        auto lineNumberText = lineText->get<TTFText>()->text;

        LOG_INFO(DOM, "Clicked on line: " << lineNumberText);

        // Display number is 1-based; convert to 0-based line index.
        focusLine(std::stoul(lineNumberText) - 1);
    }

    virtual void onProcessEvent(const OnSDLTextInput& event) override
    {
        // Ignore text input while control is held to avoid inserting characters
        // on Ctrl+Z / Ctrl+Y shortcuts.
        if (lcontrolPressed or rcontrolPressed)
            return;

        if (editorState.cursor.line >= editorState.lines.size())
            return;

        executeCommand(std::make_unique<InsertTextCommand>(
            editorState.cursor.line, editorState.cursor.col, event.text));
    }

    void focusLine(size_t lineIdx)
    {
        if (lineIdx >= editorState.lines.size())
            return;

        editorState.cursor.line = lineIdx;

        // Clamp column to the new line's length.
        if (editorState.cursor.col > editorState.lines[lineIdx].size())
            editorState.cursor.col = editorState.lines[lineIdx].size();

        ensureCursorVisible();
        refreshEditorView();
    }

    virtual void onProcessEvent(const OnSDLScanCode& event) override
    {
        // Modifier keys — update state first so the rest of the handler sees
        // the up-to-date control flags.
        if (event.key == SDL_SCANCODE_LCTRL)
        {
            lcontrolPressed = true;
            return;
        }
        if (event.key == SDL_SCANCODE_RCTRL)
        {
            rcontrolPressed = true;
            return;
        }

        const bool ctrl = lcontrolPressed or rcontrolPressed;

        // Undo / redo shortcuts.
        if (ctrl and event.key == SDL_SCANCODE_Z)
        {
            undo();
            return;
        }
        if (ctrl and event.key == SDL_SCANCODE_Y)
        {
            redo();
            return;
        }

        if (editorState.cursor.line >= editorState.lines.size())
            return;

        if (event.key == SDL_SCANCODE_RETURN)
        {
            executeCommand(std::make_unique<SplitLineCommand>(
                editorState.cursor.line, editorState.cursor.col));
        }
        else if (event.key == SDL_SCANCODE_BACKSPACE)
        {
            const std::string& line = editorState.lines[editorState.cursor.line];

            if (editorState.cursor.col > 0)
            {
                size_t startCol = editorState.cursor.col;
                if (ctrl)
                {
                    // Delete back to previous word boundary.
                    while (startCol > 0 && line[startCol - 1] == ' ') startCol--;
                    while (startCol > 0 && line[startCol - 1] != ' ') startCol--;
                }
                else
                {
                    startCol = editorState.cursor.col - 1;
                }

                std::string deleted = line.substr(startCol, editorState.cursor.col - startCol);
                if (not deleted.empty())
                {
                    executeCommand(std::make_unique<DeleteRangeCommand>(
                        editorState.cursor.line, startCol, std::move(deleted)));
                }
            }
            else if (editorState.cursor.line > 0)
            {
                // Merge the current line into the previous one. joinCol is the
                // previous line's length (where the cursor should end up).
                size_t prevLineIdx = editorState.cursor.line - 1;
                size_t joinCol = editorState.lines[prevLineIdx].size();
                executeCommand(std::make_unique<MergeLinesCommand>(prevLineIdx, joinCol));
            }
        }
        else if (event.key == SDL_SCANCODE_LEFT)
        {
            if (editorState.cursor.col > 0)
            {
                editorState.cursor.col--;
            }
            else if (editorState.cursor.line > 0)
            {
                editorState.cursor.line--;
                editorState.cursor.col = editorState.lines[editorState.cursor.line].size();
            }
            ensureCursorVisible();
            refreshEditorView();
        }
        else if (event.key == SDL_SCANCODE_RIGHT)
        {
            const std::string& lineText = editorState.lines[editorState.cursor.line];
            if (editorState.cursor.col < lineText.size())
            {
                editorState.cursor.col++;
            }
            else if (editorState.cursor.line + 1 < editorState.lines.size())
            {
                editorState.cursor.line++;
                editorState.cursor.col = 0;
            }
            ensureCursorVisible();
            refreshEditorView();
        }
        else if (event.key == SDL_SCANCODE_UP)
        {
            if (editorState.cursor.line > 0)
            {
                focusLine(editorState.cursor.line - 1);
            }
        }
        else if (event.key == SDL_SCANCODE_DOWN)
        {
            if (editorState.cursor.line + 1 < editorState.lines.size())
            {
                focusLine(editorState.cursor.line + 1);
            }
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

    std::string getLineText(size_t lineIdx) const
    {
        if (lineIdx >= editorState.lines.size()) return "";
        return editorState.lines[lineIdx];
    }

    // ---- Command dispatch ------------------------------------------------

    void executeCommand(std::unique_ptr<IEditorCommand> cmd)
    {
        cmd->execute(editorState);
        undoStack.push_back(std::move(cmd));
        redoStack.clear();
        ensureCursorVisible();
        refreshEditorView();
    }

    void undo()
    {
        if (undoStack.empty()) return;

        auto cmd = std::move(undoStack.back());
        undoStack.pop_back();

        cmd->undo(editorState);
        redoStack.push_back(std::move(cmd));

        ensureCursorVisible();
        refreshEditorView();
    }

    void redo()
    {
        if (redoStack.empty()) return;

        auto cmd = std::move(redoStack.back());
        redoStack.pop_back();

        cmd->execute(editorState);
        undoStack.push_back(std::move(cmd));

        ensureCursorVisible();
        refreshEditorView();
    }

    // In virtual mode, scroll the pool window so the cursor line stays visible.
    void ensureCursorVisible()
    {
        if (not isVirtualMode) return;

        size_t cursorLine = editorState.cursor.line;
        if (cursorLine < poolWindowStart or
            cursorLine >= poolWindowStart + linePool.size())
        {
            scrollPoolToLine(cursorLine);
        }
    }

    // ---- View synchronisation -------------------------------------------

    void refreshEditorView()
    {
        if (editorState.lines.empty())
            editorState.lines.push_back("");

        // Clamp cursor to a valid position.
        if (editorState.cursor.line >= editorState.lines.size())
            editorState.cursor.line = editorState.lines.size() - 1;
        if (editorState.cursor.col > editorState.lines[editorState.cursor.line].size())
            editorState.cursor.col = editorState.lines[editorState.cursor.line].size();

        if (isVirtualMode)
            refreshVirtualView();
        else
            refreshNonVirtualView();

        resetBlink();
        repositionCursor();
    }

    void refreshVirtualView()
    {
        // Clamp pool window so it never runs off the end.
        size_t maxStart = (editorState.lines.size() > linePool.size())
            ? editorState.lines.size() - linePool.size() : 0;
        if (poolWindowStart > maxStart)
            poolWindowStart = maxStart;

        // Sync every pool prefab with the current model using cached CompRefs.
        for (size_t i = 0; i < linePool.size(); i++)
        {
            auto& view    = linePool[i];
            size_t lineIdx = poolWindowStart + i;

            if (lineIdx < editorState.lines.size())
            {
                view.textRef->setText(editorState.lines[lineIdx]);
                view.lineNumTextRef->setText(std::to_string(lineIdx + 1));
                view.lineNumBgRef->setColors(getLineTextBgColor(lineIdx + 1));
                view.textBgRef->setColors(constant::Vector4D{0.f, 0.f, 0.f, 255.f});
            }
            else
            {
                view.textRef->setText(std::string{});
            }
        }

        updateSpacers();

        // Highlight the focused line if it lives inside the current pool window.
        size_t cursorLine = editorState.cursor.line;
        if (cursorLine >= poolWindowStart and
            cursorLine <  poolWindowStart + linePool.size() and
            cursorLine <  editorState.lines.size())
        {
            linePool[cursorLine - poolWindowStart].textBgRef->setColors(
                constant::Vector4D{255.f, 0.f, 0.f, 255.f});
        }
    }

    void refreshNonVirtualView()
    {
        auto anchor       = textInputEnt.get<UiAnchor>();
        auto listViewComp = listViewEnt.get<VerticalLayout>();

        // Remove excess prefabs from the end first. Non-virtual mode only ever
        // appends/pops at the tail (lines inserted in the middle are handled by
        // rewriting the tail prefabs' text below), so line-number labels stay
        // stable for the lifetime of each prefab — no UpdateLineText needed.
        while (linePrefabs.size() > editorState.lines.size())
        {
            int lastIdx = static_cast<int>(linePrefabs.size()) - 1;
            listViewComp->removeAt(lastIdx);
            linePrefabs.pop_back();
        }

        // Add missing prefabs at the end of the layout. We use the CompRefs
        // returned by makeLinePrefab directly instead of going through
        // Prefab::getEntity / callHelper, which is unreliable on freshly
        // created prefabs because component attachment is deferred.
        while (linePrefabs.size() < editorState.lines.size())
        {
            size_t newIdx = linePrefabs.size();
            auto view = makeLinePrefab(ecsRef, anchor, newIdx + 1);
            listViewComp->addEntity(view.entity);
            linePrefabs.push_back(view);
        }

        // Sync text and focus state for every prefab via cached CompRefs.
        for (size_t i = 0; i < linePrefabs.size(); i++)
        {
            auto& view = linePrefabs[i];
            view.textRef->setText(editorState.lines[i]);
            view.textBgRef->setColors(constant::Vector4D{0.f, 0.f, 0.f, 255.f});
        }

        if (editorState.cursor.line < linePrefabs.size())
        {
            linePrefabs[editorState.cursor.line].textBgRef->setColors(
                constant::Vector4D{255.f, 0.f, 0.f, 255.f});
        }
    }

    void repositionCursor()
    {
        auto view = getLineView(editorState.cursor.line);
        if (not view)
        {
            cursorActive = false;
            cursorEntityRef.get<PositionComponent>()->setVisible(false);
            return;
        }

        auto ttfComp   = view->textRef;
        auto ttfSystem = ecsRef->getSystem<TTFTextSystem>();

        float cursorX = 0.0f;
        if (ttfSystem)
        {
            auto mapIt = ttfSystem->charactersMap.find(ttfComp->fontPath);
            if (mapIt != ttfSystem->charactersMap.end())
            {
                const auto& fontChars = mapIt->second;
                const std::string& lineText = editorState.lines[editorState.cursor.line];
                for (size_t i = 0; i < editorState.cursor.col && i < lineText.size(); i++)
                {
                    auto it = fontChars.find(lineText[i]);
                    if (it != fontChars.end())
                        cursorX += (it->second.advance >> 6) * ttfComp->scale;
                }
            }
        }

        auto ca = cursorEntityRef.get<UiAnchor>();
        ca->setVerticalCenter(view->textBgAnchor->verticalCenter);
        ca->setLeftAnchor({view->textBgRef.entityId, AnchorType::Left});
        ca->setZConstrain(PosConstrain{view->textRef.entityId, AnchorType::Z, PosOpType::Add, 1.0f});
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

        auto listViewAnchor = listView.get<UiAnchor>();
        listViewAnchor->setTopMargin(90);
        listViewAnchor->fillIn(anchor);

        listViewEnt = listView.entity;

        // Single global cursor entity - positioned dynamically via repositionCursor()
        auto cursorShape = makeUiSimple2DShape(ecsRef, Shape2D::Square, 2.0f, 20.0f,
            constant::Vector4D{255.f, 255.f, 255.f, 255.f});
        cursorShape.get<PositionComponent>()->setVisible(false);
        cursorEntityRef = cursorShape.entity;

        // Initial editor state: three empty lines, cursor on the last one.
        editorState.lines  = {"", "", ""};
        editorState.cursor = {2, 0};

        refreshEditorView();

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

            // Tear down any existing UI state.
            auto listViewComp = listViewEnt.get<VerticalLayout>();
            listViewComp->clear();

            linePrefabs.clear();
            linePool.clear();
            topSpacerEnt = {};
            bottomSpacerEnt = {};
            poolWindowStart = 0;
            isVirtualMode = false;
            undoStack.clear();
            redoStack.clear();
            editorState.lines.clear();
            editorState.cursor = {0, 0};

            auto anchor = textInputEnt.get<UiAnchor>();

            // Read all lines into the editor model (strings only — fast).
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

                editorState.lines.push_back(std::move(expandedLine));
            }

            if (editorState.lines.empty())
                editorState.lines.push_back("");

            if (editorState.lines.size() > VIRTUAL_THRESHOLD)
            {
                isVirtualMode = true;

                // Top spacer: zero height — pool starts at the top of the file.
                auto topSpacer = ecsRef->createEntity();
                ecsRef->attach<PositionComponent>(topSpacer)->setHeight(0.0f);
                ecsRef->attach<UiAnchor>(topSpacer);
                listViewComp->addEntity(topSpacer);
                topSpacerEnt = topSpacer;

                // Create fixed pool of prefab entities. refreshEditorView fills
                // their text below.
                size_t poolSize = (editorState.lines.size() < POOL_SIZE)
                    ? editorState.lines.size() : POOL_SIZE;
                linePool.reserve(poolSize);
                for (size_t i = 0; i < poolSize; i++)
                {
                    auto view = makeLinePrefab(ecsRef, anchor, i + 1);
                    listViewComp->addEntity(view.entity);
                    linePool.push_back(view);
                }
                poolWindowStart = 0;

                // Bottom spacer: covers lines not yet in the pool.
                auto bottomSpacer = ecsRef->createEntity();
                float bottomH = static_cast<float>(editorState.lines.size() - poolSize) * LINE_HEIGHT;
                ecsRef->attach<PositionComponent>(bottomSpacer)->setHeight(bottomH);
                ecsRef->attach<UiAnchor>(bottomSpacer);
                listViewComp->addEntity(bottomSpacer);
                bottomSpacerEnt = bottomSpacer;
            }

            // refreshEditorView creates any missing prefabs in non-virtual mode
            // and pushes the loaded text into the pool/prefabs.
            refreshEditorView();
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
        size_t maxStart = (editorState.lines.size() > linePool.size())
            ? editorState.lines.size() - linePool.size() : 0;
        if (targetStart > maxStart) targetStart = maxStart;

        if (targetStart == poolWindowStart)
            return;

        poolWindowStart = targetStart;

        // Scrolling is a pure view update — the model is unchanged, and we must
        // not auto-scroll back to the cursor, so call refreshEditorView without
        // ensureCursorVisible().
        refreshEditorView();
    }

    LineView* getLineView(size_t lineIdx)
    {
        if (isVirtualMode)
        {
            if (lineIdx < poolWindowStart || lineIdx >= poolWindowStart + linePool.size())
                return nullptr;
            return &linePool[lineIdx - poolWindowStart];
        }
        if (lineIdx >= linePrefabs.size()) return nullptr;
        return &linePrefabs[lineIdx];
    }

    bool isLineInPool(size_t lineIdx) const
    {
        return lineIdx >= poolWindowStart && lineIdx < poolWindowStart + linePool.size();
    }

    void updateSpacers()
    {
        if (not topSpacerEnt or not bottomSpacerEnt) return;

        size_t poolWindowEnd = poolWindowStart + linePool.size();
        topSpacerEnt.get<PositionComponent>()->setHeight(static_cast<float>(poolWindowStart) * LINE_HEIGHT);
        float bottomH = (editorState.lines.size() > poolWindowEnd)
            ? static_cast<float>(editorState.lines.size() - poolWindowEnd) * LINE_HEIGHT
            : 0.0f;
        bottomSpacerEnt.get<PositionComponent>()->setHeight(bottomH);
    }

    void scrollPoolToLine(size_t lineIdx)
    {
        size_t halfPool = linePool.size() / 2;
        size_t newStart = (lineIdx > halfPool) ? lineIdx - halfPool : 0;
        size_t maxStart = (editorState.lines.size() > linePool.size())
            ? editorState.lines.size() - linePool.size() : 0;
        if (newStart > maxStart) newStart = maxStart;

        if (newStart == poolWindowStart)
            return;

        poolWindowStart = newStart;

        listViewEnt.get<VerticalLayout>()->yOffset = static_cast<float>(poolWindowStart) * LINE_HEIGHT;

        // Notify LayoutSystem to update scrollbar and visibility culling. The
        // guard prevents onEvent(LayoutScrolledEvent) from recomputing the pool
        // window; refreshEditorView (called by the command dispatcher) will
        // repopulate the pool based on the new poolWindowStart.
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

    // Editor model (single source of truth) and command history.
    EditorState                                  editorState;
    std::vector<std::unique_ptr<IEditorCommand>> undoStack;
    std::vector<std::unique_ptr<IEditorCommand>> redoStack;

    // Non-virtual mode: one LineView per line in editorState.lines, kept in sync
    // locally because the VerticalLayout's own `entities` vector is only updated
    // when deferred layout events are processed.
    std::vector<LineView> linePrefabs;

    // Virtual scroll state
    std::vector<LineView> linePool;       // Fixed pool of reused line prefabs
    EntityRef topSpacerEnt;               // Spacer above the visible pool window
    EntityRef bottomSpacerEnt;            // Spacer below the visible pool window
    size_t poolWindowStart = 0;           // editorState.lines index of linePool[0]
    bool isVirtualMode = false;           // True when file is large enough to virtualise
    bool updatingPool = false;            // Guard against re-entrant LayoutScrolledEvent

    static constexpr size_t POOL_SIZE         = 80;    // Prefab entities in the pool
    static constexpr float  LINE_HEIGHT       = 25.0f; // Height of each line prefab (px)
    static constexpr size_t VIRTUAL_THRESHOLD = 500;   // Lines needed to trigger virtual mode
    static constexpr size_t SCROLL_BUFFER    = 20;    // Extra lines to keep above viewport
    static constexpr float  BLINK_INTERVAL   = 0.5f;  // Seconds per cursor on/off half-cycle

    // Cursor state
    EntityRef cursorEntityRef;       // Single global cursor entity
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
