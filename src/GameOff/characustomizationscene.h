#pragma once

#include "ECS/system.h"

#include "Scene/scenemanager.h"

#include "UI/listview.h"

#include "character.h"
#include "skilltree.h"
#include "customskilltree.h"

namespace pg
{
    constexpr size_t MAXSKILLTREEINUSE = 3;

    struct PlayerCharacter : public Ctor
    {
        Character character;

        std::vector<SkillTree> learnedSkillTree = { NoneSkillTree{}, WarriorTree{}, MageTree{} };

        SkillTree* skillTreeInUse[MAXSKILLTREEINUSE] = {nullptr};

        bool inCombat = false;

        // Todo add equipment

        void removeSkillTreeAt(size_t index);

        void setSkillTreeInUse(SkillTree* sTree, size_t index);

        void applySkillTree(SkillTree* sTree);

        virtual void onCreation(EntityRef entity) override;
    };

    struct NewPlayerCreated { EntityRef entity; };

    struct PlayerHandlingSystem : public System<Own<PlayerCharacter>, StoragePolicy>
    {
        size_t lastGivenId = 0;
    };

    enum class CurrentCharacterTab : uint8_t
    {
        Stat = 0,
        Equipment,
        Job,
    };

    struct PlayerCustomizationScene : public Scene
    {
        virtual void init() override;
        virtual void startUp() override;
        virtual void execute() override;

        void addPlayerToListView(PlayerCharacter* player);

        void updateCharacterList();

        void makeStatUi();
        void showStat();

        void makeSkillTreeUi();
        void showSkillTree();
        void showSkillTreeReplacement(size_t skillTreeSelected);

        void makeSpellUi();
        void showSpell();

        bool newCharacterCreated = false;
        EntityRef newlyCreatedCharacter;

        CompRef<ListView> characterList;

        PlayerCharacter *currentPlayer;

        bool menuShown = false;

        CurrentCharacterTab tab = CurrentCharacterTab::Stat;

        std::unordered_map<_unique_id, Character*> ttfTextIdToCharacter;

        EntityRef characterName;

        std::unordered_map<std::string, EntityRef> characterStatUi;
        std::unordered_map<std::string, EntityRef> skillTreeUi;
        std::unordered_map<std::string, EntityRef> SpellsUi;
    };
}