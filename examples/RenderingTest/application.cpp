#include "application.h"

#include "2D/texture.h"
#include "UI/prefab.h"

using namespace pg;

namespace
{
    static const char *const DOM = "App";
}

struct TestPrefab : public System<InitSys>
{
    virtual void init() override
    {
        auto prefabEnt = makeAnchoredPrefab(ecsRef, 0.0f, 0.0f);

        auto ent1 = makeUiTexture(ecsRef, 32.0f, 32.0f, "NoneIcon");
        auto ent2 = makeUiTexture(ecsRef, 16.0f, 16.0f, "NoneIcon");

        auto prefab = prefabEnt.get<Prefab>();

        prefab->setMainEntity(ent1);
        prefab->addToPrefab(ent2, "Child");

        auto anch1 = ent1.get<UiAnchor>();
        auto anch2 = ent2.get<UiAnchor>();

        anch2->setBottomAnchor(anch1->bottom);
        anch2->setRightAnchor(anch1->right);

        anch2->setZConstrain(PosConstrain{prefabEnt.entity.id, AnchorType::Z, PosOpType::Add, 1.0f});
    }
};

GameApp::GameApp(const std::string &appName) : engine(appName)
{
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        // auto entity = make2DTexture(&ecs, 32.0f, 32.0f, "NoneIcon");

        // entity.get<PositionComponent>()->x = 100.0f;
        // entity.get<PositionComponent>()->y = 100.0f;

        // LOG_INFO("App", "-- Test -- Created entity with ID: " << entity.id);

        ecs.createSystem<TestPrefab>();
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
