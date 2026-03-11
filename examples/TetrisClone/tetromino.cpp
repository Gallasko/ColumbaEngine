#include "tetromino.h"

#include "2D/texture.h"

#include "logger.h"

#include <random>
#include <algorithm>

#include "Audio/audiosystem.h"

#include "titlescreen.h"

using namespace pg;

namespace
{
    static constexpr const char * const DOM = "Tetromino";

    std::string tetrominoTypeToString(const TetrominoType& type)
    {
        switch (type)
        {
            case TetrominoType::I:
            {
                return "I";
                break;
            }
            
            case TetrominoType::O:
            {
                return "O";
                break;
            }
                
            case TetrominoType::L:
            {
                return "J";
                break;
            }

            case TetrominoType::J:
            {
                return "L";
                break;
            }

            case TetrominoType::Z:
            {
                return "S";
                break;
            }

            case TetrominoType::S:
            {
                return "Z";
                break;
            }

            case TetrominoType::T:
            {
                return "T";
                break;
            }

            case TetrominoType::NOMINO:
            {
                return "Empty";
                break;
            }

            default:
            {
                return "Empty";
            }
        }

        return "Empty";
    }
}

Tetromino::Tetromino(const TetrominoType& type) : type(type)
{
    
}

void Tetromino::setMino(const TetrominoType& type)
{
    this->type = type;

    // Set rotation to 0 to always be the correct block
    rotation = 0;

    posX = 4;
    posY = 23;

    switch (type)
    {
        case TetrominoType::I:
        {
            nbPossibleRotation = 2;

            uint8_t rota0[4][4] = {{1, 0, 0, 0},
                                   {1, 0, 0, 0},
                                   {1, 0, 0, 0},
                                   {1, 0, 0, 0}};

            possibleRotation[0] = rota0;

            uint8_t rota1[4][4] = {{0, 0, 0, 0},
                                   {0, 0, 0, 0},
                                   {0, 0, 0, 0},
                                   {1, 1, 1, 1}};

            possibleRotation[1] = rota1;
            break;
        }
        
        case TetrominoType::O:
        {
            nbPossibleRotation = 1;

            uint8_t rota0[4][4] = {{0, 0, 0, 0},
                                   {0, 0, 0, 0},
                                   {1, 1, 0, 0},
                                   {1, 1, 0, 0}};

            possibleRotation[0] = rota0;
            break;
        }
            
        case TetrominoType::J:
        {
            nbPossibleRotation = 4;

            uint8_t rota0[4][4] = {{0, 0, 0, 0},
                                   {0, 0, 0, 0},
                                   {1, 0, 0, 0},
                                   {1, 1, 1, 0}};

            possibleRotation[0] = rota0;

            uint8_t rota1[4][4] = {{0, 0, 0, 0},
                                   {0, 1, 0, 0},
                                   {0, 1, 0, 0},
                                   {1, 1, 0, 0}};

            possibleRotation[1] = rota1;

            uint8_t rota2[4][4] = {{0, 0, 0, 0},
                                   {0, 0, 0, 0},
                                   {1, 1, 1, 0},
                                   {0, 0, 1, 0}};

            possibleRotation[2] = rota2;

            uint8_t rota3[4][4] = {{0, 0, 0, 0},
                                   {1, 1, 0, 0},
                                   {1, 0, 0, 0},
                                   {1, 0, 0, 0}};

            possibleRotation[3] = rota3;
            break;
        }

        case TetrominoType::L:
        {
            nbPossibleRotation = 4;

            uint8_t rota0[4][4] = {{0, 0, 0, 0},
                                   {0, 0, 0, 0},
                                   {0, 0, 1, 0},
                                   {1, 1, 1, 0}};

            possibleRotation[0] = rota0;

            uint8_t rota1[4][4] = {{0, 0, 0, 0},
                                   {1, 1, 0, 0},
                                   {0, 1, 0, 0},
                                   {0, 1, 0, 0}};

            possibleRotation[1] = rota1;

            uint8_t rota2[4][4] = {{0, 0, 0, 0},
                                   {0, 0, 0, 0},
                                   {1, 1, 1, 0},
                                   {1, 0, 0, 0}};

            possibleRotation[2] = rota2;

            uint8_t rota3[4][4] = {{0, 0, 0, 0},
                                   {1, 0, 0, 0},
                                   {1, 0, 0, 0},
                                   {1, 1, 0, 0}};

            possibleRotation[3] = rota3;
            break;
        }

        case TetrominoType::S:
        {
            nbPossibleRotation = 2;

            uint8_t rota0[4][4] = {{0, 0, 0, 0},
                                   {0, 0, 0, 0},
                                   {1, 1, 0, 0},
                                   {0, 1, 1, 0}};

            possibleRotation[0] = rota0;

            uint8_t rota1[4][4] = {{0, 0, 0, 0},
                                   {0, 1, 0, 0},
                                   {1, 1, 0, 0},
                                   {1, 0, 0, 0}};

            possibleRotation[1] = rota1;
            break;
        }

        case TetrominoType::Z:
        {
            nbPossibleRotation = 2;

            uint8_t rota0[4][4] = {{0, 0, 0, 0},
                                   {0, 0, 0, 0},
                                   {0, 1, 1, 0},
                                   {1, 1, 0, 0}};

            possibleRotation[0] = rota0;

            uint8_t rota1[4][4] = {{0, 0, 0, 0},
                                   {1, 0, 0, 0},
                                   {1, 1, 0, 0},
                                   {0, 1, 0, 0}};

            possibleRotation[1] = rota1;
            break;
        }

        case TetrominoType::T:
        {
            nbPossibleRotation = 4;

            uint8_t rota0[4][4] = {{0, 0, 0, 0},
                                   {0, 0, 0, 0},
                                   {0, 1, 0, 0},
                                   {1, 1, 1, 0}};

            possibleRotation[0] = rota0;

            uint8_t rota1[4][4] = {{0, 0, 0, 0},
                                   {0, 1, 0, 0},
                                   {1, 1, 0, 0},
                                   {0, 1, 0, 0}};

            possibleRotation[1] = rota1;

            uint8_t rota2[4][4] = {{0, 0, 0, 0},
                                   {0, 0, 0, 0},
                                   {1, 1, 1, 0},
                                   {0, 1, 0, 0}};

            possibleRotation[2] = rota2;

            uint8_t rota3[4][4] = {{0, 0, 0, 0},
                                   {1, 0, 0, 0},
                                   {1, 1, 0, 0},
                                   {1, 0, 0, 0}};

            possibleRotation[3] = rota3;
            break;
        }

        default:
        {
            nbPossibleRotation = 1;

            uint8_t rota0[4][4] = {{0, 0, 0, 0},
                                   {0, 0, 0, 0},
                                   {0, 0, 0, 0},
                                   {0, 0, 0, 0}};

            possibleRotation[0] = rota0;
            break;
        }
    }
}

