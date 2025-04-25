#include "gtest/gtest.h"

#include "Renderer/renderer.h"

namespace pg
{
    namespace test
    {
        struct MockRenderer : public AbstractRenderer
        {
            MockRenderer(MasterRenderer* masterRenderer, const RenderStage& stage) : AbstractRenderer(masterRenderer, stage) {}

            void addRenderCall(const RenderCall& call)
            {
                renderCallList.push_back(call);
            }
        };

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(render_call_test, initialization)
        {
            RenderCall call;

            EXPECT_EQ(call.key,          0);
            EXPECT_EQ(call.data.size(),  0);
            EXPECT_EQ(call.batchable,    true);
        }

        TEST(render_call_test, setting_key_values)
        {
            RenderCall call;

            EXPECT_EQ(call.key,          0);
            EXPECT_EQ(call.data.size(),  0);
            EXPECT_EQ(call.batchable,    true);

            call.setDepth(-5);

            EXPECT_EQ(call.key, ((uint64_t)(0b000000000000111111111111) - 5) << 30);

            auto depth = call.getDepth();

            EXPECT_EQ(depth, -5);

            bool visible = call.getVisibility();

            EXPECT_TRUE(visible);

            call.setVisibility(false);

            EXPECT_EQ(call.key, (((uint64_t)(0b000000000000111111111111) - 5) << 30) + ((uint64_t)0b1 << 63));

            visible = call.getVisibility();

            EXPECT_FALSE(visible);

            call.setVisibility(true);

            EXPECT_EQ(call.key, (((uint64_t)(0b000000000000111111111111) - 5) << 30));

            visible = call.getVisibility();

            EXPECT_TRUE(visible);
        }

        TEST(render_call_test, set_and_get_render_stage_and_opacity)
        {
            RenderCall call;
            call.setRenderStage(RenderStage::Render);
            EXPECT_EQ(call.getRenderStage(), RenderStage::Render);

            call.setOpacity(OpacityType::Additive);
            EXPECT_EQ(call.getOpacity(), OpacityType::Additive);
        }

        TEST(render_call_test, material_id_encoding)
        {
            uint64_t materialId = 0x123456;
            RenderCall call(true, RenderStage::Render, OpacityType::Opaque, 0, materialId);
            EXPECT_EQ(call.getMaterialId(), materialId);

            // ensure key lower bits equal materialId
            EXPECT_EQ((call.key & 0x3FFFFFFF), materialId);
        }

        TEST(render_call_test, depth_encoding_and_sort)
        {
            RenderCall nearCall(true, RenderStage::Render, OpacityType::Opaque, -10, 1);
            RenderCall farCall (true, RenderStage::Render, OpacityType::Opaque, +5, 1);

            EXPECT_LT(nearCall.getDepth(), farCall.getDepth());

            std::vector<RenderCall> v{farCall, nearCall};
            std::sort(v.begin(), v.end());
            // sorted by key which includes depth bits, so near (smaller depth) first
            EXPECT_EQ(v[0].getDepth(), -10);
            EXPECT_EQ(v[1].getDepth(), +5);
        }

        TEST(renderer_test, basic_render)
        {
            MasterRenderer masterRenderer;
            MockRenderer renderer(&masterRenderer, RenderStage::Render);

            RenderCall call1, call2;

            call1.data.push_back(1);
            call1.data.push_back(2);

            call2.data.push_back(3);
            call2.data.push_back(4);

            renderer.addRenderCall(call1);
            renderer.addRenderCall(call2);

            masterRenderer.execute();

            EXPECT_EQ(masterRenderer.getRenderCalls(0).size(), 0);
            // Expected 1 and not 2 because the second Call was correctly batched
            EXPECT_EQ(masterRenderer.getRenderCalls(1).size(), 1);
            EXPECT_EQ(masterRenderer.getRenderCalls().size(), 0);

            EXPECT_EQ(masterRenderer.getRenderCalls(1)[0].data.size(), 4);
            EXPECT_EQ(masterRenderer.getRenderCalls(1)[0].data[0], 1);
            EXPECT_EQ(masterRenderer.getRenderCalls(1)[0].data[1], 2);
            EXPECT_EQ(masterRenderer.getRenderCalls(1)[0].data[2], 3);
            EXPECT_EQ(masterRenderer.getRenderCalls(1)[0].data[3], 4);

        }

