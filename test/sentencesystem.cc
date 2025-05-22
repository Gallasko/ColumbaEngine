#include "stdafx.h"

#include <gtest/gtest.h>

#include "mocksentencesystem.h"

#include "mocklogger.h"

namespace pg
{
    namespace test
    {
        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(sentence_system, initialization)
        {
            MasterRenderer renderer;

            MockSentenceSystem sys(&renderer);

            EXPECT_EQ(sys.getCurrentSize(),     1);
            EXPECT_EQ(sys.getElementIndex(),    0);
            EXPECT_EQ(sys.getVisibleElements(), 0);
            EXPECT_EQ(sys.getNbAttributes(),    22);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(sentence_system, creation)
        {
            MockLogger logger;
            EntitySystem ecs;

            MasterRenderer renderer;

            ecs.createSystem<UiComponentSystem>();

            auto sys = ecs.createMockSystem<SentenceSystem, MockSentenceSystem>(&renderer);

            EXPECT_EQ(sys->getCurrentSize(),     1);
            EXPECT_EQ(sys->getElementIndex(),    0);
            EXPECT_EQ(sys->getVisibleElements(), 0);

            auto ent = ecs.createEntity();

            EXPECT_EQ(sys->getCurrentSize(),     1);
            EXPECT_EQ(sys->getElementIndex(),    0);
            EXPECT_EQ(sys->getVisibleElements(), 0);

            auto ui = ecs.attach<UiComponent>(ent);

            ui->setX(15);
            ui->setY(16);

            auto text = ecs.attach<SentenceText>(ent, SentenceText{"Hello"});

            ui->setWidth(&text->textWidth);
            ui->setHeight(&text->textHeight);

            EXPECT_EQ(sys->getCurrentSize(),     8);
            EXPECT_EQ(sys->getElementIndex(),    5);
            EXPECT_EQ(sys->getVisibleElements(), 5);

            EXPECT_EQ(ui->width, 79);
            EXPECT_EQ(ui->height, 18);

            if (sys->getElementIndex() >= 5)
            {
                auto buffer = sys->getBuffer();

                EXPECT_EQ(buffer[0],  15.0f);
                EXPECT_EQ(buffer[1],  16.0f);
                EXPECT_EQ(buffer[2],  0.0f);

                EXPECT_EQ(buffer[7],  18.0f);
                EXPECT_EQ(buffer[8],  18.0f);

                EXPECT_EQ(buffer[9],  255.0f);
                EXPECT_EQ(buffer[10], 255.0f);
                EXPECT_EQ(buffer[11], 255.0f);
            }
            else
            {
                EXPECT_TRUE(false);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(sentence_system, multiple_sentence)
        {
            MockLogger logger;
            EntitySystem ecs;

            MasterRenderer renderer;

            ecs.createSystem<UiComponentSystem>();

            auto sys = ecs.createMockSystem<SentenceSystem, MockSentenceSystem>(&renderer);

            EXPECT_EQ(sys->getCurrentSize(),     1);
            EXPECT_EQ(sys->getElementIndex(),    0);
            EXPECT_EQ(sys->getVisibleElements(), 0);

            auto ent = ecs.createEntity();

            EXPECT_EQ(sys->getCurrentSize(),     1);
            EXPECT_EQ(sys->getElementIndex(),    0);
            EXPECT_EQ(sys->getVisibleElements(), 0);

            auto ui = ecs.attach<UiComponent>(ent);

            ui->setX(15);
            ui->setY(16);

            auto text = ecs.attach<SentenceText>(ent, SentenceText{"Hello"});

            ui->setWidth(&text->textWidth);
            ui->setHeight(&text->textHeight);

            EXPECT_EQ(sys->getCurrentSize(),     8);
            EXPECT_EQ(sys->getElementIndex(),    5);
            EXPECT_EQ(sys->getVisibleElements(), 5);

            EXPECT_EQ(ui->width, 79);
            EXPECT_EQ(ui->height, 18);

            if (sys->getElementIndex() >= 5)
            {
                auto buffer = sys->getBuffer();

                EXPECT_EQ(buffer[0],  15.0f);
                EXPECT_EQ(buffer[1],  16.0f);
                EXPECT_EQ(buffer[2],  0.0f);

                EXPECT_EQ(buffer[7],  18.0f);
                EXPECT_EQ(buffer[8],  18.0f);

                EXPECT_EQ(buffer[9],  255.0f);
                EXPECT_EQ(buffer[10], 255.0f);
                EXPECT_EQ(buffer[11], 255.0f);
            }
            else
            {
                EXPECT_TRUE(false);
            }

            makeSentence(&ecs, 10, 20, {"world !"});

            EXPECT_EQ(sys->getCurrentSize(),     16);
            EXPECT_EQ(sys->getElementIndex(),    12);
            EXPECT_EQ(sys->getVisibleElements(), 12);

            makeSentence(&ecs, 100, 200, {"New text ..."});

            EXPECT_EQ(sys->getCurrentSize(),     32);
            EXPECT_EQ(sys->getElementIndex(),    24);
            EXPECT_EQ(sys->getVisibleElements(), 24);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(sentence_system, hidden_sentence)
        {
            MockLogger logger;
            EntitySystem ecs;

            MasterRenderer renderer;

            ecs.createSystem<UiComponentSystem>();

            auto sys = ecs.createMockSystem<SentenceSystem, MockSentenceSystem>(&renderer);

            auto ent = makeSentence(&ecs, 100, 200, {"New text ..."});

            EXPECT_EQ(sys->getCurrentSize(),     16);
            EXPECT_EQ(sys->getElementIndex(),    12);
            EXPECT_EQ(sys->getVisibleElements(), 12);

            ent.get<UiComponent>()->setVisibility(false);

            EXPECT_EQ(sys->getCurrentSize(),     16);
            EXPECT_EQ(sys->getElementIndex(),    12);
            EXPECT_EQ(sys->getVisibleElements(), 0);

            makeSentence(&ecs, 20, 50, {"HA"});

            EXPECT_EQ(sys->getCurrentSize(),     16);
            EXPECT_EQ(sys->getElementIndex(),    14);
            EXPECT_EQ(sys->getVisibleElements(), 2);

            if (sys->getElementIndex() >= 16)
            {
                auto buffer = sys->getBuffer();

                EXPECT_EQ(buffer[0],  20.0f);
                EXPECT_EQ(buffer[1],  50.0f);
            }

        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(sentence_system, set_text)
        {
            MockLogger logger;
            EntitySystem ecs;

            MasterRenderer renderer;

            ecs.createSystem<UiComponentSystem>();

            auto sys = ecs.createMockSystem<SentenceSystem, MockSentenceSystem>(&renderer);

            auto ent = makeSentence(&ecs, 100, 200, {"Old text ..."});

            EXPECT_EQ(sys->getCurrentSize(),     16);
            EXPECT_EQ(sys->getElementIndex(),    12);
            EXPECT_EQ(sys->getVisibleElements(), 12);

            ent.get<SentenceText>()->setText("New text !");

            EXPECT_EQ(sys->getCurrentSize(),     16);
            EXPECT_EQ(sys->getElementIndex(),    10);
            EXPECT_EQ(sys->getVisibleElements(), 10);

            EXPECT_EQ(logger.getNbError(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(sentence_system, set_multiple_text)
        {
            MockLogger logger;
            EntitySystem ecs;

            MasterRenderer renderer;

            ecs.createSystem<UiComponentSystem>();

            auto sys = ecs.createMockSystem<SentenceSystem, MockSentenceSystem>(&renderer);

            auto ent = makeSentence(&ecs, 100, 200, {"Old text ..."});

            EXPECT_EQ(sys->getCurrentSize(),     16);
            EXPECT_EQ(sys->getElementIndex(),    12);
            EXPECT_EQ(sys->getVisibleElements(), 12);

            auto ent2 = makeSentence(&ecs, 150, 250, {"Text 2"});

            EXPECT_EQ(sys->getCurrentSize(),     32);
            EXPECT_EQ(sys->getElementIndex(),    18);
            EXPECT_EQ(sys->getVisibleElements(), 18);

            ent.get<SentenceText>()->setText("New text !");

            EXPECT_EQ(sys->getCurrentSize(),     32);
            EXPECT_EQ(sys->getElementIndex(),    16);
            EXPECT_EQ(sys->getVisibleElements(), 16);

            ent2.get<SentenceText>()->setText("New text 2 !");

            EXPECT_EQ(sys->getCurrentSize(),     32);
            EXPECT_EQ(sys->getElementIndex(),    22);
            EXPECT_EQ(sys->getVisibleElements(), 22);

            EXPECT_EQ(logger.getNbError(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(sentence_system, set_multiple_text_with_invisible)
        {
            MockLogger logger;
            EntitySystem ecs;

            MasterRenderer renderer;

            ecs.createSystem<UiComponentSystem>();

            auto sys = ecs.createMockSystem<SentenceSystem, MockSentenceSystem>(&renderer);

            auto ent = makeSentence(&ecs, 100, 200, {"Old text ..."});

            EXPECT_EQ(sys->getCurrentSize(),     16);
            EXPECT_EQ(sys->getElementIndex(),    12);
            EXPECT_EQ(sys->getVisibleElements(), 12);

            auto ent2 = makeSentence(&ecs, 150, 250, {"Text 2"});

            EXPECT_EQ(sys->getCurrentSize(),     32);
            EXPECT_EQ(sys->getElementIndex(),    18);
            EXPECT_EQ(sys->getVisibleElements(), 18);

            auto ent3 = makeSentence(&ecs, 150, 250, {"New text 3 !"});

            EXPECT_EQ(sys->getCurrentSize(),     32);
            EXPECT_EQ(sys->getElementIndex(),    30);
            EXPECT_EQ(sys->getVisibleElements(), 30);

            ent.get<SentenceText>()->setText("New text !");

            EXPECT_EQ(sys->getCurrentSize(),     32);
            EXPECT_EQ(sys->getElementIndex(),    28);
            EXPECT_EQ(sys->getVisibleElements(), 28);

            ent.get<UiComponent>()->setVisibility(false);

            EXPECT_EQ(sys->getCurrentSize(),     32);
            EXPECT_EQ(sys->getElementIndex(),    28);
            EXPECT_EQ(sys->getVisibleElements(), 18);

            ent3.get<SentenceText>()->setText("Move text 3");

            EXPECT_EQ(sys->getCurrentSize(),     32);
            EXPECT_EQ(sys->getElementIndex(),    27);
            EXPECT_EQ(sys->getVisibleElements(), 17);

            ent.get<UiComponent>()->setVisibility(true);

            EXPECT_EQ(sys->getCurrentSize(),     32);
            EXPECT_EQ(sys->getElementIndex(),    27);
            EXPECT_EQ(sys->getVisibleElements(), 27);

            ent2.get<SentenceText>()->setText("New text 2...");

            EXPECT_EQ(sys->getCurrentSize(),     64);
            EXPECT_EQ(sys->getElementIndex(),    34);
            EXPECT_EQ(sys->getVisibleElements(), 34);

            EXPECT_EQ(logger.getNbError(), 0);
        }
    }
}