RandomTetrominoGenerator::RandomTetrominoGenerator()
{
    bag1 = {TetrominoType::I, TetrominoType::O, TetrominoType::J, TetrominoType::L, TetrominoType::S, TetrominoType::Z, TetrominoType::T};
    bag2 = {TetrominoType::I, TetrominoType::O, TetrominoType::J, TetrominoType::L, TetrominoType::S, TetrominoType::Z, TetrominoType::T};
    // bag1 = {TetrominoType::I, TetrominoType::I, TetrominoType::I, TetrominoType::I, TetrominoType::I, TetrominoType::I, TetrominoType::I};
    // bag2 = {TetrominoType::I, TetrominoType::I, TetrominoType::I, TetrominoType::I, TetrominoType::I, TetrominoType::I, TetrominoType::I};

    std::random_device rd;
    std::mt19937 g(rd());

    index = std::rand() % 14;

    std::shuffle(bag1.begin(), bag1.end(), g);
    std::shuffle(bag2.begin(), bag2.end(), g);
}

TetrominoType RandomTetrominoGenerator::generateTetromino()
{
    TetrominoType type;

    if (index < 7)
        type = bag1[index];
    else
        type = bag2[index - 7];

    index++;

    if (index == 7)
    {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(bag1.begin(), bag1.end(), g);
    }
    else if (index == 14)
    {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(bag2.begin(), bag2.end(), g);

        index = 0;
    }
 
    return type;
}

TetrominoType RandomTetrominoGenerator::preview(size_t next)
{
    auto nextIndex = (index + next) % 14;

    if (nextIndex < 7)
        return bag1[nextIndex];
    else 
        return bag2[nextIndex - 7];
}

