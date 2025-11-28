// #pragma once

// #include "UI/uisystem.h"

// #include "Interpreter/pginterpreter.h"

// namespace pg
// {
//     class GetX : public Function
//     {
//         using Function::Function;
//     public:
//         void setUp(EntitySystem* ecsRef, CompRef<UiComponent> comp)
//         {
//             setArity(0, 0);

//             this->ecsRef = ecsRef;
//             this->comp = comp;
//         }

//         virtual ValuablePtr call(ValuableQueue&) override
//         {
//             return makeVar(comp->pos.x);
//         }

//         EntitySystem* ecsRef = nullptr;
//         CompRef<UiComponent> comp;
//     };

//     class GetY : public Function
//     {
//         using Function::Function;
//     public:
//         void setUp(EntitySystem* ecsRef, CompRef<UiComponent> comp)
//         {
//             setArity(0, 0);

//             this->ecsRef = ecsRef;
//             this->comp = comp;
//         }

//         virtual ValuablePtr call(ValuableQueue&) override
//         {
//             return makeVar(comp->pos.y);
//         }

//         EntitySystem* ecsRef = nullptr;
//         CompRef<UiComponent> comp;
//     };

//     class GetZ : public Function
//     {
//         using Function::Function;
//     public:
//         void setUp(EntitySystem* ecsRef, CompRef<UiComponent> comp)
//         {
//             setArity(0, 0);

//             this->ecsRef = ecsRef;
//             this->comp = comp;
//         }

//         virtual ValuablePtr call(ValuableQueue&) override
//         {
//             return makeVar(comp->pos.z);
//         }

//         EntitySystem* ecsRef = nullptr;
//         CompRef<UiComponent> comp;
//     };

//     class GetWidth : public Function
//     {
//         using Function::Function;
//     public:
//         void setUp(EntitySystem* ecsRef, CompRef<UiComponent> comp)
//         {
//             setArity(0, 0);

//             this->ecsRef = ecsRef;
//             this->comp = comp;
//         }

//         virtual ValuablePtr call(ValuableQueue&) override
//         {
//             return makeVar(comp->width);
//         }

//         EntitySystem* ecsRef = nullptr;
//         CompRef<UiComponent> comp;
//     };

//     class GetHeight : public Function
//     {
//         using Function::Function;
//     public:
//         void setUp(EntitySystem* ecsRef, CompRef<UiComponent> comp)
//         {
//             setArity(0, 0);

//             this->ecsRef = ecsRef;
//             this->comp = comp;
//         }

//         virtual ValuablePtr call(ValuableQueue&) override
//         {
//             return makeVar(comp->height);
//         }

//         EntitySystem* ecsRef = nullptr;
//         CompRef<UiComponent> comp;
//     };

//     class SetVisible : public Function
//     {
//         using Function::Function;
//     public:
//         void setUp(EntitySystem* ecsRef, CompRef<UiComponent> comp)
//         {
//             setArity(1, 1);

//             this->ecsRef = ecsRef;
//             this->comp = comp;
//         }

//         virtual ValuablePtr call(ValuableQueue& args) override
//         {
//             auto visibility = args.front()->getElement();
//             args.pop();

//             comp->setVisibility(visibility.get<bool>());

//             return nullptr;
//         }

//         EntitySystem* ecsRef = nullptr;
//         CompRef<UiComponent> comp;
//     };

//     class SetX : public Function
//     {
//         using Function::Function;
//     public:
//         void setUp(EntitySystem* ecsRef, CompRef<UiComponent> comp)
//         {
//             setArity(1, 1);

//             this->ecsRef = ecsRef;
//             this->comp = comp;
//         }

//         virtual ValuablePtr call(ValuableQueue& args) override
//         {
//             auto x = args.front()->getElement();
//             args.pop();

//             comp->setX(x.get<float>());

//             return nullptr;
//         }

//         EntitySystem* ecsRef = nullptr;
//         CompRef<UiComponent> comp;
//     };

//     class SetY : public Function
//     {
//         using Function::Function;
//     public:
//         void setUp(EntitySystem* ecsRef, CompRef<UiComponent> comp)
//         {
//             setArity(1, 1);

//             this->ecsRef = ecsRef;
//             this->comp = comp;
//         }

//         virtual ValuablePtr call(ValuableQueue& args) override
//         {
//             auto y = args.front()->getElement();
//             args.pop();

//             comp->setY(y.get<float>());

//             return nullptr;
//         }

//         EntitySystem* ecsRef = nullptr;
//         CompRef<UiComponent> comp;
//     };

//     class SetZ : public Function
//     {
//         using Function::Function;
//     public:
//         void setUp(EntitySystem* ecsRef, CompRef<UiComponent> comp)
//         {
//             setArity(1, 1);

//             this->ecsRef = ecsRef;
//             this->comp = comp;
//         }

//         virtual ValuablePtr call(ValuableQueue& args) override
//         {
//             auto z = args.front()->getElement();
//             args.pop();

//             comp->setZ(z.get<float>());

//             return nullptr;
//         }

//         EntitySystem* ecsRef = nullptr;
//         CompRef<UiComponent> comp;
//     };

//     class SetW : public Function
//     {
//         using Function::Function;
//     public:
//         void setUp(EntitySystem* ecsRef, CompRef<UiComponent> comp)
//         {
//             setArity(1, 1);

//             this->ecsRef = ecsRef;
//             this->comp = comp;
//         }

//         virtual ValuablePtr call(ValuableQueue& args) override
//         {
//             auto w = args.front()->getElement();
//             args.pop();

//             comp->setWidth(w.get<float>());

//             return nullptr;
//         }

//         EntitySystem* ecsRef = nullptr;
//         CompRef<UiComponent> comp;
//     };

//     class SetH : public Function
//     {
//         using Function::Function;
//     public:
//         void setUp(EntitySystem* ecsRef, CompRef<UiComponent> comp)
//         {
//             setArity(1, 1);

//             this->ecsRef = ecsRef;
//             this->comp = comp;
//         }

//         virtual ValuablePtr call(ValuableQueue& args) override
//         {
//             auto h = args.front()->getElement();
//             args.pop();

//             comp->setHeight(h.get<float>());

//             return nullptr;
//         }

//         EntitySystem* ecsRef = nullptr;
//         CompRef<UiComponent> comp;
//     };

//     std::vector<SysListElement> uiElementFunctionsList(const Function *caller, EntitySystem *ecsRef, CompRef<UiComponent> comp);

//     struct UiModule : public SysModule
//     {
//         UiModule(EntitySystem *)
//         {
//         }
//     };

// }