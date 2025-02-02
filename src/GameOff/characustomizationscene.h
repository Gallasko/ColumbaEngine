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
        PlayerCharacter() { for (size_t i = 0; i < MAXSKILLTREEINUSE; i ++) { skillTreeInUse[i] = "None"; } }

        PlayerCharacter(const PlayerCharacter& other) : character(other.character), learnedSkillTree(other.learnedSkillTree), inCombat(other.inCombat)
        {
            for (size_t i = 0; i < MAXSKILLTREEINUSE; i++)
            {
                skillTreeInUse[i] = other.skillTreeInUse[i];
            }
        }

        PlayerCharacter& operator=(const PlayerCharacter& other)
        {
            character = other.character;
            learnedSkillTree = other.learnedSkillTree;
            inCombat = other.inCombat;

            for (size_t i = 0; i < MAXSKILLTREEINUSE; i++)
            {
                skillTreeInUse[i] = other.skillTreeInUse[i];
            }

            return *this;
        }

        Character character;

        std::vector<SkillTree> learnedSkillTree = { NoneSkillTree{} };

        std::string skillTreeInUse[MAXSKILLTREEINUSE] = {};

        inline static std::string getType() { return "PlayerCharacter"; }

        bool inCombat = false;

        // Todo add equipment

        void removeSkillTreeAt(size_t index);

        void setSkillTreeInUse(SkillTree* sTree, size_t index);

        void applySkillTree(SkillTree* sTree);

        void applyLevelGain(const LevelIncrease& levelGain);

        void learnSpells(const LevelIncrease& levelGain);

        void updateSpellList();

        virtual void onCreation(EntityRef entity) override;

        EntitySystem *ecsRef = nullptr;
    };

    template <>
    void serialize(Archive& archive, const PlayerCharacter& value);

    template <>
    PlayerCharacter deserialize(const UnserializedObject& serializedString);

    struct NewPlayerCreated { EntityRef entity; };

    struct PlayerHandlingSystem : public System<Own<PlayerCharacter>, StoragePolicy, SaveSys>
    {
        virtual std::string getSystemName() const override { return "PlayerHandlingSystem"; }

        virtual void save(Archive& archive) override
        {
            auto players = view<PlayerCharacter>();

            serialize(archive, "nbPlayers", players.nbComponents() - 1);

            size_t i = 0;

            for (const auto& player : players)
            {
                serialize(archive, "player" + std::to_string(i), *player);

                ++i;
            }
        }

        virtual void load(const UnserializedObject& ss) override
        {
            size_t nbPlayers = 0;

            defaultDeserialize(ss, "nbPlayers", nbPlayers);

            for (size_t i = 0; i < nbPlayers; ++i)
            {
                auto player = deserialize<PlayerCharacter>(ss["player" + std::to_string(i)]);

                player.updateSpellList();

                auto ent = ecsRef->createEntity();

                ecsRef->attach<PlayerCharacter>(ent, player);
            }
        }

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

        void makeUpgradableTab();
        void showUpgradableTab();
        void showNeededItemsToLevelUp(SkillTree* sTree);

        void makeSpellUi();
        void showSpell();

        bool newCharacterCreated = false;
        EntityRef newlyCreatedCharacter;

        CompRef<ListView> characterList;

        PlayerCharacter *currentPlayer;

        bool enoughItemsToLevelUp = false;
        SkillTree* sTreeToUpgrade = nullptr;

        bool menuShown = false;

        CurrentCharacterTab tab = CurrentCharacterTab::Stat;

        std::unordered_map<_unique_id, Character*> ttfTextIdToCharacter;

        EntityRef characterName;

        std::unordered_map<std::string, EntityRef> characterStatUi;
        std::unordered_map<std::string, EntityRef> skillTreeUi;
        std::unordered_map<std::string, EntityRef> upgradeTabUi;
        std::unordered_map<std::string, EntityRef> SpellsUi;
    };
}