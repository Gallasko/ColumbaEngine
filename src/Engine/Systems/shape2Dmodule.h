#pragma once

#include "2D/simple2dobject.h"

#include "Interpreter/pginterpreter.h"

#include "positionmodule.h"

namespace pg
{
    class CreateRectangle : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem* ecsRef)
        {
            setArity(4, 4);

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

            std::cout << "[Interpreter]: Creating rec at: (" << x.toString() << ", " << y.toString() << ")" << std::endl;

            float width = 0.0f, height = 0.0f;

            if (not w.isNumber() or not h.isNumber())
            {
                LOG_ERROR("CreateRectangle", "Cannot create a new rectangle values passed are not numbers");
            }
            else
            {
                width = w.get<float>();
                height = h.get<float>();
            }

            auto rec = makeSimple2DShape(ecsRef, Shape2D::Square, width, height, {255.0f, 0.0f, 0.0f, 0.0f});
            auto recUi = rec.get<PositionComponent>();

            if(x.isNumber() and y.isNumber() and w.isNumber() and h.isNumber())
            {
                recUi->setX(x.get<float>());
                recUi->setY(y.get<float>());
            }
            else
            {
                LOG_ERROR("CreateRectangle", "Cannot create a new rectangle values passed are not numbers");
            }

            auto list = makeList(this, {});

            auto res = addToList(this, list, uiElementFunctionsList(this, ecsRef, recUi));

            return res;
        }

        EntitySystem* ecsRef = nullptr;
    };

    struct Shape2DModule : public SysModule
    {
        Shape2DModule(EntitySystem *ecsRef)
        {
            addSystemFunction<CreateRectangle>("renderSquare", ecsRef);
        }
    };
}