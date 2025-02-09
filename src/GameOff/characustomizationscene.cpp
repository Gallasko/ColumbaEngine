#include "characustomizationscene.h"

#include "UI/ttftext.h" 

#include "UI/textinput.h"

#include "inventory.h"

namespace pg
{
    namespace
    {
        constexpr const char * const DOM = "Player Character";

        struct CreateNewPlayerButtonPressed {};

        struct SelectedCharacter
        {
            SelectedCharacter(PlayerCharacter *chara) : chara(chara) {}

            PlayerCharacter *chara;
        };

        struct ShowSkillBookUpgradeNeed
        {
            ShowSkillBookUpgradeNeed(SkillTree* sTree) : sTree(sTree) {}

            SkillTree* sTree;
        };

        struct ChangeSkillTreeInUse
        {
            ChangeSkillTreeInUse(SkillTree *sTree, size_t skillTreeSelected) : sTree(sTree), skillTreeSelected(skillTreeSelected) {}

            SkillTree *sTree;
            size_t skillTreeSelected;
        };

        struct LevelUpSkillTree {};
    }

    void PlayerCharacter::removeSkillTreeAt(size_t index)
    {
        auto sTreeName = skillTreeInUse[index];

        const auto& itSTree = std::find(learnedSkillTree.begin(), learnedSkillTree.end(), sTreeName);

        if (itSTree == learnedSkillTree.end())
        {
            LOG_ERROR("Player Chara", "SkillTree: " << sTreeName << " is not learnt by chara " << character.name);
            return;
        }

        const auto& skillTree = *itSTree;

        for (size_t i = 0; i < skillTree.currentLevel + 1; ++i)
        {
            character.stat -= skillTree.levelGains[i].stats;

            for (auto& spell : skillTree.levelGains[i].learntSpells)
            {
                auto it = std::find(character.spells.begin(), character.spells.end(), spell);

                if (it != character.spells.end())
                    character.spells.erase(it);
            }

            for (auto& passive : skillTree.levelGains[i].learntPassives)
            {
                auto it = std::find_if(character.passives.begin(), character.passives.end(), [passive](const PassiveEffect& effect) { return effect.call.passiveName == passive.passiveName; });

                if (it != character.passives.end())
                    character.passives.erase(it);
            }
        }
    }

    void PlayerCharacter::setSkillTreeInUse(SkillTree* sTree, size_t index)
    {
        // Remove buff from the old skill tree at index !
        removeSkillTreeAt(index);

        // Then replace the skill tree by the new one
        skillTreeInUse[index] = sTree->name;

        // Finally apply all the statbuff / new spell / new passive to the character
        applySkillTree(sTree);
    }

    void PlayerCharacter::applySkillTree(SkillTree* sTree)
    {
        for (size_t i = 0; i < sTree->currentLevel + 1; ++i)
        {
            applyLevelGain(sTree->levelGains[i]);
        }
    }

    void PlayerCharacter::applyLevelGain(const LevelIncrease& levelGain)
    {
        character.stat += levelGain.stats;

        learnSpells(levelGain);
    }

    void PlayerCharacter::learnSpells(const LevelIncrease& levelGain)
    {
        for (auto& spell : levelGain.learntSpells)
        {
            character.spells.push_back(spell);
        }

        if (levelGain.learntPassives.size() > 0 and ecsRef)
        {
            auto passiveDatabase = ecsRef->getSystem<PassiveDatabase>();

            for (auto& passiveCall : levelGain.learntPassives)
            {
                auto passive = passiveDatabase->resolvePassive(passiveCall);

                if (passive.name != NOOPPASSIVE)
                {
                    PassiveEffect effect;

                    effect.call = passiveCall;
                    effect.effect = passive;
                    effect.info = passiveCall.info;

                    character.passives.push_back(effect);
                }
                else
                {
                    LOG_ERROR("PlayerCharacter", "Passive named: " << passiveCall.passiveName << " is not registered in the database !");
                }
            }
        }
    }

