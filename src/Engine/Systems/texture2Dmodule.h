#pragma once

#include "2D/texture.h"

#include "Interpreter/pginterpreter.h"

#include "positionmodule.h"
#include "uimodule.h"

namespace pg
{
    class CreateTexture : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem* ecsRef)
        {
            setArity(5, 5);

            this->ecsRef = ecsRef;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto x = args.front()->getElement();
            args.pop();

            auto y = args.front()->getElement();
            args.pop();

            auto w = args.front()->getElement();
            args.pop();

            auto h = args.front()->getElement();
            args.pop();

            auto name = args.front()->getElement();
            args.pop();

            if(not w.isNumber() or not h.isNumber() or not name.isLitteral())
            {
                LOG_ERROR("Create Texture", "Cannot create a new texture, invalid parameters");

                return makeList(this, {});
            }

            auto tex = makeUiTexture(ecsRef, w.get<float>(), h.get<float>(), name.toString());
            auto texUi = tex.get<PositionComponent>();

            if(x.isNumber() and y.isNumber())
            {
                texUi->setX(x.get<float>());
                texUi->setY(y.get<float>());
            }
            else
            {
                LOG_ERROR("Create Texture", "Cannot create a new texture, invalid parameters");
            }

            auto list = makeList(this, {});

            // Todo: fix this for position component instead of ui component 
            auto res = addToList(this, list, uiElementFunctionsList(this, ecsRef, texUi));

            return res;
        }

        EntitySystem* ecsRef = nullptr;
    };

    struct Texture2DModule : public SysModule
    {
        Texture2DModule(EntitySystem *ecsRef)
        {
            addSystemFunction<CreateTexture>("renderTexture", ecsRef);
        }
    };
}