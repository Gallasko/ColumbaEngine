#pragma once

#include "2D/position.h"

#include "Interpreter/pginterpreter.h"

namespace pg
{
    class PosGetX : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem* ecsRef, CompRef<PositionComponent> comp)
        {
            setArity(0, 0);

            this->ecsRef = ecsRef;
            this->comp = comp;
        }

        virtual ValuablePtr call(ValuableQueue&) override
        {
            return makeVar(comp->x);
        }

        EntitySystem* ecsRef = nullptr;
        CompRef<PositionComponent> comp;
    };

    class PosGetY : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem* ecsRef, CompRef<PositionComponent> comp)
        {
            setArity(0, 0);

            this->ecsRef = ecsRef;
            this->comp = comp;
        }

        virtual ValuablePtr call(ValuableQueue&) override
        {
            return makeVar(comp->y);
        }

        EntitySystem* ecsRef = nullptr;
        CompRef<PositionComponent> comp;
    };

    class PosGetZ : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem* ecsRef, CompRef<PositionComponent> comp)
        {
            setArity(0, 0);

            this->ecsRef = ecsRef;
            this->comp = comp;
        }

        virtual ValuablePtr call(ValuableQueue&) override
        {
            return makeVar(comp->z);
        }

        EntitySystem* ecsRef = nullptr;
        CompRef<PositionComponent> comp;
    };

    class PosGetWidth : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem* ecsRef, CompRef<PositionComponent> comp)
        {
            setArity(0, 0);

            this->ecsRef = ecsRef;
            this->comp = comp;
        }

        virtual ValuablePtr call(ValuableQueue&) override
        {
            return makeVar(comp->width);
        }

        EntitySystem* ecsRef = nullptr;
        CompRef<PositionComponent> comp;
    };

    class PosGetHeight : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem* ecsRef, CompRef<PositionComponent> comp)
        {
            setArity(0, 0);

            this->ecsRef = ecsRef;
            this->comp = comp;
        }

        virtual ValuablePtr call(ValuableQueue&) override
        {
            return makeVar(comp->height);
        }

        EntitySystem* ecsRef = nullptr;
        CompRef<PositionComponent> comp;
    };

    class PosSetVisible : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem* ecsRef, CompRef<PositionComponent> comp)
        {
            setArity(1, 1);

            this->ecsRef = ecsRef;
            this->comp = comp;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto visibility = args.front()->getElement();
            args.pop();

            comp->setVisibility(visibility.get<bool>());

            return nullptr;
        }

        EntitySystem* ecsRef = nullptr;
        CompRef<PositionComponent> comp;
    };

    class PosSetX : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem* ecsRef, CompRef<PositionComponent> comp)
        {
            setArity(1, 1);

            this->ecsRef = ecsRef;
            this->comp = comp;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto x = args.front()->getElement();
            args.pop();

            comp->setX(x.get<float>());

            return nullptr;
        }

        EntitySystem* ecsRef = nullptr;
        CompRef<PositionComponent> comp;
    };

    class PosSetY : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem* ecsRef, CompRef<PositionComponent> comp)
        {
            setArity(1, 1);

            this->ecsRef = ecsRef;
            this->comp = comp;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto y = args.front()->getElement();
            args.pop();

            comp->setY(y.get<float>());

            return nullptr;
        }

        EntitySystem* ecsRef = nullptr;
        CompRef<PositionComponent> comp;
    };

    class PosSetZ : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem* ecsRef, CompRef<PositionComponent> comp)
        {
            setArity(1, 1);

            this->ecsRef = ecsRef;
            this->comp = comp;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto z = args.front()->getElement();
            args.pop();

            comp->setZ(z.get<float>());

            return nullptr;
        }

        EntitySystem* ecsRef = nullptr;
        CompRef<PositionComponent> comp;
    };

    class PosSetW : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem* ecsRef, CompRef<PositionComponent> comp)
        {
            setArity(1, 1);

            this->ecsRef = ecsRef;
            this->comp = comp;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto w = args.front()->getElement();
            args.pop();

            comp->setWidth(w.get<float>());

            return nullptr;
        }

        EntitySystem* ecsRef = nullptr;
        CompRef<PositionComponent> comp;
    };

    class PosSetH : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem* ecsRef, CompRef<PositionComponent> comp)
        {
            setArity(1, 1);

            this->ecsRef = ecsRef;
            this->comp = comp;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto h = args.front()->getElement();
            args.pop();

            comp->setHeight(h.get<float>());

            return nullptr;
        }

        EntitySystem* ecsRef = nullptr;
        CompRef<PositionComponent> comp;
    };

    std::vector<SysListElement> uiElementFunctionsList(const Function *caller, EntitySystem *ecsRef, CompRef<PositionComponent> comp);
}