    void PlayerCharacter::updateSpellList()
    {
        character.spells.clear();
        character.passives.clear();

        character.spells.push_back(BasicStrike{});

        for (size_t i = 0; i < MAXSKILLTREEINUSE; i++)
        {
            auto sTreeName = skillTreeInUse[i];

            const auto& itSTree = std::find(learnedSkillTree.begin(), learnedSkillTree.end(), sTreeName);

            if (itSTree == learnedSkillTree.end())
            {
                LOG_ERROR("Player Chara", "SkillTree: " << sTreeName << " is not learnt by chara " << character.name);
                return;
            }

            const auto& skillTree = *itSTree;

            for (size_t j = 0; j < skillTree.currentLevel + 1; ++j)
            {
                const auto& levelGain = skillTree.levelGains[j];
                learnSpells(levelGain);
            }
        }
    }

    void PlayerCharacter::onCreation(EntityRef entity)
    {
        ecsRef = entity->world();

        auto sys = ecsRef->getSystem<PlayerHandlingSystem>();

        if (sys)
            character.id = sys->lastGivenId++;
    }

    template <>
    void serialize(Archive& archive, const PlayerCharacter& value)
    {
        archive.startSerialization(PlayerCharacter::getType());

        serialize(archive, "chara", value.character);

        auto nbSkillTreeLearned = value.learnedSkillTree.size();

        serialize(archive, "nbSkillTreeLearned", nbSkillTreeLearned);

        size_t i = 0;

        for (const auto& sTree : value.learnedSkillTree)
        {
            serialize(archive, "sTree" + std::to_string(i), sTree);

            ++i;
        }

        for (i = 0; i < MAXSKILLTREEINUSE; ++i)
        {
            auto sTreeInUse = value.skillTreeInUse[i];

            serialize(archive, "sTreeInUse" + std::to_string(i), sTreeInUse);
        }

        archive.endSerialization();
    }

    template <>
    PlayerCharacter deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_MILE(DOM, "Deserializing Player Character");

            PlayerCharacter data;

            data.character = deserialize<Character>(serializedString["chara"]);

            auto nbSkillLearned = deserialize<size_t>(serializedString["nbSkillTreeLearned"]);

            data.learnedSkillTree.clear();

            for (size_t i = 0; i < nbSkillLearned; i++)
            {
                auto sTree = deserialize<SkillTree>(serializedString["sTree" + std::to_string(i)]);

                data.learnedSkillTree.push_back(sTree);
            }

            if (std::find(data.learnedSkillTree.begin(), data.learnedSkillTree.end(), NoneSkillTree{}) == data.learnedSkillTree.end())
            {
                data.learnedSkillTree.push_back(NoneSkillTree{});
            }

            for (size_t i = 0; i < MAXSKILLTREEINUSE; ++i)
            {
                std::string sTreeName = "None";
                
                defaultDeserialize(serializedString, "sTreeInUse" + std::to_string(i), sTreeName);

                data.skillTreeInUse[i] = sTreeName;

                auto it = std::find(data.learnedSkillTree.begin(), data.learnedSkillTree.end(), sTreeName);

                if (it != data.learnedSkillTree.end())
                {
                    data.skillTreeInUse[i] = sTreeName;
                }
                else
                {
                    LOG_ERROR("Player character", "Skill tree is not learnt for the player: " << sTreeName);
                }
            }

