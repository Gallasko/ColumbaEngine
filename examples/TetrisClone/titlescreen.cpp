#include "titlescreen.h"

#include "ECS/system.h"
#include "Input/inputcomponent.h"

#include "Systems/coresystems.h"

#include "keyconfig.h"

#include "tetromino.h"
#include "2D/texture.h"

std::string scancodeToKey(const SDL_Scancode& code)
{
    std::string texName = "keys.";

    if (code >= SDL_SCANCODE_A and code <= SDL_SCANCODE_Z)
    {
        auto key = std::string(1, static_cast<char>(97 - SDL_SCANCODE_A + code));

        texName += key;
    }
    else if (code>= SDL_SCANCODE_1 and code <= SDL_SCANCODE_9)
    {
        auto key = std::string(1, static_cast<char>(49 - SDL_SCANCODE_1 + code));

        texName += key;
    }
    else if (code == SDL_SCANCODE_0)
    {
        texName += "0";
    }
    else if (code == SDL_SCANCODE_RIGHT)
    {
        texName += "right";
    }
    else if (code == SDL_SCANCODE_LEFT)
    {
        texName += "left";
    }
    else if (code == SDL_SCANCODE_DOWN)
    {
        texName += "down";
    }
    else if (code == SDL_SCANCODE_UP)
    {
        texName += "up";
    }
    else if (code == SDL_SCANCODE_RETURN)
    {
        texName += "enter";
    }
    else if (code == SDL_SCANCODE_ESCAPE)
    {
        texName += "esc";

    }
    else if (code == SDL_SCANCODE_SPACE)
    {
        texName += "space";

    }
    else if (code == SDL_SCANCODE_LSHIFT or code == SDL_SCANCODE_RSHIFT)
    {
        texName += "shift";
    }
    else
    {
        texName += "None";
    }

    return texName;
}

