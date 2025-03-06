#include "fightscene.h"

#include "Systems/oneventcomponent.h"

#include "UI/ttftext.h" 

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <SDL2/SDL.h>
#include <SDL_opengles2.h>
// #include <SDL_opengl_glext.h>
#include <GLES2/gl2.h>
// #include <GLFW/glfw3.h>
#else
#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif
#include <GL/gl.h>
#endif

#include <random>

#include "inventory.h"

#include "gamefacts.h"

#include "locationscene.h"

namespace pg
{
    namespace
    {
        constexpr float SPEEDUNITTHRESHOLD = 999;

        double randomNumber()
        {
            // Making rng static ensures that it stays the same
            // Between different invocations of the function
            static std::default_random_engine rng;

            std::uniform_real_distribution<double> dist(0.0, 1.0); 
            return dist(rng); 
        }

        void applyPassiveToChara(EntitySystem *ecsRef, PassiveDatabase *database, Character& character, const std::vector<PassiveCall>& calls)
        {
            for (const auto& p : calls)
            {
                auto passive = database->resolvePassive(p);

                LOG_INFO("Fight Sys", "Applying passive [" << p.passiveName << "] to " << character.name << " !");

                if (passive.name != NOOPPASSIVE)
                {
                    PassiveEffect effect;

                    effect.call = p;
                    effect.effect = passive;
                    effect.info = p.info;

                    character.addPassive(effect, ecsRef);
                }
                else
                {
                    LOG_ERROR("PlayerCharacter", "Passive named: " << p.passiveName << " is not registered in the database !");
                }
            }
        }

        Spell getCastedSpell(Character* character)
        {
            const auto& behaviour = character->behaviour;

            switch (behaviour.type)
            {
            case CharaBehaviourType::Random:
            {
                // Todo create a vector with all the spells not in cd instead of using the character's spells list directly
                size_t rng = randomNumber() * (character->spells.size() + 1);

                // Todo check if the spell is not in cd before trying to cast it
                if (rng == character->spells.size())
                    return character->basicSpell;
                else
                    return character->spells[rng];

                break;
            }
            case CharaBehaviourType::OnlyAutoAttack:
            {
                return character->basicSpell;
                break;
            }
                
            case CharaBehaviourType::Pattern:
            default:
                LOG_ERROR("Fight System", "Fight behaviour type is not supported yet");
                break;
            }

            return Spell{"Error Spell", 0, 0};
        }

        bool getNextTarget(Character* caster, size_t& receiver, const std::vector<Character>& characters, const Spell& spell)
        {
            auto it = spell.properties.find("SpellType");

            if (it == spell.properties.end())
            {
                LOG_ERROR("Fight System", "Spell does not have SpellType property, cannot determine target");
                return false;
            }

            if (it->second.get<std::string>() == "Damage")
            {
                const Character* target = nullptr;

                for (const auto& chara : characters)
                {
                    if (caster->type != chara.type and not (chara.playingStatus == PlayingStatus::Dead))
                    {
                        if (target == nullptr)
                        {
                            target = &chara;
                        }
                        else
                        {
                            // Todo implement logic based on targetting logic of the chara
                        }
                    }
                }

                if (target != nullptr)
                {
                    receiver = target->id;
                    return true;
                }
            }
            else
            {
                LOG_ERROR("Fight System", "Spell [" << spell.name << "] SpellType property (" << it->second.get<std::string>() << ") is not supported yet");
                return false;
            }

            return false;
        }
    }

    void FightSystem::onEvent(const StartFight&)
    {
        LOG_INFO("Fight System", "Load aggro map");
        
        currentState = FightState::Start;

        for (auto& character : characters)
        {
            for (const auto& chara : characters)
            {
                if (character.id != chara.id)
                {
                    character.aggroMap[chara.id] = chara.stat.physicalAttack + chara.stat.magicalAttack;
                }
            }
        }

        currentState = FightState::NextTurn;

        timer->start();

        // calculateNextPlayingCharacter();
    }

    void FightSystem::onEvent(const EnemyNextTurn& event)
    {
        needToProcessEnemyNextTurn = true;

        currentPlayingCharacter = event.chara;
    }

    void FightSystem::onEvent(const PlayFightAnimationDone&)
    {
        // calculateNextPlayingCharacter();
    }

    void FightSystem::onEvent(const SpellCasted& event)
    {
        spellToResolve = event;

        spellToBeResolved = true;
    }

