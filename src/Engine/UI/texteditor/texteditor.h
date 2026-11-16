#pragma once

#include "ECS/entitysystem.h"
#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "UI/ttftext.h"
#include "UI/focusable.h"
#include "Input/inputcomponent.h"
#include "Input/sdlevents.h"
#include "Input/clipboard.h"
#include "Systems/coresystems.h"

#include "textbuffer.h"
#include "wrap.h"
#include "markdown.h"
#include "search.h"
#include "markup.h"

namespace pg
{
    // Sent after every edit of an editor, and on Ctrl+S
    struct TextEditorChanged { _unique_id id; };
    struct TextEditorSaveRequested { _unique_id id; };

    /**
     * A multi-line Markdown text editor: the text, its wrapping, the find state and what the
     * system draws for it. Only the visual lines in view are drawn (a TTFText each, in a pool),
     * clipped to the editor's box.
     */
    struct TextEditorComponent : public Component
    {
        DEFAULT_COMPONENT_MEMBERS(TextEditorComponent)

        explicit TextEditorComponent(const std::string& text) : buffer(text) {}

        // New text: undo history cleared, saved, scrolled to the top
        void setText(const std::string& text)
        {
            buffer.setText(text);
            dirtyFlag = false;
            topVisual = 0;
            needsRefresh = true;
        }

        std::string text() const { return buffer.text(); }
        bool dirty() const { return dirtyFlag; }
        void markSaved() { dirtyFlag = false; needsRefresh = true; }

        texteditor::TextBuffer buffer;
        texteditor::WrapLayout wrap;
        std::vector<std::vector<texteditor::Span>> spans;

        std::string font = "editor";
        float scale = 0.4f;
        bool readOnly = false;
        bool showLineNumbers = true;

        // The first visual line in view
        size_t topVisual = 0;
        bool dirtyFlag = false;
        bool needsRefresh = true;
        bool followCursor = false;

        // Find
        bool findOpen = false;
        std::string query;
        std::vector<texteditor::Match> matches;
        size_t currentMatch = 0;

        // Mouse
        bool dragging = false;
        double lastClickTime = -10000.0;
        float wantedX = -1.0f;

        // What the system made to draw it, and the last texts set (to skip setText when unchanged)
        _unique_id background = 0;
        _unique_id gutter = 0;
        _unique_id cursorRect = 0;
        _unique_id findBar = 0;
        _unique_id findText = 0;
        std::vector<_unique_id> lineTexts;
        std::vector<std::string> lineMarkups;
        std::vector<_unique_id> numberTexts;
        std::vector<std::string> numberMarkups;
        std::vector<_unique_id> highlightRects;

        size_t laidOutRevision = static_cast<size_t>(-1);
        float laidOutWidth = -1.0f;
        double lastEditMs = 0.0;
        bool undoBroken = true;
    };

    template <typename Type>
    CompList<PositionComponent, UiAnchor, TextEditorComponent, FocusableComponent> makeTextEditor(Type* ecs, float x, float y, float width, float height, const std::string& text = "")
    {
        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);
        ui->setX(x);
        ui->setY(y);
        ui->setWidth(width);
        ui->setHeight(height);

        auto anchor = ecs->template attach<UiAnchor>(entity);
        auto editor = ecs->template attach<TextEditorComponent>(entity, text);
        auto focus = ecs->template attach<FocusableComponent>(entity);
        ecs->template attach<MouseLeftClickComponent>(entity, makeCallable<OnFocus>(OnFocus{entity.id}), MouseStateTrigger::OnPress);

        return {entity, ui, anchor, editor, focus};
    }

    struct TextEditorSystem : public System<Own<TextEditorComponent>, Listener<OnSDLTextInput>, Listener<OnSDLScanCode>, Listener<OnSDLScanCodeReleased>, Listener<OnMouseClick>, Listener<OnMouseMove>, Listener<OnMouseRelease>, Listener<OnSDLMouseWheel>, Listener<ClipboardText>, Listener<TickEvent>, Listener<OnFocus>, InitSys>
    {
        virtual std::string getSystemName() const override { return "Text Editor System"; }

        virtual void init() override {}

        virtual void onEvent(const OnSDLTextInput& event) override;
        virtual void onEvent(const OnSDLScanCode& event) override;
        virtual void onEvent(const OnSDLScanCodeReleased& event) override;
        virtual void onEvent(const OnMouseClick& event) override;
        virtual void onEvent(const OnMouseMove& event) override;
        virtual void onEvent(const OnMouseRelease& event) override;
        virtual void onEvent(const OnSDLMouseWheel& event) override;
        virtual void onEvent(const ClipboardText& event) override;
        virtual void onEvent(const TickEvent& event) override;
        virtual void onEvent(const OnFocus& event) override;

        virtual void execute() override;

    private:
        // Where an editor's parts are, in window coordinates
        struct Geometry
        {
            float x = 0, y = 0, width = 0, height = 0;
            float gutterWidth = 0, textX = 0, textY = 0, textWidth = 0, lineHeight = 16;
            size_t visibleLines = 1;
        };

        TextEditorComponent* focusedEditor();
        bool isFocused(_unique_id id);
        Geometry geometryOf(_unique_id id, TextEditorComponent* editor);
        texteditor::MeasureFn measureFor(TextEditorComponent* editor);

        void layout(TextEditorComponent* editor, const Geometry& g);
        void draw(TextEditorComponent* editor, const Geometry& g);
        void edited(TextEditorComponent* editor);
        void moveVertically(TextEditorComponent* editor, long lines, bool extend);
        void keepCursorInView(TextEditorComponent* editor, const Geometry& g);
        texteditor::TextPos positionAt(TextEditorComponent* editor, float px, float py);
        void refreshMatches(TextEditorComponent* editor, bool jump);
        void jumpToMatch(TextEditorComponent* editor, bool backwards);

        _unique_id makeRect(_unique_id clipper, float z, constant::Vector4D colour);
        _unique_id makeText(_unique_id clipper, float z, const std::string& font, float scale);
        void placeRect(_unique_id id, float x, float y, float w, float h, bool visible, const constant::Vector4D* colour = nullptr);
        void placeText(_unique_id id, float x, float y, const std::string& markup, std::string* last, bool visible);

        bool shiftDown = false;
        bool ctrlDown = false;
        // Milliseconds of a steady clock: TickEvent only says "time passed", not how much
        static double nowMs();

        bool cursorOn = true;
        double blinkStartMs = 0.0;
        Point2D mouse{0, 0};
    };
}