void GameCanvas::init()
{
    auto entity = createEntity();

    timer = ecsRef->attach<Timer>(entity);

    timer->interval = 500;

    timer->callback = makeCallable<FallTimeout>();

    auto entity2 = createEntity();

    lockTimer = ecsRef->attach<Timer>(entity2);

    lockTimer->interval = 500;

    lockTimer->callback = makeCallable<LockTimeout>();

    lockTimer->running = false;

    auto entity3 = createEntity();
    auto entity4 = createEntity();
    auto entity5 = createEntity();
    leftTimer = ecsRef->attach<Timer>(entity3);
    bottomTimer = ecsRef->attach<Timer>(entity4);
    rightTimer = ecsRef->attach<Timer>(entity5);

    leftTimer->interval = 80;
    bottomTimer->interval = 80;
    rightTimer->interval = 80;

    leftTimer->callback = makeCallable<MoveLeft>();
    bottomTimer->callback = makeCallable<MoveBottom>();
    rightTimer->callback = makeCallable<MoveRight>();

    leftTimer->running = false;
    bottomTimer->running = false;
    rightTimer->running = false;

    auto canvasTex = makeUiTexture(this, 24 + 24 * 10, 24 + 24 * 20, "Canvas");

    auto canvasUi = canvasTex.get<UiComponent>();

    canvasUi->setZ(1);

    canvasUi->setVisibility(true);

    auto nextTex = makeUiTexture(this, 111 * 4, 40 * 4, "Next");

    auto nextUi = nextTex.get<UiComponent>();

    nextUi->setZ(1);
    nextUi->setLeftAnchor(canvasUi->right);

    auto holdTex = makeUiTexture(this, 30 * 4, 40 * 4, "Hold");

    auto holdUi = holdTex.get<UiComponent>();

    holdUi->setZ(1);
    holdUi->setLeftAnchor(canvasUi->right);
    holdUi->setTopAnchor(nextUi->bottom);

    for (size_t i = 0; i < 4; i++)
    {
        for (size_t j = 0; j < 4; j++)
        {
            auto tex = makeUiTexture(this, 24, 24, "Empty");

            auto ui = tex.get<UiComponent>();

            ui->setBottomAnchor(holdUi->bottom);
            ui->setLeftAnchor(holdUi->left);

            ui->setBottomMargin(12 + 24 * i);
            ui->setLeftMargin(12 + 24 * j);

            ui->setZ(2);

            holdedCanvas[i][j] = tex.entity;
        }
    }

    for (size_t i = 0; i < 4; i++)
    {
        for (size_t j = 0; j < 16; j++)
        {
            auto tex = makeUiTexture(this, 24, 24, "Empty");

            auto ui = tex.get<UiComponent>();

            ui->setBottomAnchor(nextUi->bottom);
            ui->setLeftAnchor(nextUi->left);

            ui->setBottomMargin(12 + 24 * i);
            ui->setLeftMargin(12 + 24 * j + (static_cast<int>(j / 4) * 12));

            ui->setZ(2);

            nextCanvas[i][j] = tex.entity;
        }
    }

    for (size_t i = 0; i < 10; i++)
    {
        for (size_t j = 0; j < 24; j++)
        {
            auto tex = makeUiTexture(this, 24, 24, "Empty");

            auto ui = tex.get<UiComponent>();

            ui->setX(12 + 24 * i);
            ui->setY(-12 + 20 * 24 - 24 * j);
            ui->setZ(2);

            entityCanvas[i][j] = tex.entity;

            canvas[i][j] = -1;
        }
    }

    auto clStr = makeSentence(this, 0, 0, {"Lines: 0"});

    auto clUi = clStr.get<UiComponent>();

    clUi->setTopAnchor(holdUi->bottom);
    clUi->setLeftAnchor(canvasUi->right);

    clUi->setTopMargin(20);
    clUi->setLeftMargin(10);

    clearedLinesStr = clStr.entity;

    auto scStr = makeSentence(this, 0, 0, {"Score: 0"});

    auto scUi = scStr.get<UiComponent>();

    scUi->setTopAnchor(clUi->bottom);
    scUi->setLeftAnchor(canvasUi->right);

    scUi->setTopMargin(20);
    scUi->setLeftMargin(10);

    scoreStr = scStr.entity;

    auto highscoreValue = ecsRef->getSavedData("highscore" + std::to_string(0));

    std::string highscore;

    if (highscoreValue.isNumber() and highscoreValue.get<size_t>() != 0)
    {
        highscore = highscoreValue.toString();
    }
    else
    {
        highscore = "10000";
    }

    auto highscoreSent = makeSentence(this, 0, 0, {"Highscore: " + highscore});

    highscoreSent.get<UiComponent>()->setTopAnchor(scUi->bottom);
    highscoreSent.get<UiComponent>()->setLeftAnchor(scUi->left);

    highscoreSent.get<UiComponent>()->setTopMargin(20);

    highscoreText = highscoreSent.get<SentenceText>();

    auto restartSent = makeSentence(this, 0, 0, {"Press Start to retry or"});

    restartSent.get<UiComponent>()->setTopAnchor(highscoreSent.get<UiComponent>()->bottom);
    restartSent.get<UiComponent>()->setLeftAnchor(scUi->left);

    restartSent.get<UiComponent>()->setTopMargin(20);
    restartSent.get<UiComponent>()->setVisibility(false);

    restartText = restartSent.get<UiComponent>();

    auto restart2Sent = makeSentence(this, 0, 0, {"Press Quit to go to Main menu"});

    restart2Sent.get<UiComponent>()->setTopAnchor(restartSent.get<UiComponent>()->bottom);
    restart2Sent.get<UiComponent>()->setLeftAnchor(scUi->left);

    restart2Sent.get<UiComponent>()->setTopMargin(20);
    restart2Sent.get<UiComponent>()->setVisibility(false);

    restartText2 = restart2Sent.get<UiComponent>();

    listenToEvent<ConfiguredKeyEvent<TetrisConfig>>([this] (const ConfiguredKeyEvent<TetrisConfig>& event) { LOG_INFO(DOM, "Received tetris config"); onEventCallback(event.value); });

    listenToEvent<ConfiguredKeyEventReleased<TetrisConfig>>([this] (const ConfiguredKeyEventReleased<TetrisConfig>& event) {
        LOG_INFO(DOM, "Received tetris config");

        if (event.value == TetrisConfig::SlowDrop)
        {
            LOG_INFO(DOM, "Game canvas Down Released");

            bottomTimer->stop();
        }

        if (event.value == TetrisConfig::MoveLeft)
        {
            LOG_INFO(DOM, "Game canvas Left Released");

            leftTimer->stop();
        }

        if (event.value == TetrisConfig::MoveRight)
        {
            LOG_INFO(DOM, "Game canvas Right Released");

            rightTimer->stop();
        }
    });

    listenToEvent<FallTimeout>([this] (const FallTimeout& event) { onEventCallback(event); });
    listenToEvent<LockTimeout>([this] (const LockTimeout& event) { onEventCallback(event); });

    listenToEvent<MoveLeft>([this] (const MoveLeft&) { leftTimer->interval = 80; moveLeft(); });
    listenToEvent<MoveRight>([this] (const MoveRight&) { rightTimer->interval = 80; moveRight(); });
    listenToEvent<MoveBottom>([this] (const MoveBottom&) { bottomTimer->interval = 80; moveDown(); });
}