        TEST(renderer_test, basic_render_not_batchable)
        {
            MasterRenderer masterRenderer;
            MockRenderer renderer(&masterRenderer, RenderStage::Render);

            RenderCall call1, call2;

            call1.batchable = false;
            call1.data.push_back(1);
            call1.data.push_back(2);

            call2.data.push_back(3);
            call2.data.push_back(4);

            renderer.addRenderCall(call1);
            renderer.addRenderCall(call2);

            masterRenderer.execute();

            EXPECT_EQ(masterRenderer.getRenderCalls(0).size(), 0);
            // Expected 1 and not 2 because the second Call was correctly batched
            EXPECT_EQ(masterRenderer.getRenderCalls(1).size(), 2);
            EXPECT_EQ(masterRenderer.getRenderCalls().size(), 0);

            EXPECT_EQ(masterRenderer.getRenderCalls(1)[0].data.size(), 2);
            EXPECT_EQ(masterRenderer.getRenderCalls(1)[0].data[0], 1);
            EXPECT_EQ(masterRenderer.getRenderCalls(1)[0].data[1], 2);
            EXPECT_EQ(masterRenderer.getRenderCalls(1)[1].data.size(), 2);
            EXPECT_EQ(masterRenderer.getRenderCalls(1)[1].data[0], 3);
            EXPECT_EQ(masterRenderer.getRenderCalls(1)[1].data[1], 4);
        }

        TEST(renderer_test, render_different_state)
        {
            MasterRenderer masterRenderer;
            MockRenderer renderer(&masterRenderer, RenderStage::Render);

            RenderCall call1, call2;
            call1.data.push_back(10.0f);
            call1.state.scissorEnabled = false;
            call2.data.push_back(20.0f);
            call2.state.scissorEnabled = true;

            renderer.addRenderCall(call1);
            renderer.addRenderCall(call2);
            masterRenderer.execute();

            const auto& calls = masterRenderer.getRenderCalls(1);
            ASSERT_EQ(calls.size(), 2u);
            EXPECT_FALSE(calls[0].state.scissorEnabled);
            EXPECT_TRUE(calls[1].state.scissorEnabled);
        }

        TEST(renderer_test, render_multiple_keys_sorted)
        {
            MasterRenderer masterRenderer;
            MockRenderer renderer(&masterRenderer, RenderStage::Render);

            RenderCall call1(true, RenderStage::Render, OpacityType::Opaque, 0, 2);
            RenderCall call2(true, RenderStage::Render, OpacityType::Opaque, 0, 1);
            call1.data.push_back(200.0f);
            call2.data.push_back(100.0f);

            renderer.addRenderCall(call1);
            renderer.addRenderCall(call2);
            masterRenderer.execute();

            const auto& calls = masterRenderer.getRenderCalls(1);
            ASSERT_EQ(calls.size(), 2u);
            EXPECT_EQ(calls[0].getMaterialId(), 1ull);
            EXPECT_EQ(calls[1].getMaterialId(), 2ull);
        }

        TEST(renderer_test, render_visibility_order)
        {
            MasterRenderer masterRenderer;
            MockRenderer renderer(&masterRenderer, RenderStage::Render);

            RenderCall hidden(true, RenderStage::Render, OpacityType::Opaque, 0, 1);
            RenderCall visible(true, RenderStage::Render, OpacityType::Opaque, 0, 2);
            hidden.setVisibility(false);

            renderer.addRenderCall(hidden);
            renderer.addRenderCall(visible);
            masterRenderer.execute();

            const auto& calls = masterRenderer.getRenderCalls(1);
            ASSERT_EQ(calls.size(), 2);
            EXPECT_TRUE(calls[0].getVisibility());
            EXPECT_FALSE(calls[1].getVisibility());
        }

