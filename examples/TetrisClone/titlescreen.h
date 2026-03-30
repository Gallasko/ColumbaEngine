#pragma once

#include "Scene/scenemanager.h"

#include "tetromino.h"

using namespace pg;

struct PlayClicked {};
struct HighscoreClicked {};
struct OptionClicked {};
struct KeyBindPressed
{ 
    KeyBindPressed(const TetrisConfig& key, size_t pos) : key(key), pos(pos) {}
    KeyBindPressed(const KeyBindPressed& other) : key(other.key), pos(other.pos) {}    

    KeyBindPressed& operator=(const KeyBindPressed& other)
    {
        key = other.key;
        pos = other.pos;

        return *this;
    }

    TetrisConfig key;
    size_t pos;
};

#include "2D/texture.h"

struct TitleScreen : public Scene
{
    virtual void init() override;

    CompRef<UiComponent> playButtonUi;
    CompRef<UiComponent> optionButtonUi;
    CompRef<UiComponent> highscoreButtonUi;

    void moveCursor();

    void hideHighscore();
    void showHighscore();

    void hideOption();
    void setTextureForOption(const SDL_Scancode& code, size_t option);
    void showOption();

    int cursorPos = 1;
    CompRef<UiComponent> cursorUi;

    bool highscoreShown = false;

    CompRef<UiComponent> highscoreTitleComp;
    EntityRef highscoreTable[10];

    bool optionShown = false;

    CompRef<UiComponent> optionTitleComp;
    CompRef<UiComponent> commandsText[8];
    EntityRef commands[8];
    SDL_Scancode currentCode[8];
    CompRef<UiComponent> modifyBindingComp;
    CompRef<UiComponent> modifingBindingComp;

    EntityRef buttonText;

    bool waitingForKey = false;
    KeyBindPressed waitedKey = {TetrisConfig::Start, 0};
};