            return data;
        }

        return PlayerCharacter{};
    }

    void PlayerCustomizationScene::init()
    {
        auto createNewPlayer = makeTTFText(this, 10, 10, "res/font/Inter/static/Inter_28pt-Light.ttf", "Create new player", 0.6);

        attach<MouseLeftClickComponent>(createNewPlayer.entity, makeCallable<CreateNewPlayerButtonPressed>());

        listenToEvent<CreateNewPlayerButtonPressed>([this](const CreateNewPlayerButtonPressed&) {
            // Can't use the scene function here to create and attach as the entity needs to outlive the scene
            auto player = ecsRef->createEntity();

            ecsRef->attach<PlayerCharacter>(player);

            ecsRef->sendEvent(NewPlayerCreated{player});
        });

        listenToEvent<NewPlayerCreated>([this](const NewPlayerCreated& event) {
            auto player = event.entity.get<PlayerCharacter>();

            addPlayerToListView(player.component);
        });

        listenToEvent<SelectedCharacter>([this](const SelectedCharacter& event) {
            currentPlayer = event.chara;

            LOG_INFO("CharacterLeftClicked", "Current player id: " << currentPlayer->character.id);

            characterName.get<TTFText>()->setText(currentPlayer->character.name);
            characterName.get<TextInputComponent>()->text = currentPlayer->character.name;

            characterName.get<UiComponent>()->setVisibility(true);

            showStat();
            showSkillTree();
            showUpgradableTab();
        });

        listenToEvent<ShowSkillBookUpgradeNeed>([this](const ShowSkillBookUpgradeNeed& event) {
            LOG_INFO("Chara STree", "STree selected: " << event.sTree->name);

            showNeededItemsToLevelUp(event.sTree);
        });

        listenToStandardEvent("CharaNameChange", [this](const StandardEvent& event) {
            auto returnText = event.values.at("return").toString();

            LOG_INFO("CharaNameChange", "Name changed to: " << returnText);

            LOG_INFO("CharacterLeftClicked", "Current player id: " << currentPlayer->character.id);

            currentPlayer->character.name = returnText;

            updateCharacterList();
        });

        listenToEvent<ChangeSkillTreeInUse>([this](const ChangeSkillTreeInUse& event) {
            currentPlayer->setSkillTreeInUse(event.sTree, event.skillTreeSelected);

            showSkillTree();
            showStat();

            skillTreeUi["treeList"].get<ListView>()->setVisibility(false);
        });

        listenToEvent<LevelUpSkillTree>([this](const LevelUpSkillTree&) {
            if (not enoughItemsToLevelUp or not sTreeToUpgrade)
                return;

            for (auto& requiredItem : sTreeToUpgrade->requiredMatForNextLevel.neededMat[sTreeToUpgrade->currentLevel])
            {
                ecsRef->sendEvent(LoseItem{requiredItem});
            }

            sTreeToUpgrade->currentLevel++;

            bool treeEquiped = false;

            for (size_t i = 0; i < MAXSKILLTREEINUSE; i++)
            {
                if (currentPlayer->skillTreeInUse[i] == sTreeToUpgrade->name)
                {
                    treeEquiped = true;
                    break;
                }
            }

            if (treeEquiped)
                currentPlayer->applyLevelGain(sTreeToUpgrade->levelGains[sTreeToUpgrade->currentLevel]);

            showStat();
            showUpgradableTab();

            showNeededItemsToLevelUp(sTreeToUpgrade);
        });

        auto listView = makeListView(this, 0, 150, 300, 120);

        characterList = listView.get<ListView>();

        auto charaName = makeTTFTextInput(this, 300.0f, 0.0f, StandardEvent("CharaNameChange"), "res/font/Inter/static/Inter_28pt-Light.ttf", "Character 1", 0.7);

        characterName = charaName.entity;

        charaName.get<UiComponent>()->setVisibility(false);
        
        charaName.get<TextInputComponent>()->clearTextAfterEnter = false;

        makeStatUi();
        makeSkillTreeUi();

        makeUpgradableTab();
    }

    void PlayerCustomizationScene::startUp()
    {
        LOG_INFO("Player", "Starting up");

        for (auto player : ecsRef->view<PlayerCharacter>())
        {
            LOG_INFO("Player", "Loading players");

            if (player)
            {
                LOG_INFO("Player", "Got player: " << player->character.name);

                addPlayerToListView(player);
            }
        }
    }

    void PlayerCustomizationScene::execute()
    {
    }

    void PlayerCustomizationScene::addPlayerToListView(PlayerCharacter* player)
    {
        auto ttf = makeTTFText(this, 0, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", player->character.name, 0.4);
        auto ttfUi = ttf.get<UiComponent>();
        ttfUi->setVisibility(false);

        attach<MouseLeftClickComponent>(ttf.entity, makeCallable<SelectedCharacter>(player));

        characterList->addEntity(ttfUi);

        ttfTextIdToCharacter.emplace(ttf.entity.id, &player->character);
    }

    void PlayerCustomizationScene::updateCharacterList()
    {
        LOG_INFO("CharaNameChange", "Updating character list...");

        for (auto& character : characterList->entities)
        {
            auto entity = ecsRef->getEntity(character.entityId);

            if (entity != nullptr and entity->has<TTFText>())
            {
                auto chara = ttfTextIdToCharacter.at(character.entityId);
                entity->get<TTFText>()->setText(chara->name);
            }
        }
    }

    void PlayerCustomizationScene::makeStatUi()
    {
        std::vector<std::string> statToDisplay = {"HP", "ATK", "MATK", "DEF", "MDEF", "SPEED", "CC", "CD", "EVA"};

        float baseX = 400;
        float baseY = 75;

        for (const std::string& stat : statToDisplay)
        {
            auto ttf = makeTTFText(this, baseX, baseY, "res/font/Inter/static/Inter_28pt-Light.ttf", stat, 0.4);
            auto ttfUi = ttf.get<UiComponent>();
            ttfUi->setVisibility(false);

            auto ttfNb = makeTTFText(this, baseX + 125, baseY, "res/font/Inter/static/Inter_28pt-Light.ttf", "0", 0.4);
            auto ttfUi2 = ttfNb.get<UiComponent>();
            ttfUi2->setVisibility(false);

            characterStatUi[stat] = ttf.entity;
            characterStatUi[stat + "nb"] = ttfNb.entity;

            baseY += 35;
        }
    }

    void PlayerCustomizationScene::showStat()
    {
        characterStatUi["HPnb"].get<TTFText>()->setText(std::to_string(currentPlayer->character.stat.health));
        characterStatUi["ATKnb"].get<TTFText>()->setText(std::to_string(currentPlayer->character.stat.physicalAttack));
        characterStatUi["DEFnb"].get<TTFText>()->setText(std::to_string(currentPlayer->character.stat.physicalDefense));
        characterStatUi["MATKnb"].get<TTFText>()->setText(std::to_string(currentPlayer->character.stat.magicalAttack));
        characterStatUi["MDEFnb"].get<TTFText>()->setText(std::to_string(currentPlayer->character.stat.magicalDefense));
        characterStatUi["SPEEDnb"].get<TTFText>()->setText(std::to_string(currentPlayer->character.stat.speed));
        characterStatUi["CCnb"].get<TTFText>()->setText(std::to_string(currentPlayer->character.stat.critChance));
        characterStatUi["CDnb"].get<TTFText>()->setText(std::to_string(currentPlayer->character.stat.critDamage));
        characterStatUi["EVAnb"].get<TTFText>()->setText(std::to_string(currentPlayer->character.stat.evasionRate));

        for (auto entity : characterStatUi)
        {
            entity.second.get<UiComponent>()->setVisibility(true);
        }
    }

    struct SelectNewSkillTreeUi
    {
        SelectNewSkillTreeUi(size_t id) : id(id) {}

        size_t id;
    };

    void PlayerCustomizationScene::makeSkillTreeUi()
    {
        float baseX = 400;
        float baseY = 450;

        for (size_t i = 0; i < 3; i++)
        {
            auto ttf = makeTTFText(this, baseX, baseY, "res/font/Inter/static/Inter_28pt-Light.ttf", "None", 0.4);
            auto ttfUi = ttf.get<UiComponent>();
            ttfUi->setVisibility(false);

            attach<MouseLeftClickComponent>(ttf.entity, makeCallable<SelectNewSkillTreeUi>(i));

            skillTreeUi["skill" + std::to_string(i)] = ttf.entity;

            baseX += 90;
        }

        listenToEvent<SelectNewSkillTreeUi>([this](const SelectNewSkillTreeUi& event) {
            showSkillTreeReplacement(event.id);
        });

        auto listView = makeListView(this, 400, 500, 200, 120);
        listView.get<ListView>()->setVisibility(false);

        skillTreeUi["treeList"] = listView.entity;
    }

    void PlayerCustomizationScene::showSkillTree()
    {
        for (size_t i = 0; i < MAXSKILLTREEINUSE; i++)
        {
            auto ui = skillTreeUi["skill" + std::to_string(i)];

            std::string skillTreeName = currentPlayer->skillTreeInUse[i];

            ui.get<TTFText>()->setText(skillTreeName);

            ui.get<UiComponent>()->setVisibility(true);
        }
    }

    void PlayerCustomizationScene::showSkillTreeReplacement(size_t skillTreeSelected)
    {
        auto listView = skillTreeUi["treeList"].get<ListView>();

        listView->clear();

        std::vector<std::string> skillTreeNameInUse;
        skillTreeNameInUse.reserve(MAXSKILLTREEINUSE);

        for (size_t i = 0; i < MAXSKILLTREEINUSE; i++)
        {
            skillTreeNameInUse.push_back(currentPlayer->skillTreeInUse[i]);
        }

        auto pushInListView = [this, skillTreeSelected](const std::string& name, SkillTree* sTree, ListView* listView)
        {
            auto ttf = makeTTFText(this, 0, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", name , 0.4);
            listView->addEntity(ttf.get<UiComponent>());

            attach<MouseLeftClickComponent>(ttf.entity, makeCallable<ChangeSkillTreeInUse>(sTree, skillTreeSelected));
        };

        for (auto& sTree : currentPlayer->learnedSkillTree)
        {
            if (sTree.name == "None")
            {
                pushInListView("None", &sTree, listView);
                continue;
            }

            auto it = std::find(skillTreeNameInUse.begin(), skillTreeNameInUse.end(), sTree.name);
            
            if (it == skillTreeNameInUse.end())
            {
                pushInListView(sTree.name + " lv" + std::to_string(sTree.currentLevel), &sTree, listView);
            }
        }

        listView->setVisibility(true);
    }

    void PlayerCustomizationScene::makeUpgradableTab()
    {
        auto listView = makeListView(this, 650, 150, 300, 200);
        listView.get<ListView>()->setVisibility(false);

        upgradeTabUi["SelectSkillTreeView"] = listView.entity;

        auto listView2 = makeListView(this, 650, 400, 300, 200);
        listView2.get<ListView>()->setVisibility(false);

        upgradeTabUi["NeededMatView"] = listView2.entity;

        auto upgradeButton = makeTTFText(this, 720, 75, "res/font/Inter/static/Inter_28pt-Light.ttf", "Upgrade !", 0.6f);
        upgradeButton.get<UiComponent>()->setVisibility(false);
        attach<MouseLeftClickComponent>(upgradeButton.entity, makeCallable<LevelUpSkillTree>());
        upgradeTabUi["UpgradeButton"] = upgradeButton.entity;
    }

    void PlayerCustomizationScene::showUpgradableTab()
    {
        auto listView = upgradeTabUi["SelectSkillTreeView"].get<ListView>();

        listView->clear();

        listView->setVisibility(true);

        auto pushInListView = [this](const std::string& name, SkillTree* sTree, ListView* listView)
        {
            auto ttf = makeTTFText(this, 0, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", name , 0.4);
            listView->addEntity(ttf.get<UiComponent>());

            attach<MouseLeftClickComponent>(ttf.entity, makeCallable<ShowSkillBookUpgradeNeed>(sTree));
        };

        for (auto& sTree : currentPlayer->learnedSkillTree)
        {
            if (sTree.name == "None")
            {
                continue;
            }

            pushInListView(sTree.name + " lv" + std::to_string(sTree.currentLevel), &sTree, listView);
        }
    }

    void PlayerCustomizationScene::showNeededItemsToLevelUp(SkillTree* sTree)
    {
        sTreeToUpgrade = sTree;

        auto listView = upgradeTabUi["NeededMatView"].get<ListView>();

        listView->clear();

        listView->setVisibility(true);

        auto sys = ecsRef->getSystem<InventorySystem>();

        if (not sys)
        {
            LOG_ERROR("Inventory System", "Inventory sys not available");
            return;
        }

        constant::Vector4D color = {255.0, 255.0, 255.0, 255.0f};

        enoughItemsToLevelUp = true;

        for (auto& requiredItem : sTree->requiredMatForNextLevel.neededMat[sTree->currentLevel])
        {
            std::string str = requiredItem.name + " x" + std::to_string(requiredItem.nbItems);

            if (sys->hasEnough(requiredItem))
            {
                color = {255.0, 255.0, 255.0, 255.0f};
            }
            else
            {
                color = {255.0, 0.0, 0.0, 255.0f};

                enoughItemsToLevelUp = false;
            }

            auto ttf = makeTTFText(this, 0, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", str, 0.4, color);
            listView->addEntity(ttf.get<UiComponent>());
        }

        if (sTree->currentLevel >= sTree->maxLevel)
        {
            upgradeTabUi["UpgradeButton"].get<UiComponent>()->setVisibility(false);
        }
        else
        {
            upgradeTabUi["UpgradeButton"].get<UiComponent>()->setVisibility(true);

            if (enoughItemsToLevelUp)
            {
                upgradeTabUi["UpgradeButton"].get<TTFText>()->setColor({0.0f, 255.0f, 0.0f, 255.0f});
            }
            else
            {
                upgradeTabUi["UpgradeButton"].get<TTFText>()->setColor({255.0f, 0.0f, 0.0f, 255.0f});
            }
        }
    }

}