        TEST(renderer_test, render_scissor_grouping)
        {
            MasterRenderer masterRenderer;
            MockRenderer renderer(&masterRenderer, RenderStage::Render);

            RenderCall a(true, RenderStage::Render, OpacityType::Opaque, 0, 1);
            RenderCall b(true, RenderStage::Render, OpacityType::Opaque, 0, 1);
            a.state.scissorEnabled = true;
            b.state.scissorBound = {10,10,100,100};

            a.data.push_back(5.0f);
            b.data.push_back(6.0f);

            renderer.addRenderCall(a);
            renderer.addRenderCall(b);
            masterRenderer.execute();

            const auto& calls = masterRenderer.getRenderCalls(1);
            ASSERT_EQ(calls.size(), 2);
            // same key but different state => no merge
            EXPECT_TRUE(calls[0].state.scissorEnabled);
            EXPECT_EQ(calls[1].state.scissorBound.x, 10);
        }

        TEST(renderer_test, complex_batching_sequence)
        {
            MasterRenderer masterRenderer;
            MockRenderer renderer(&masterRenderer, RenderStage::Render);

            // call1 and call2 merge
            RenderCall call1, call2;
            call1.data = {1.0f};
            call2.data = {2.0f};

            // call3 non-batchable
            RenderCall call3;
            call3.batchable = false;
            call3.data = {3.0f};

            // call4 and call5 merge (same key, state)
            RenderCall call4, call5;
            call4.data = {4.0f};
            call5.data = {5.0f};

            renderer.addRenderCall(call1);
            renderer.addRenderCall(call2);
            renderer.addRenderCall(call3);
            renderer.addRenderCall(call4);
            renderer.addRenderCall(call5);
            masterRenderer.execute();

            const auto& calls = masterRenderer.getRenderCalls(1);
            // expect: [merged(1+2+4+5), call3] => 2 total
            ASSERT_EQ(calls.size(), 2);
            EXPECT_EQ(calls[0].data, (std::vector<float>{1, 2, 4, 5}));
            EXPECT_EQ(calls[1].data, (std::vector<float>{3}));
        }

        TEST(renderer_test, render_state_sorting)
        {
            MasterRenderer masterRenderer;
            MockRenderer renderer(&masterRenderer, RenderStage::Render);

            // Two calls same key, different state
            RenderCall callA(true, RenderStage::Render, OpacityType::Opaque, 0, 1);
            RenderCall callB(true, RenderStage::Render, OpacityType::Opaque, 0, 1);
            callA.state.scissorBound = {0, 0, 10, 10};
            callB.state.scissorBound = {5, 5, 15, 15};

            renderer.addRenderCall(callA);
            renderer.addRenderCall(callB);
            masterRenderer.execute();

            const auto& calls = masterRenderer.getRenderCalls(1);
            ASSERT_EQ(calls.size(), 2);
            // grouping by state ordering: lexicographically scissorBound
            EXPECT_LT(calls[0].state.scissorBound.x, calls[1].state.scissorBound.x);
        }

        TEST(renderer_test, chained_batching_three_calls)
        {
            MasterRenderer masterRenderer;
            MockRenderer renderer(&masterRenderer, RenderStage::Render);

            RenderCall call1, call2, call3;
            call1.data = {1.0f};
            call2.data = {2.0f};
            call3.data = {3.0f};

            renderer.addRenderCall(call1);
            renderer.addRenderCall(call2);
            renderer.addRenderCall(call3);
            masterRenderer.execute();

            const auto& calls = masterRenderer.getRenderCalls(1);
            ASSERT_EQ(calls.size(), 1u);
            EXPECT_EQ(calls[0].data, (std::vector<float>{1,2,3}));
        }

        TEST(renderer_test, zero_data_calls)
        {
            MasterRenderer masterRenderer;
            MockRenderer renderer(&masterRenderer, RenderStage::Render);

            RenderCall call1, call2;
            // call1 has no data
            call2.data = {4.0f};

            renderer.addRenderCall(call1);
            renderer.addRenderCall(call2);
            masterRenderer.execute();

            const auto& calls = masterRenderer.getRenderCalls(1);
            ASSERT_EQ(calls.size(), 1u);
            EXPECT_EQ(calls[0].data.size(), 1u);
            EXPECT_FLOAT_EQ(calls[0].data[0], 4.0f);
        }
    } // namespace test

} // namespace pg