    void FightSystem::onEvent(const StartFightAtLocation& event)
    {
        clear();

        inEncounter = true;

        auto& location = event.location;

        currentLocation = location;

        // Todo make unrandom Encounters
        if (location.randomEncounter)
        {
            auto nbEncounters = location.possibleEnounters.size();

            if (nbEncounters == 0)
            {
                LOG_ERROR("Fight System", "Nb encounter = 0 for location: " << location.name << "! Cannont generate combat !");
                return;
            }

            int rng = randomNumber() * nbEncounters;

            auto encounter = location.possibleEnounters[rng];

            currentEncounter = encounter;

            for (const auto& chara : encounter.characters)
            {
                addCharacter(chara);
            }
        }

        for (const auto& chara : event.characters)
        {
            addCharacter(chara);
        }

        // onEvent(StartFight{});

        ecsRef->getSystem<SceneElementSystem>()->loadSystemScene<FightScene>();
    }

    void FightSystem::onEvent(const DeadPlayerEvent& event)
    {
        if (event.chara.type == CharacterType::Enemy)
        {
            ecsRef->sendEvent(IncreaseFact{event.chara.name + "_defeated"});
        }

        checkEndFight();
    }

    void FightSystem::init()
    {
        auto timerEnt = ecsRef->createEntity();

        timer = ecsRef->attach<Timer>(timerEnt);

        timer->interval = fightSpeed;

        timer->oneShot = true;
    }

    void FightSystem::clear()
    {
        currentPlayingCharacter = nullptr;

        characters.clear();

        needToProcessEnemyNextTurn = false;

        spellToBeResolved = false;

        inEncounter = false;
    }

    void FightSystem::execute()
    {
        if (timer->running)
            return;

        switch (currentState)
        {
            case FightState::NextTurn:
                calculateNextPlayingCharacter();
                break;

            case FightState::StartTurn:
                startTurn();
                break;

            case FightState::CastSpell:
                processNextTurn();
                break;

            case FightState::Start:
                LOG_INFO("Fight System", "Nothing to do");
                break;
            default:
                // LOG_ERROR("Fight System", "Unhandled FightState: " << static_cast<int>(currentState));
                break;
        }
    }

    void FightSystem::resolveSpell(size_t casterId, size_t receiverId, Spell* spell)
    {
        auto& caster = characters[casterId];
        auto& receiver = characters[receiverId];

        // Todo this is the heart of the fighting system

        float spellDamage = spell->baseDmg + spell->physicalMultipler * caster.stat.physicalAttack + spell->magicalMultipler * caster.stat.magicalAttack;

        auto rng = randomNumber() * 100;

        if (rng <= caster.stat.critChance)
        {
            spellDamage *= caster.stat.critDamage / 100.0f;

            ecsRef->sendEvent(FightMessageEvent{"Critical Hit !"});
        }

        receiver.receiveDmg(spellDamage, ecsRef);

        // Give more aggro to player who deal damage to the character
        receiver.aggroMap[casterId] += spellDamage;

        auto passiveDatabase = ecsRef->getSystem<PassiveDatabase>();

        applyPassiveToChara(ecsRef, passiveDatabase, caster, spell->applyToSelf);

        applyPassiveToChara(ecsRef, passiveDatabase, receiver, spell->applyToTarget);

        ecsRef->sendEvent(FightSystemUpdate{});

        ecsRef->sendEvent(PlayFightAnimation{receiverId, FightAnimationEffects::Hit});
    }

    void FightSystem::processNextTurn()
    {
        Character *chara = currentPlayingCharacter;

        // Todo here need to resolve spell and enemy AI

        if (chara->playingStatus == PlayingStatus::Dead)
        {
            LOG_ERROR("Fight Scene", "Character: " << chara->name << ", is dead but still try to play !");
            skipTurn();

            return;
        }


        Spell spell;

        if (chara->spells.size() == 0)
        {
            spell = chara->basicSpell;
        }
        else
        {
            spell = getCastedSpell(chara);
        }

        LOG_INFO("Fight Scene", "Character: " << chara->name << ", has " << chara->spells.size() << ", spells");

        size_t nbTargetHit = 0;

        while (nbTargetHit < spell.nbTargets)
        {
            size_t currentTarget = 0;
            auto targetHit = getNextTarget(chara, currentTarget, characters, spell);
            
            if (not targetHit)
            {
                LOG_WARNING("Fight Scene", "Character: " << chara->name << ", cannot find a target to cast spell: " << spell.name);
                break;
            }

            ecsRef->sendEvent(FightMessageEvent{chara->name + " cast " + spell.name});
            
            resolveSpell(chara->id, currentTarget, &spell);

            nbTargetHit++;
        }

        // If the mob couldn't hit anyone we need to at least send one PlayFightAnimation to update the state machine
        if (nbTargetHit == 0)
            skipTurn(chara->id, FightAnimationEffects::Nothing);
        else
        {
            // Todo next state here should be a wait for animation to finish + or timeout in case the scene becomes unavailable mid anim
            // Or state is next turn if the fight happens in background (FightScene in not the current scene)
            currentState = FightState::NextTurn;

            timer->start();
        }
    }

