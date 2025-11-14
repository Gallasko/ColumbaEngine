// #pragma once

// #include "UI/sentencesystem.h"

// #include "Interpreter/pginterpreter.h"

// #include "uimodule.h"

// namespace pg
// {
//     class SetText : public Function
//     {
//         using Function::Function;
//     public:
//         void setUp(EntitySystem* ecsRef, CompRef<SentenceText> comp)
//         {
//             setArity(1, 1);

//             this->ecsRef = ecsRef;
//             this->comp = comp;
//         }

//         virtual ValuablePtr call(ValuableQueue& args) override
//         {
//             auto text = args.front()->getElement();
//             args.pop();

//             comp->setText(text.toString());

//             return nullptr;
//         }

//         EntitySystem* ecsRef = nullptr;
//         CompRef<SentenceText> comp;
//     };

//     class CreateUiText : public Function
//     {
//         using Function::Function;
//     public:
//         void setUp(EntitySystem* ecsRef)
//         {
//             setArity(3, 3);

//             this->ecsRef = ecsRef;
//         }

//         virtual ValuablePtr call(ValuableQueue& args) override
//         {
//             auto x = args.front()->getElement();
//             args.pop();

//             auto y = args.front()->getElement();
//             args.pop();

//             auto sentenceText = args.front()->getElement();
//             args.pop();

//             if (not x.isNumber() or not y.isNumber() or not sentenceText.isLitteral())
//             {
//                 LOG_ERROR("CreateUiText", "Cannot create a new ui text, values passed are not of the correct type." <<
//                           "\nExpected (number, number, litteral) instead got:  " <<
//                           x.getTypeString() << ", " <<
//                           x.getTypeString() << ", " <<
//                           sentenceText.getTypeString() << "!");
//                 return nullptr;
//             }

//             auto text = makeSentence(ecsRef, x.get<float>(), y.get<float>(), {sentenceText.get<std::string>()});

//             auto textUi = text.get<UiComponent>();

//             auto list = makeList(this, {{"setText", makeFun<SetText>(this, "setText", ecsRef, text.get<SentenceText>())}} );

//             auto res = addToList(this, list, uiElementFunctionsList(this, ecsRef, textUi));

//             return res;
//         }

//         EntitySystem* ecsRef = nullptr;
//     };

//     struct SentenceModule : public SysModule
//     {
//         SentenceModule(EntitySystem *ecsRef)
//         {
//             LOG_THIS_MEMBER("Sentence Module");

//             addSystemFunction<CreateUiText>("newUiText", ecsRef);
//         }
//     };

// }