void GameCanvas::startUp()
{
    LOG_INFO(DOM, "Game canvas Start Pressed");
    // ecsRef->sendEvent(StartAudio{"res/audio/mainost.ogg"});

    restartText->setVisibility(false);
    restartText2->setVisibility(false);

    reset();

    running = true;

    timer->running = true;

    spawnTetromino();
}

bool GameCanvas::moveHelper(int x, int y)
{
    bool move = true;

    for (uint8_t i = 0; i < 4; i++)
    {
        for (uint8_t j = 0; j < 4; j++)
        {
            if (currentMino.possibleRotation[currentMino.rotation].pos[j][i] == 1)
            {
                if (canvas[currentMino.posX + x + i][currentMino.posY + y - j] != -1)
                {
                    move = false;
                }
            }
        }
    }

    if (move)
    {
        currentMino.posX += x;
        currentMino.posY += y;
    }

    std::string movedStr = move ? "YES" : "NO";

    LOG_INFO(DOM, "Move helper moved: " << movedStr);

    return move;
}

void GameCanvas::moveDown()
{
    bool move = true;

    if (currentMino.posY < 4)
    {
        move = false;
    }
    else
    {
        setMino(-1, "Empty");
        move = moveHelper(0, -1);
        setMino(static_cast<uint8_t>(currentMino.type), tetrominoTypeToString(currentMino.type));
    }

    if (not move)
    {
        startLockTimer();
    }
}

void GameCanvas::moveLeft()
{
    if (currentMino.posX > 0)
    {
        setMino(-1, "Empty");
        moveHelper(-1, 0);
        setMino(static_cast<uint8_t>(currentMino.type), tetrominoTypeToString(currentMino.type));
    }

    resetLockTimer();
}

void GameCanvas::moveUp()
{
    bool move = true;

    if (currentMino.posY >= 20)
    {
        move = false;
    }
    else
    {
        setMino(-1, "Empty");
        move = moveHelper(0, 1);
        setMino(static_cast<uint8_t>(currentMino.type), tetrominoTypeToString(currentMino.type));
    }

    if (not move)
    {
        startLockTimer();
    }
}

void GameCanvas::moveRight()
{
    if (not ((currentMino.type == TetrominoType::I and currentMino.rotation == 1) and currentMino.posX > 5) and not
           (((currentMino.type == TetrominoType::S and currentMino.rotation == 0) or
             (currentMino.type == TetrominoType::Z and currentMino.rotation == 0) or
             (currentMino.type == TetrominoType::T and currentMino.rotation == 0) or
             (currentMino.type == TetrominoType::T and currentMino.rotation == 2) or
             (currentMino.type == TetrominoType::J and currentMino.rotation == 0) or
             (currentMino.type == TetrominoType::J and currentMino.rotation == 2) or
             (currentMino.type == TetrominoType::L and currentMino.rotation == 0) or
             (currentMino.type == TetrominoType::L and currentMino.rotation == 2)) and currentMino.posX > 6) and not
            ((currentMino.type == TetrominoType::O or
             (currentMino.type == TetrominoType::S and currentMino.rotation == 1) or
             (currentMino.type == TetrominoType::Z and currentMino.rotation == 1) or
             (currentMino.type == TetrominoType::T and currentMino.rotation == 1) or
             (currentMino.type == TetrominoType::T and currentMino.rotation == 3) or
             (currentMino.type == TetrominoType::J and currentMino.rotation == 1) or
             (currentMino.type == TetrominoType::J and currentMino.rotation == 3) or
             (currentMino.type == TetrominoType::L and currentMino.rotation == 1) or
             (currentMino.type == TetrominoType::L and currentMino.rotation == 3)) and currentMino.posX > 7) and not
            ((currentMino.type == TetrominoType::I and currentMino.rotation == 0) and currentMino.posX > 8))
    {
        setMino(-1, "Empty");
        moveHelper(1, 0);
        setMino(static_cast<uint8_t>(currentMino.type), tetrominoTypeToString(currentMino.type));
    }

    resetLockTimer();
}

void GameCanvas::snapBottom()
{
    bool move = true;

    setMino(-1, "Empty");

    do
    {
        if (currentMino.posY < 4)
        {
            move = false;
        }
        else
        {
            move = moveHelper(0, -1);

            if (move)
            {
                score += 1 + nbClearedLines / 10;
                updateScore();
            }
        }
    } while(move);

    setMino(static_cast<uint8_t>(currentMino.type), tetrominoTypeToString(currentMino.type));

    onEventCallback(LockTimeout{});
}