void TitleScreen::init()
{
    LOG_INFO("TitleScreen", "Loading title...");

    auto background = makeUiTexture(this, 792, 600, "titlescreenBackground");

    auto versionText = makeSentence(this, 0, 0, {"Version 1.0"});

    versionText.get<UiComponent>()->setLeftAnchor(background.get<UiComponent>()->left);
    versionText.get<UiComponent>()->setBottomAnchor(background.get<UiComponent>()->bottom);

    versionText.get<UiComponent>()->setRightMargin(5);
    versionText.get<UiComponent>()->setBottomMargin(5);
    versionText.get<UiComponent>()->setZ(1);

    auto usingEngine = makeSentence(this, 0, 0, {"Using PG Engine"});

    usingEngine.get<UiComponent>()->setRightAnchor(background.get<UiComponent>()->right);
    usingEngine.get<UiComponent>()->setBottomAnchor(background.get<UiComponent>()->bottom);

    usingEngine.get<UiComponent>()->setRightMargin(5);
    usingEngine.get<UiComponent>()->setBottomMargin(5);
    usingEngine.get<UiComponent>()->setZ(1);

    auto madeBy = makeSentence(this, 0, 0, {"Made by Pigeon Codeur"});

    madeBy.get<UiComponent>()->setRightAnchor(background.get<UiComponent>()->right);
    madeBy.get<UiComponent>()->setBottomAnchor(usingEngine.get<UiComponent>()->top);

    madeBy.get<UiComponent>()->setRightMargin(5);
    madeBy.get<UiComponent>()->setBottomMargin(15);

    madeBy.get<UiComponent>()->setZ(1);

    auto playButton = makeUiTexture(this, 2 * 86, 2 * 19, "playButton");

    playButtonUi = playButton.get<UiComponent>();

    playButtonUi->setX((792 / 2.0) - 86);
    playButtonUi->setY(400);
    playButtonUi->setZ(1);

    ecsRef->attach<MouseLeftClickComponent>(playButton.entity, makeCallable<PlayClicked>());

    listenToEvent<PlayClicked>([this](const PlayClicked&) { LOG_INFO("Titlescreen", "Playclicked"); ecsRef->getSystem<SceneElementSystem>()->loadSystemScene<GameCanvas>(); });

    // auto buttons = makeUiTexture(this, 13 * 4, 12 * 4, "keys.g");

    // buttons.get<UiComponent>()->setBottomAnchor(playButtonUi->top);
    // buttons.get<UiComponent>()->setLeftAnchor(playButtonUi->left);
    // buttons.get<UiComponent>()->setLeftMargin(18);
    // buttons.get<UiComponent>()->setBottomMargin(18);

    // // buttonText = buttons.get<Texture2DComponent>();
    // buttonText = buttons.entity;

    // listenToEvent<OnSDLScanCode>([this](const OnSDLScanCode& event) {
    //     LOG_INFO("Titlescreen", "Sdl Scancode");

    //     // std::string texName = std::string("keys.") + event.text;

    //     auto texName = scancodeToKey(event.key);

    //     if (texName == "keys.shift" or texName == "keys.enter")
    //     {
    //         buttonText.get<UiComponent>()->setWidth(28 * 4);
    //     }
    //     else if (texName == "keys.space")
    //     {
    //         buttonText.get<UiComponent>()->setWidth(29 * 4);
    //     }
    //     else if (texName == "keys.esc")
    //     {
    //         buttonText.get<UiComponent>()->setWidth(18 * 4);
    //     }
    //     else
    //     {
    //         buttonText.get<UiComponent>()->setWidth(13 * 4);
    //     }

    //     // LOG_INFO("Titlescreen", "Tex name " << texName << " from value: " << event.text);
    //     buttonText.get<Texture2DComponent>()->setTexture(texName);
    // });

    auto optionButton = makeUiTexture(this, 2 * 86, 2 * 19, "optionButton");

    optionButtonUi = optionButton.get<UiComponent>();

    optionButtonUi->setTopAnchor(playButtonUi->top);
    optionButtonUi->setLeftAnchor(playButtonUi->right);
    optionButtonUi->setLeftMargin(18);

    ecsRef->attach<MouseLeftClickComponent>(optionButton.entity, makeCallable<OptionClicked>());

    listenToEvent<OptionClicked>([this](const OptionClicked&) {
        LOG_INFO("Titlescreen", "Option");
        if (not optionShown)
        {
            hideHighscore();
            showOption();
        }
        else
        {
            hideOption();
        }
    });

    auto highscoreButton = makeUiTexture(this, 2 * 86, 2 * 19, "highscoreButton");

    ecsRef->attach<MouseLeftClickComponent>(highscoreButton.entity, makeCallable<HighscoreClicked>());

    listenToEvent<HighscoreClicked>([this](const HighscoreClicked&) {
        LOG_INFO("Titlescreen", "Highscore");
        if (not highscoreShown)
        {
            hideOption();
            showHighscore();
        }
        else
        {
            hideHighscore();
        }
    });

    highscoreButtonUi = highscoreButton.get<UiComponent>();

    highscoreButtonUi->setTopAnchor(optionButtonUi->top);
    highscoreButtonUi->setRightAnchor(playButtonUi->left);
    highscoreButtonUi->setRightMargin(18);

    auto cursor = makeUiTexture(this, 2 * 99, 2 * 31, "selectedButton");

    cursorUi = cursor.get<UiComponent>();

    auto optionTitle = makeSentence(this, 140, 150, {"Options :"});

    optionTitle.get<UiComponent>()->setX(792 / 2.0 - optionTitle.get<SentenceText>()->textWidth / 2);
    optionTitle.get<UiComponent>()->setZ(1);
    optionTitle.get<UiComponent>()->setVisibility(false);

    optionTitleComp = optionTitle.get<UiComponent>();

    auto control1 = makeSentence(this, 135, 195, {"Move Left:"});

    control1.get<UiComponent>()->setZ(1);
    control1.get<UiComponent>()->setVisibility(false);

    commandsText[0] = control1.get<UiComponent>();

    auto control2 = makeSentence(this, 135, 250, {"Move Right:"});

    control2.get<UiComponent>()->setTopAnchor(control1.get<UiComponent>()->bottom);
    control2.get<UiComponent>()->setTopMargin(25);
    control2.get<UiComponent>()->setZ(1);
    control2.get<UiComponent>()->setVisibility(false);

    commandsText[1] = control2.get<UiComponent>();

    auto control3 = makeSentence(this, 135, 250, {"Fast Drop:"});

    control3.get<UiComponent>()->setTopAnchor(control2.get<UiComponent>()->bottom);
    control3.get<UiComponent>()->setTopMargin(25);
    control3.get<UiComponent>()->setZ(1);
    control3.get<UiComponent>()->setVisibility(false);

    commandsText[2] = control3.get<UiComponent>();

    auto control4 = makeSentence(this, 135, 250, {"Slow Drop:"});

    control4.get<UiComponent>()->setTopAnchor(control3.get<UiComponent>()->bottom);
    control4.get<UiComponent>()->setTopMargin(25);
    control4.get<UiComponent>()->setZ(1);
    control4.get<UiComponent>()->setVisibility(false);

    commandsText[3] = control4.get<UiComponent>();

    auto control5 = makeSentence(this, 445, 195, {"Rotate:"});

    control5.get<UiComponent>()->setZ(1);
    control5.get<UiComponent>()->setVisibility(false);

    commandsText[4] = control5.get<UiComponent>();

    auto control6 = makeSentence(this, 445, 250, {"Hold:"});

    control6.get<UiComponent>()->setTopAnchor(control5.get<UiComponent>()->bottom);
    control6.get<UiComponent>()->setTopMargin(25);
    control6.get<UiComponent>()->setZ(1);
    control6.get<UiComponent>()->setVisibility(false);

    commandsText[5] = control6.get<UiComponent>();

    auto control7 = makeSentence(this, 445, 250, {"Start:"});

    control7.get<UiComponent>()->setTopAnchor(control6.get<UiComponent>()->bottom);
    control7.get<UiComponent>()->setTopMargin(25);
    control7.get<UiComponent>()->setZ(1);
    control7.get<UiComponent>()->setVisibility(false);

    commandsText[6] = control7.get<UiComponent>();

    auto control8 = makeSentence(this, 445, 250, {"Quit:"});

    control8.get<UiComponent>()->setTopAnchor(control7.get<UiComponent>()->bottom);
    control8.get<UiComponent>()->setTopMargin(25);
    control8.get<UiComponent>()->setZ(1);
    control8.get<UiComponent>()->setVisibility(false);

    commandsText[7] = control8.get<UiComponent>();

    ecsRef->attach<MouseLeftClickComponent>(control1.entity, makeCallable<KeyBindPressed>(TetrisConfig::MoveLeft, 0));
    ecsRef->attach<MouseLeftClickComponent>(control2.entity, makeCallable<KeyBindPressed>(TetrisConfig::MoveRight, 1));
    ecsRef->attach<MouseLeftClickComponent>(control3.entity, makeCallable<KeyBindPressed>(TetrisConfig::FastDrop, 2));
    ecsRef->attach<MouseLeftClickComponent>(control4.entity, makeCallable<KeyBindPressed>(TetrisConfig::SlowDrop, 3));
    ecsRef->attach<MouseLeftClickComponent>(control5.entity, makeCallable<KeyBindPressed>(TetrisConfig::RotateRight, 4));
    ecsRef->attach<MouseLeftClickComponent>(control6.entity, makeCallable<KeyBindPressed>(TetrisConfig::Hold, 5));
    ecsRef->attach<MouseLeftClickComponent>(control7.entity, makeCallable<KeyBindPressed>(TetrisConfig::Start, 6));
    ecsRef->attach<MouseLeftClickComponent>(control8.entity, makeCallable<KeyBindPressed>(TetrisConfig::Quit, 7));

    auto key1 = makeUiTexture(this, 29 * 3, 12 * 3, "keys.space");

    key1.get<UiComponent>()->setTopAnchor(control1.get<UiComponent>()->top);
    key1.get<UiComponent>()->setLeftAnchor(control1.get<UiComponent>()->right);
    key1.get<UiComponent>()->setTopMargin(-10);
    key1.get<UiComponent>()->setLeftMargin(10);
    key1.get<UiComponent>()->setZ(1);
    key1.get<UiComponent>()->setVisibility(false);

    ecsRef->attach<MouseLeftClickComponent>(key1.entity, makeCallable<KeyBindPressed>(TetrisConfig::MoveLeft, 0));

    commands[0] = key1.entity;

    auto key2 = makeUiTexture(this, 29 * 3, 12 * 3, "keys.space");

    key2.get<UiComponent>()->setTopAnchor(control2.get<UiComponent>()->top);
    key2.get<UiComponent>()->setLeftAnchor(control2.get<UiComponent>()->right);
    key2.get<UiComponent>()->setTopMargin(-10);
    key2.get<UiComponent>()->setLeftMargin(10);
    key2.get<UiComponent>()->setZ(1);
    key2.get<UiComponent>()->setVisibility(false);

    ecsRef->attach<MouseLeftClickComponent>(key2.entity, makeCallable<KeyBindPressed>(TetrisConfig::MoveRight, 1));

    commands[1] = key2.entity;

    auto key3 = makeUiTexture(this, 29 * 3, 12 * 3, "keys.space");

    key3.get<UiComponent>()->setTopAnchor(control3.get<UiComponent>()->top);
    key3.get<UiComponent>()->setLeftAnchor(control3.get<UiComponent>()->right);
    key3.get<UiComponent>()->setTopMargin(-10);
    key3.get<UiComponent>()->setLeftMargin(10);
    key3.get<UiComponent>()->setZ(1);
    key3.get<UiComponent>()->setVisibility(false);

    ecsRef->attach<MouseLeftClickComponent>(key3.entity, makeCallable<KeyBindPressed>(TetrisConfig::FastDrop, 2));

    commands[2] = key3.entity;

    auto key4 = makeUiTexture(this, 29 * 3, 12 * 3, "keys.space");

    key4.get<UiComponent>()->setTopAnchor(control4.get<UiComponent>()->top);
    key4.get<UiComponent>()->setLeftAnchor(control4.get<UiComponent>()->right);
    key4.get<UiComponent>()->setTopMargin(-10);
    key4.get<UiComponent>()->setLeftMargin(10);
    key4.get<UiComponent>()->setZ(1);
    key4.get<UiComponent>()->setVisibility(false);

    ecsRef->attach<MouseLeftClickComponent>(key4.entity, makeCallable<KeyBindPressed>(TetrisConfig::SlowDrop, 3));

    commands[3] = key4.entity;

    auto key5 = makeUiTexture(this, 29 * 3, 12 * 3, "keys.space");

    key5.get<UiComponent>()->setTopAnchor(control5.get<UiComponent>()->top);
    key5.get<UiComponent>()->setLeftAnchor(control5.get<UiComponent>()->right);
    key5.get<UiComponent>()->setTopMargin(-10);
    key5.get<UiComponent>()->setLeftMargin(10);
    key5.get<UiComponent>()->setZ(1);
    key5.get<UiComponent>()->setVisibility(false);

    ecsRef->attach<MouseLeftClickComponent>(key5.entity, makeCallable<KeyBindPressed>(TetrisConfig::RotateRight, 4));

    commands[4] = key5.entity;

    auto key6 = makeUiTexture(this, 29 * 3, 12 * 3, "keys.space");

    key6.get<UiComponent>()->setTopAnchor(control6.get<UiComponent>()->top);
    key6.get<UiComponent>()->setLeftAnchor(control6.get<UiComponent>()->right);
    key6.get<UiComponent>()->setTopMargin(-10);
    key6.get<UiComponent>()->setLeftMargin(10);
    key6.get<UiComponent>()->setZ(1);
    key6.get<UiComponent>()->setVisibility(false);

    ecsRef->attach<MouseLeftClickComponent>(key6.entity, makeCallable<KeyBindPressed>(TetrisConfig::Hold, 5));

    commands[5] = key6.entity;

    auto key7 = makeUiTexture(this, 29 * 3, 12 * 3, "keys.space");

    key7.get<UiComponent>()->setTopAnchor(control7.get<UiComponent>()->top);
    key7.get<UiComponent>()->setLeftAnchor(control7.get<UiComponent>()->right);
    key7.get<UiComponent>()->setTopMargin(-10);
    key7.get<UiComponent>()->setLeftMargin(10);
    key7.get<UiComponent>()->setZ(1);
    key7.get<UiComponent>()->setVisibility(false);

    ecsRef->attach<MouseLeftClickComponent>(key7.entity, makeCallable<KeyBindPressed>(TetrisConfig::Start, 6));

    commands[6] = key7.entity;

    auto key8 = makeUiTexture(this, 29 * 3, 12 * 3, "keys.space");

    key8.get<UiComponent>()->setTopAnchor(control8.get<UiComponent>()->top);
    key8.get<UiComponent>()->setLeftAnchor(control8.get<UiComponent>()->right);
    key8.get<UiComponent>()->setTopMargin(-10);
    key8.get<UiComponent>()->setLeftMargin(10);
    key8.get<UiComponent>()->setZ(1);
    key8.get<UiComponent>()->setVisibility(false);

    ecsRef->attach<MouseLeftClickComponent>(key8.entity, makeCallable<KeyBindPressed>(TetrisConfig::Quit, 7));

    for (size_t i = 0; i < 8; i++)
    {
        currentCode[i] = SDL_SCANCODE_SPACE;
    }

    listenToEvent<KeyBindPressed>([this](const KeyBindPressed& event) {
        LOG_INFO("Titlescreen", "KeyBindPressed");

        modifingBindingComp->setVisibility(true);

        waitedKey = event;

        waitingForKey = true;
    });

    listenToEvent<OnSDLScanCode>([this](const OnSDLScanCode& event) {
        LOG_INFO("Titlescreen", "KeyBindPressed");

        if (not waitingForKey)
        {
            return;
        }

        // const auto& def = scancodeMap.at(waitedKey.key);

        modifingBindingComp->setVisibility(false);

        ecsRef->sendEvent(ChangeKeyBind{waitedKey.key, currentCode[waitedKey.pos], event.key});

        setTextureForOption(event.key, waitedKey.pos);

        waitingForKey = false;
    });
    
    commands[7] = key8.entity;

    auto modifyBindingText = makeSentence(this, 80, 550, {"Click on any control to modify binding key"});

    modifyBindingText.get<UiComponent>()->setTopAnchor(control8.get<UiComponent>()->bottom);
    modifyBindingText.get<UiComponent>()->setTopMargin(15);

    modifyBindingText.get<UiComponent>()->setZ(1);
    modifyBindingText.get<UiComponent>()->setVisibility(false);

    modifyBindingComp = modifyBindingText.get<UiComponent>();

    auto modifingBindingText = makeSentence(this, 80, 550, {"Press a new key to replace old binding", 2.0, {255.0f, 86.0f, 86.0f, 255.0f}, {0.0f, 0.0f, 0.0f, 255.0f}, {255.0f, 255.0f, 255.0f, 255.0f}});

    modifingBindingText.get<UiComponent>()->setTopAnchor(playButton.get<UiComponent>()->bottom);
    modifingBindingText.get<UiComponent>()->setTopMargin(15);

    modifingBindingText.get<UiComponent>()->setZ(1);
    modifingBindingText.get<UiComponent>()->setVisibility(false);

    modifingBindingComp = modifingBindingText.get<UiComponent>();

    auto highscoreText = makeSentence(this, 120, 150, {"Highscores :"});

    highscoreText.get<UiComponent>()->setX(792 / 2.0 - highscoreText.get<SentenceText>()->textWidth / 2);
    highscoreText.get<UiComponent>()->setZ(1);
    highscoreText.get<UiComponent>()->setVisibility(false);

    highscoreTitleComp = highscoreText.get<UiComponent>();

    CompRef<UiComponent> lastUi = highscoreText.get<UiComponent>();

    for (size_t i = 0; i < 10; ++i)
    {
        auto ent = makeSentence(this, 145, 0, {"1. aaaaaaaaaaaa - 120000000245"});
        highscoreTable[i] = ent.entity;
        ent.get<UiComponent>()->setZ(1);
        ent.get<UiComponent>()->setVisibility(false);

        ent.get<UiComponent>()->setTopAnchor(lastUi->bottom);
        ent.get<UiComponent>()->setTopMargin(4);

        lastUi = ent.get<UiComponent>();
    }

    moveCursor();

    listenToEvent<ConfiguredKeyEvent<TetrisConfig>>([this](const ConfiguredKeyEvent<TetrisConfig>& event) {
        if (event.value == TetrisConfig::Start)
        {
            if (cursorPos == 0 and not highscoreShown)
            {
                hideOption();
                showHighscore();
            }
            else if (cursorPos == 1)
            {
                ecsRef->getSystem<SceneElementSystem>()->loadSystemScene<GameCanvas>();
            }
            else if (cursorPos == 2 and not optionShown)
            {
                hideHighscore();
                showOption();
            }
        }
        else if (event.value == TetrisConfig::Quit)
        {
            if (highscoreShown)
            {
                hideHighscore();
            }
            else if (optionShown)
            {
                hideOption();
            }
        }
        else if (event.value == TetrisConfig::MoveLeft)
        {
            cursorPos--;

            if (cursorPos < 0)
                cursorPos = 0;
            
            moveCursor();
        }
        else if (event.value == TetrisConfig::MoveRight)
        {
            cursorPos++;

            if (cursorPos > 2)
                cursorPos = 2;

            moveCursor();
        }
    });

    LOG_INFO("TitleScreen", "Title loaded");
}