    void FightSystem::skipTurn(size_t id, const FightAnimationEffects& effect)
    {
        LOG_INFO("Fight System", "Character: " << characters[id].name << ", skip turn");

        // Play a miss like animation
        // ecsRef->sendEvent(PlayFightAnimation{id, effect});

        currentState = FightState::NextTurn;

        timer->start();
    }

    void FightSystem::addCharacter(Character character)
    {
        character.id = characters.size();

        LOG_INFO("Fight System", "Adding character: " << character.name << ", get id: " << character.id);

        // Set up the base states of the spell at startup
        for (auto& spell : character.spells)
        {
            spell.numberOfTurnsSinceLastUsed = spell.baseCooldown;
        }

        characters.push_back(character);
    }

    void FightSystem::calculateNextPlayingCharacter()
    {
        Character *nextPlayingCharacter = findNextPlayingCharacter();

        if (nextPlayingCharacter)
        {
            currentPlayingCharacter = nextPlayingCharacter;
            currentState = FightState::StartTurn;
            timer->start();
            return;
        }

        for (auto& chara : characters)
        {
            if (chara.playingStatus != PlayingStatus::Dead)
                chara.speedUnits += chara.stat.speed;
        }
    }

    void FightSystem::startTurn()
    {
        currentPlayingCharacter->speedUnits -= SPEEDUNITTHRESHOLD + 1;

        LOG_INFO("Fight System", "Next playing character: " << currentPlayingCharacter->name);

        ecsRef->sendEvent(FightMessageEvent{"Starting turn of " + currentPlayingCharacter->name});

        tickDownPassives(currentPlayingCharacter);

        currentState = FightState::CastSpell;
        timer->start();
    }

    void FightSystem::tickDownPassives(Character* character)
    {
        bool passiveWasRemoved = false;

        // Iterate through all the character passives to tick down their turn count
        for (int i = character->passives.size() - 1; i >= 0; --i)
        {
            auto& passive = character->passives[i];

            if (passive.info.type == PassiveType::CharacterEffect and passive.info.trigger == TriggerType::TurnStart)
            {
                passive.effect.applyOnCharacter.apply(*character, ecsRef);
            }

            // If the passive is not infinite (remainingTurn == -1) tick down a turn from it
            if (passive.info.remainingTurns != -1)
            {
                --passive.info.remainingTurns;

                // After ticking it down if it reaches 0 delete it
                if (passive.info.remainingTurns <= 0)
                {
                    // In case of a character boost we need to undo the boost first before deleting it
                    if (passive.info.type == PassiveType::CharacterEffect)
                    {
                        // passive.removeFromCharacter(*character);
                    }

                    std::string message = character->name + " is no longer under: " + passive.call.passiveName;

                    ecsRef->sendEvent(FightMessageEvent{message});

                    character->passives.erase(character->passives.begin() + i);

                    passiveWasRemoved = true;
                }
            }
        }

        if (passiveWasRemoved)
        {
            ecsRef->sendEvent(FightSystemUpdate{});
        }
    }

    Character* FightSystem::findNextPlayingCharacter()
    {
        Character* chara;
        bool characterTurn = false;

        // Character get a turn once their speed unit 
        float higherSpeedUnit = SPEEDUNITTHRESHOLD;

        for (auto& player : characters)
        {
            if (player.speedUnits > higherSpeedUnit)
            {
                LOG_INFO("Fight System", "Found character that need to play: " << player.name);
                characterTurn = true;
                higherSpeedUnit = player.speedUnits;

                chara = &player;
            }
        }

        if (characterTurn)
            return chara;
        else
            return nullptr;
    }

    void FightSystem::checkEndFight()
    {
        bool allAlliesDead = true;
        bool allEnemiesDead = true;

        for (const auto& chara : characters)
        {
            if (chara.playingStatus == PlayingStatus::Alive)
            {
                if (chara.type == CharacterType::Player)
                {
                    allAlliesDead = false;
                }
                else
                {
                    allEnemiesDead = false;
                }
            }
        }

        if (allAlliesDead)
        {
            currentState = FightState::End;
            processLose();
        }
        else if (allEnemiesDead)
        {
            currentState = FightState::End;
            processWin();
        }
    }