void GameCanvas::rotate()
{
    if (not running)
        return;

    setMino(-1, "Empty");

    bool placeable = false;

    currentMino.rotation = (currentMino.rotation + 1) % currentMino.nbPossibleRotation;

    int offsetX = 0;

    if ((currentMino.type == TetrominoType::I and currentMino.rotation == 1) and currentMino.posX == 9)
    {
        offsetX = -3;
    }
    else if ((currentMino.type == TetrominoType::I and currentMino.rotation == 1) and currentMino.posX == 8)
    {
        offsetX = -2;
    }
    else if ((currentMino.type == TetrominoType::I and currentMino.rotation == 1) and currentMino.posX == 7)
    {
        offsetX = -1;
    }
    else if(((currentMino.type == TetrominoType::S and currentMino.rotation == 0) or
             (currentMino.type == TetrominoType::Z and currentMino.rotation == 0) or
             (currentMino.type == TetrominoType::T and currentMino.rotation == 0) or
             (currentMino.type == TetrominoType::T and currentMino.rotation == 2) or
             (currentMino.type == TetrominoType::J and currentMino.rotation == 0) or
             (currentMino.type == TetrominoType::J and currentMino.rotation == 2) or
             (currentMino.type == TetrominoType::L and currentMino.rotation == 0) or
             (currentMino.type == TetrominoType::L and currentMino.rotation == 2)) and currentMino.posX > 7)
    {
        offsetX = -1;
    }

    placeable = moveHelper(offsetX, 0);

    if (not placeable)
        currentMino.posX += offsetX;

    while (not placeable and currentMino.posY < 20)
    {
        placeable = moveHelper(0, 1);

        if (not placeable)
            currentMino.posY++;
    }

    setMino(static_cast<uint8_t>(currentMino.type), tetrominoTypeToString(currentMino.type));

    resetLockTimer();
}

void GameCanvas::hold()
{
    if (not running)
        return;

    if (swapped)
    {
        return;
    }

    resetLockTimer();

    setMino(-1, "Empty");

    setHoldedMino("Empty");

    if (holdedMino.type == TetrominoType::NOMINO)
    {
        holdedMino.setMino(currentMino.type);

        spawnTetromino();
    }
    else
    {
        auto temp = currentMino.type;
        currentMino.setMino(holdedMino.type);

        holdedMino.setMino(temp);

        swapped = true;
    }

    setHoldedMino(tetrominoTypeToString(holdedMino.type));

    setMino(static_cast<uint8_t>(currentMino.type), tetrominoTypeToString(currentMino.type));
}

void GameCanvas::setMino(int value, const std::string& texture)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        for (uint8_t j = 0; j < 4; j++)
        {
            if (currentMino.possibleRotation[currentMino.rotation].pos[j][i] == 1)
            {
                canvas[currentMino.posX + i][currentMino.posY - j] = value;
                entityCanvas[currentMino.posX + i][currentMino.posY - j]->get<Texture2DComponent>()->setTexture(texture);
            }
        }
    }
}

void GameCanvas::setHoldedMino(const std::string& texture)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        for (uint8_t j = 0; j < 4; j++)
        {
            if (holdedMino.possibleRotation[holdedMino.rotation].pos[j][i] == 1)
            {
                holdedCanvas[i][j]->get<Texture2DComponent>()->setTexture(texture);
            }
        }
    }
}

void GameCanvas::setNextMinos(const std::string& tex1, const std::string& tex2, const std::string& tex3, const std::string& tex4)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        for (uint8_t j = 0; j < 16; j++)
        {
            auto y = j % 4;
            uint8_t offset = j / 4;
            switch (offset)
            {
                case 0:
                {
                    if (nextMino[offset].possibleRotation[nextMino[offset].rotation].pos[y][i] == 1)
                    {
                        nextCanvas[i][j]->get<Texture2DComponent>()->setTexture(tex1);
                    }
                    break;
                }
                case 1:
                {
                    if (nextMino[offset].possibleRotation[nextMino[offset].rotation].pos[y][i] == 1)
                    {
                        nextCanvas[i][j]->get<Texture2DComponent>()->setTexture(tex2);
                    }
                    break;
                }
                case 2:
                {
                    if (nextMino[offset].possibleRotation[nextMino[offset].rotation].pos[y][i] == 1)
                    {
                        nextCanvas[i][j]->get<Texture2DComponent>()->setTexture(tex3);
                    }
                    break;
                }
                case 3:
                {
                    if (nextMino[offset].possibleRotation[nextMino[offset].rotation].pos[y][i] == 1)
                    {
                        nextCanvas[i][j]->get<Texture2DComponent>()->setTexture(tex4);
                    }
                    break;
                }
                default:
                {
                    LOG_ERROR(DOM, "SHOULD NEVER HAPPEND !");
                }
            }
        }
    }
}

void GameCanvas::startLockTimer()
{
    if (not lockTimer->running)
        lockTimer->currentTime = 0;

    lockTimer->running = true;
}