void TitleScreen::moveCursor()
{
    if (cursorPos == 0)
    {
        cursorUi->setTopAnchor(highscoreButtonUi->top);
        cursorUi->setLeftAnchor(highscoreButtonUi->left);
    }
    else if (cursorPos == 1)
    {
        cursorUi->setTopAnchor(playButtonUi->top);
        cursorUi->setLeftAnchor(playButtonUi->left);
    }
    else if (cursorPos == 2)
    {
        cursorUi->setTopAnchor(optionButtonUi->top);
        cursorUi->setLeftAnchor(optionButtonUi->left);
    }

    cursorUi->setTopMargin(-12);
    cursorUi->setLeftMargin(-14);
}

void TitleScreen::hideHighscore()
{
    highscoreTitleComp->setVisibility(false);

    for (size_t i = 0; i < 10; i++)
    {
        highscoreTable[i].get<UiComponent>()->setVisibility(false);
    }

    highscoreShown = false;
}

void TitleScreen::showHighscore()
{
    highscoreTitleComp->setVisibility(true);

    for (size_t i = 0; i < 10; i++)
    {
        highscoreTable[i].get<UiComponent>()->setVisibility(true);

        auto highscoreValue = ecsRef->getSavedData("highscore" + std::to_string(i));
        auto highscoreLine  = ecsRef->getSavedData("highscoreLine" + std::to_string(i));

        std::string name = "line: ";
        std::string value;

        if (highscoreLine.isNumber() and highscoreLine.template get<int>() != 0)
        {
            name += highscoreLine.toString();
        }
        else
        {
            name += std::to_string(20 - i * 2);
        }

        if (highscoreValue.isNumber() and highscoreValue.template get<int>() != 0)
        {
            value = highscoreValue.toString();
        }
        else
        {
            value = std::to_string(10000 - i * 1000);
        }

        std::string highscore = std::to_string(i + 1) + ". " + name + " - " + value;

        highscoreTable[i].get<SentenceText>()->setText(highscore);
    }

    highscoreShown = true;
}