    void FightSystem::processWin()
    {
        LOG_INFO("FightSystem", "You Won !");

        if (inEncounter)
        {
            for (const auto& drop : currentEncounter.dropTable)
            {
                if (randomNumber() < drop.dropChance)
                {
                    auto item = drop.item;

                    item.nbItems = drop.quantity;

                    ecsRef->sendEvent(GainItem{item});
                }
            }
        }

        ecsRef->getSystem<SceneElementSystem>()->loadSystemScene<LocationScene>();
    }

    void FightSystem::processLose()
    {
        LOG_INFO("FightSystem", "You Lost !");

        ecsRef->getSystem<SceneElementSystem>()->loadSystemScene<LocationScene>();
    }

    struct SpellDoneClicked {};

    void FightScene::init()
    {
        fightSys = ecsRef->getSystem<FightSystem>();

        float xEnemyName = 80;

        auto& enemyNames = uiElements["Enemy Names"];
        auto& enemyHealths = uiElements["Enemy Health"];

        auto& playerNames = uiElements["Player Names"];
        auto& playerHealths = uiElements["Player Health"];

        float xPlayerName = 80; 

        size_t j = 0, k = 0;

        for (size_t i = 0; i < fightSys->characters.size(); ++i)
        {
            auto& character = fightSys->characters[i];

            if (character.type == CharacterType::Enemy)
            {
                auto enemyText = makeTTFText(this, xEnemyName, 20, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", character.name, 0.4);

                attach<MouseLeftClickComponent>(enemyText.entity, makeCallable<CharacterLeftClicked>(&character));

                enemyNames.push_back(enemyText.entity);

                auto enemyHealth = makeTTFText(this, xEnemyName, 50, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", std::to_string(character.stat.health), 0.4);

                enemyHealths.push_back(enemyHealth.entity);

                xEnemyName += 100;

                ++j;
            }
            else if (character.type == CharacterType::Player)
            {
                auto playerText = makeTTFText(this, xPlayerName, 120, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", character.name, 0.4);

                attach<MouseLeftClickComponent>(playerText.entity, makeCallable<CharacterLeftClicked>(&character));

                playerNames.push_back(playerText.entity);

                auto playerHealth = makeTTFText(this, xPlayerName, 170, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", std::to_string(character.stat.health), 0.4);

                playerHealths.push_back(playerHealth.entity);

                xPlayerName += 100;

                ++k;
            }
            
        }

        auto listView2 = makeListView(this, 620, 120, 300, 400);

        logView = listView2.get<ListView>();

        logView->stickToBottom = true;

        logView->spacing = 5;

        listenToEvent<FightSystemUpdate>([this](const FightSystemUpdate&) {
            needHealthBarUpdate = true;
        });

        listenToEvent<FightMessageEvent>([this](const FightMessageEvent& event) {
            writeInLog(event.message);
        });
    }

    void FightScene::startUp()
    {
        ecsRef->sendEvent(StartFight{});
    }

    void FightScene::execute()
    {
        if (needHealthBarUpdate)
        {
            updateHealthBars();
            needHealthBarUpdate = false;
        }

        if (animationToDo.size() == 0)
            return;

        for (auto anim : animationToDo)
        {
            //Run animation
        }

        animationToDo.clear();

        ecsRef->sendEvent(PlayFightAnimationDone{});
    }

    void FightScene::writeInLog(const std::string& message)
    {
        auto playerTurnText = makeTTFText(this, 0, 0, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", message, 0.4);

        auto ui = playerTurnText.get<PositionComponent>();

        ui->setVisibility(false);

        logView->addEntity(playerTurnText.entity);
    }

    void FightScene::updateHealthBars()
    {
        auto& enemyHealths = uiElements["Enemy Health"];
        auto& playerHealths = uiElements["Player Health"];

        size_t j = 0, k = 0;

        for (size_t i = 0; i < fightSys->characters.size(); ++i)
        {
            auto& chara = fightSys->characters[i];

            if (chara.type == CharacterType::Enemy)
            {
                enemyHealths[j].get<TTFText>()->setText(std::to_string(chara.stat.health));

                LOG_INFO("Fight Scene", "Health: " << enemyHealths[j].get<TTFText>()->text);

                ++j;
            }
            else if (chara.type == CharacterType::Player)
            {
                playerHealths[k].get<TTFText>()->setText(std::to_string(chara.stat.health));

                LOG_INFO("Fight Scene", "Health: " << playerHealths[k].get<TTFText>()->text);

                ++k;
            }
        }
    }
}