void GameCanvas::resetLockTimer()
{
    lockTimer->currentTime = 0;

    lockTimer->running = false;
}

void GameCanvas::onEventCallback(const OnSDLGamepadPressed& event)
{
    if (event.button == SDL_CONTROLLER_BUTTON_START)
    {
        LOG_INFO(DOM, "Game canvas Start Pressed");
        // ecsRef->sendEvent(StartAudio{"res/audio/mainost.ogg"});

        if (not running)
        {
            reset();

            running = true;

            timer->running = true;

            spawnTetromino();
        }
    }

    if (not running)
    {
        return;
    }

    if (event.button == SDL_CONTROLLER_BUTTON_X)
    {
        LOG_INFO(DOM, "Game canvas X Pressed");

        hold();
    }

    if (event.button == SDL_CONTROLLER_BUTTON_Y)
    {
        LOG_INFO(DOM, "Game canvas Y Pressed");

        printCanvas();
    }

    if (event.button == SDL_CONTROLLER_BUTTON_B)
    {
        LOG_INFO(DOM, "Game canvas B Pressed");

        rotate();
    }

    if (event.button == SDL_CONTROLLER_BUTTON_DPAD_UP)
    {
        LOG_INFO(DOM, "Game canvas Up Pressed");

        snapBottom();
    }

    if (event.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
    {
        LOG_INFO(DOM, "Game canvas Down Pressed");

        moveDown();
        // bottomTimer->running = true;
    }

    if (event.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT)
    {
        LOG_INFO(DOM, "Game canvas Left Pressed");

        moveLeft();

        // rightTimer->stop();

        // leftTimer->start();
    }

    if (event.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
    {
        LOG_INFO(DOM, "Game canvas Right Pressed");

        moveRight();
    }
}

void GameCanvas::onEventCallback(const OnSDLGamepadAxisChanged&)
{
    // if (event.axis == GAMEPAD_AXIS_LEFTX)
    // {
    //     if (event.value >= 0.16)
    //     {
    //         lTimer.stop()
    //         rVelocity = 5 * event.value;
    //         rTimer.start()
    //     }
    //     else if (event.value <= -0.16)
    //     {
    //         rTimer.stop()
    //         lVelocity = 5 * event.value;
    //         lTimer.start()
    //     }
    //     else
    //     {
    //         rTimer.stop()
    //         lTimer.stop()
    //     }
    // }
    // else if (event.axis == GAMEPAD_AXIS_LEFTY)
    // {
    //     if (event.value >= 0.16)
    //     {
    //         dTimer.stop()
    //         uVelocity = 5 * event.value;
    //         uTimer.start()
    //     }
    //     else if (event.value <= -0.16)
    //     {
    //         uTimer.stop()
    //         dVelocity = 5 * event.value;
    //         dTimer.start()
    //     }
    //     else
    //     {
    //         uTimer.stop()
    //         dTimer.stop()
    //     }
    // }
}

void GameCanvas::onEventCallback(const OnSDLScanCodeReleased& event)
{
    // Todo merge this function with GameCanvas::onEvent(const OnSDLGamepadPressed& event)
    if (event.key == SDL_SCANCODE_RETURN)
    {
        // ecsRef->sendEvent(StartAudio{"res/audio/mainost.ogg"});
        LOG_INFO(DOM, "Game canvas Start Pressed");

        if (not running)
        {
            reset();

            running = true;

            timer->running = true;

            spawnTetromino();
        }
    }

    if (not running)
    {
        return;
    }

    if (event.key == SDL_SCANCODE_Q)
    {
        LOG_INFO(DOM, "Game canvas X Pressed");

        hold();
    }

    if (event.key == SDL_SCANCODE_E)
    {
        LOG_INFO(DOM, "Game canvas Y Pressed");

        printCanvas();
    }

    if (event.key == SDL_SCANCODE_SPACE)
    {
        LOG_INFO(DOM, "Game canvas B Pressed");

        rotate();
    }

    if (event.key == SDL_SCANCODE_W)
    {
        LOG_INFO(DOM, "Game canvas Up Pressed");

        snapBottom();
    }

    if (event.key == SDL_SCANCODE_S)
    {
        LOG_INFO(DOM, "Game canvas Down Pressed");

        moveDown();
    }

    if (event.key == SDL_SCANCODE_A)
    {
        LOG_INFO(DOM, "Game canvas Left Pressed");

        moveLeft();
    }

    if (event.key == SDL_SCANCODE_D)
    {
        LOG_INFO(DOM, "Game canvas Right Pressed");

        moveRight();
    }
}

void GameCanvas::onEventCallback(const TetrisConfig& event)
{
    if (event == TetrisConfig::Quit)
    {
        LOG_INFO(DOM, "Quit requested");
        
        if (running)
        {
            gameOver();
        }

        ecsRef->getSystem<SceneElementSystem>()->loadSystemScene<TitleScreen>();
    }

    if (not running and event == TetrisConfig::Start)
    {
        LOG_INFO(DOM, "Game canvas Start Pressed");
        // ecsRef->sendEvent(StartAudio{"res/audio/mainost.ogg"});

        reset();

        running = true;

        timer->running = true;

        spawnTetromino();
    }

    if (not running)
    {
        return;
    }
    
    if (event == TetrisConfig::Hold)
    {
        LOG_INFO(DOM, "Game canvas X Pressed");

        hold();
    }

    if (event == TetrisConfig::RotateRight)
    {
        LOG_INFO(DOM, "Game canvas B Pressed");

        rotate();
    }

    if (event == TetrisConfig::FastDrop)
    {
        LOG_INFO(DOM, "Game canvas Up Pressed");

        snapBottom();
    }

    if (event == TetrisConfig::SlowDrop)
    {
        LOG_INFO(DOM, "Game canvas Down Pressed");

        if (not bottomTimer->running)
        {
            moveDown();
            bottomTimer->interval = 120;
            bottomTimer->start();
        }
    }

    if (event == TetrisConfig::MoveLeft)
    {
        LOG_INFO(DOM, "Game canvas Left Pressed");

        rightTimer->stop();
        if (not leftTimer->running)
        {
            moveLeft();
            leftTimer->interval = 120;
            leftTimer->start();
        }
    }

    if (event == TetrisConfig::MoveRight)
    {
        LOG_INFO(DOM, "Game canvas Right Pressed");

        leftTimer->stop();
        if (not rightTimer->running)
        {
            moveRight();
            rightTimer->interval = 120;
            rightTimer->start();
        }
    }
}

void GameCanvas::onEventCallback(const FallTimeout&)
{
    LOG_INFO(DOM, "Fall timeout: ");

    moveDown();
}

void GameCanvas::onEventCallback(const LockTimeout&)
{
    LOG_INFO(DOM, "Lock timeout: ");

    lockTimer->running = false;

    checkClearedLines();

    for (int i = 0; i < 10; i++)
    {
        if (canvas[i][20] != -1)
        {
            gameOver();

            return;
        }
    }

    swapped = false;

    spawnTetromino();

    showNext();
}

void GameCanvas::dropOneLine(uint8_t y, uint8_t nb)
{
    LOG_INFO(DOM, "Game canvas drop one line, line " << y << " becoming " << y + 1 << " and moving: " << nb << " lines");
 
    // If nb = 0 just clear the line
    if (nb == 0)
    {
        for (auto i = 0; i < 10; i++)
        {
            canvas[i][y] = -1;

            auto ent = entityCanvas[i][y];
            ent->get<Texture2DComponent>()->setTexture("Empty");
        }
    }
    else
    {
        for (auto j = 0; j < nb; j++)
        {
            for (auto i = 0; i < 10; i++)
            {
                canvas[i][y + j] = canvas[i][y + j + 1];

                canvas[i][y + j + 1] = -1;

                auto ent = entityCanvas[i][y + j];
                auto ent1 = entityCanvas[i][y + j + 1];

                auto tex = ent1->get<Texture2DComponent>();

                auto texName = tex->textureName;

                ent->get<Texture2DComponent>()->setTexture(texName);
                tex->setTexture("Empty");
            }
        }
    }    
}

void GameCanvas::dropLines(uint8_t y, uint8_t nb)
{
    for (auto j = y; j < 20; j++)
    {
        LOG_INFO(DOM, "Game canvas drop one line, line " << j - nb << " becoming " << j);

        for (auto i = 0; i < 10; i++)
        {
            canvas[i][j - nb] = canvas[i][j];
            canvas[i][j] = -1;

            auto ent = entityCanvas[i][j - nb];
            auto ent1 = entityCanvas[i][j];

            auto tex = ent1->get<Texture2DComponent>();

            ent->get<Texture2DComponent>()->setTexture(tex->textureName);
            tex->setTexture("Empty");
        }
    }
}

void GameCanvas::checkClearedLines()
{
    printCanvas();

    auto yTop = currentMino.posY;

    uint8_t nbDroppedLines = 0;

    for (auto j = 3; j >= 0; j--)
    {
        bool cleared = true;

        for (auto i = 0; i < 10; i++)
        {
            if (canvas[i][yTop - j - nbDroppedLines] == -1)
                cleared = false;
        }

        if (cleared)
        {
            LOG_INFO(DOM, "-------------------------------------");
            LOG_INFO(DOM, "Line " << yTop - j - nbDroppedLines << " cleared");

            dropOneLine(yTop - j - nbDroppedLines, j);

            nbDroppedLines++;
        };
    }

    if (nbDroppedLines > 0)
    {
        LOG_INFO(DOM, "Cleared lines: " << nbDroppedLines);
        
        dropLines(yTop + 1, nbDroppedLines);

        nbClearedLines += nbDroppedLines;

        auto multiplier = previousWasTetris and nbClearedLines == 4 ? 2.0f : 1.0f;

        score += nbClearedLines * std::pow(4, multiplier * nbDroppedLines);

        LOG_INFO(DOM, "Score: " << score);

        updateScore();

        if (nbClearedLines == 4)
        {
            previousWasTetris = true;
        }
        else
        {
            previousWasTetris = false;
        }
    }

    updateClearedLines();

    printCanvas();
}

void GameCanvas::showNext()
{
    setNextMinos("Empty", "Empty", "Empty", "Empty");

    for (size_t i = 0; i < 4; i++)
    {
        auto next = generator.preview(i);
        LOG_INFO(DOM, "Next mino i+" << i << ": is " << tetrominoTypeToString(next));
        nextMino[i].setMino(next);
    }

    setNextMinos(tetrominoTypeToString(nextMino[0].type), tetrominoTypeToString(nextMino[1].type), tetrominoTypeToString(nextMino[2].type), tetrominoTypeToString(nextMino[3].type));
}

void GameCanvas::spawnTetromino()
{
    LOG_INFO(DOM, "Generate Tetromino");

    resetLockTimer();

    auto type = generator.generateTetromino();

    showNext();

    currentMino.setMino(type);

    LOG_INFO(DOM, "New Tetromino: " << static_cast<uint8_t>(currentMino.type));

    for (auto i = 0; i < 4; i++)
    {
        for (auto j = 0; j < 4; j++)
        {
            if (currentMino.possibleRotation[currentMino.rotation].pos[j][i] != 0)
            {
                canvas[currentMino.posX + i][currentMino.posY - j] = static_cast<uint8_t>(currentMino.type);
                auto ent = entityCanvas[currentMino.posX + i][currentMino.posY - j];

                ent->get<Texture2DComponent>()->setTexture(tetrominoTypeToString(currentMino.type));
            }
        }
    }

    LOG_INFO(DOM, "Mino spawned");
}

void GameCanvas::updateClearedLines()
{
    clearedLinesStr->get<SentenceText>()->setText("Lines: " + std::to_string(nbClearedLines));

    auto interval = 500 - static_cast<uint32_t>(nbClearedLines / 10) * 25;

    timer->interval = interval < 50 ? 50 : interval;
}

void GameCanvas::updateScore()
{
    scoreStr->get<SentenceText>()->setText("Score: " + std::to_string(score));
}

void GameCanvas::printCanvas()
{
    std::string line = "";

    for (auto i = 23; i >= 0; i--)
    {
        if (i >= 10)
            line = std::to_string(i) + ": | ";
        else
            line = " " + std::to_string(i) + ": | ";

        for (auto j = 0; j < 10; j++)
        {
            if (canvas[j][i] == -1)
            {
                line += "-1 |";
            }
            else
            {
                line += " " + std::to_string(canvas[j][i]) + " |";
            }
        }

        LOG_INFO(DOM, line);
    }
}

void GameCanvas::reset()
{
    score = 0;
    nbClearedLines = 0;

    updateClearedLines();
    updateScore();

    for (size_t i = 0; i < 10; i++)
    {
        for (size_t j = 0; j < 24; j++)
        {
            auto ent = entityCanvas[i][j];

            ent->get<Texture2DComponent>()->setTexture("Empty");

            canvas[i][j] = -1;
        }
    }
}

void GameCanvas::gameOver()
{
    restartText->setVisibility(true);
    restartText2->setVisibility(true);

    ecsRef->sendEvent(StopAudio{});

    running = false;

    timer->running = false;

    for (size_t i = 0; i < 10; i++)
    {
        for (size_t j = 0; j < 24; j++)
        {
            auto ent = entityCanvas[i][j];

            ent->get<Texture2DComponent>()->setTexture("Ghost");
        }
    }

    checkHighscore();
}

void GameCanvas::checkHighscore()
{
    ElementType lastLine;
    ElementType lastScore;

    bool highscoreBeaten = false;

    for (size_t i = 0; i < 10; ++i)
    {
        auto highscoreLine = ecsRef->getSavedData("highscoreLine" + std::to_string(i));
        auto highscoreValue = ecsRef->getSavedData("highscore" + std::to_string(i));

        ElementType line;
        ElementType value;

        if (highscoreLine.isNumber() and highscoreLine.template get<int>() != 0)
        {
            line = highscoreLine;
        }
        else
        {
            line = 20 - i * 2;
        }

        if (highscoreValue.isNumber() and highscoreValue.get<size_t>() != 0)
        {
            value = highscoreValue.get<size_t>();
        }
        else
        {
            value = 10000 - i * 1000;
        }
        
        if (not highscoreBeaten and value.get<size_t>() < score)
        {
            lastLine = line;
            lastScore = value;

            ecsRef->sendEvent(SaveElementEvent{"highscoreLine" + std::to_string(i), nbClearedLines});
            ecsRef->sendEvent(SaveElementEvent{"highscore" + std::to_string(i), score});

            // Highscore beaten !
            if (i == 0)
            {
                highscoreText->setText("Highscore: " + std::to_string(score));
            }

            highscoreBeaten = true;
        }
        else if (highscoreBeaten)
        {
            ecsRef->sendEvent(SaveElementEvent{"highscoreLine" + std::to_string(i), lastLine});
            ecsRef->sendEvent(SaveElementEvent{"highscore" + std::to_string(i), lastScore});

            lastLine = line;
            lastScore = value;
        }
    }

    // Todo ask for name
    // Todo add name in highscore
}