void TitleScreen::hideOption()
{
    waitingForKey = false;

    optionTitleComp->setVisibility(false);

    for (size_t i = 0; i < 8; i++)
    {
        commandsText[i]->setVisibility(false);
        commands[i].get<UiComponent>()->setVisibility(false);
    }

    modifyBindingComp->setVisibility(false);

    modifingBindingComp->setVisibility(false);

    optionShown = false;
}

void TitleScreen::setTextureForOption(const SDL_Scancode& code, size_t option)
{
    currentCode[option] = code;

    auto texName = scancodeToKey(code);

    if (texName == "keys.shift" or texName == "keys.enter")
    {
        commands[option].get<UiComponent>()->setWidth(28 * 3);
    }
    else if (texName == "keys.space")
    {
        commands[option].get<UiComponent>()->setWidth(29 * 3);
    }
    else if (texName == "keys.esc")
    {
        commands[option].get<UiComponent>()->setWidth(18 * 3);
    }
    else
    {
        commands[option].get<UiComponent>()->setWidth(13 * 3);
    }

    // LOG_INFO("Titlescreen", "Tex name " << texName << " from value: " << event.text);
    commands[option].get<Texture2DComponent>()->setTexture(texName);
}

void TitleScreen::showOption()
{
    optionTitleComp->setVisibility(true);

    std::vector<TetrisConfig> configurableKeys = {
        TetrisConfig::MoveLeft,
        TetrisConfig::MoveRight,
        TetrisConfig::FastDrop,
        TetrisConfig::SlowDrop,
        TetrisConfig::RotateRight,
        TetrisConfig::Hold,
        TetrisConfig::Start,
        TetrisConfig::Quit
    };

    for (size_t i = 0; i < 8; i++)
    {
        auto savedKey = ecsRef->getSavedData(scancodeMap.at(configurableKeys[i]).name);

        SDL_Scancode keyCode = scancodeMap.at(configurableKeys[i]).code;

        if (not savedKey.isEmpty() and savedKey.isNumber())
        {
            keyCode = static_cast<SDL_Scancode>(savedKey.template get<int>());
        }

        setTextureForOption(keyCode, i);
    }

    for (size_t i = 0; i < 8; i++)
    {
        commandsText[i]->setVisibility(true);
        commands[i].get<UiComponent>()->setVisibility(true);
    }

    modifyBindingComp->setVisibility(true);

    optionShown = true;
}