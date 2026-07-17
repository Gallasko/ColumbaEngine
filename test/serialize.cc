#include "stdafx.h"

#include <filesystem>
#include <iostream>
namespace fs = std::filesystem;

#include "gtest/gtest.h"

#include "serialization.h"

#include "mocklogger.h"

namespace pg
{
    struct TestSerializeA
    {
        TestSerializeA(int data) : data(data) {}

        int data = 0;
    };

    template <>
    void serialize(Archive& archive, const TestSerializeA& a)
    {
        archive.startSerialization("Test Serial A");

        serialize(archive, "data", a.data);

        archive.endSerialization();
    }


    namespace test
    {
        namespace
        {
            // Per-test file name: ctest runs each test in its own process,
            // possibly in parallel, from the same working directory — a shared
            // file name would let tests overwrite each other's data mid-run.
            std::string tempSerializePath()
            {
                const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();

                return std::string("tmpSerializeTest_") + info->name() + ".sz";
            }
        }

        // Todo mock serializer or add a serialize to text method !

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(serialize_test, initialization)
        {
            Serializer serialize;

            auto map = serialize.getSerializedMap();

            EXPECT_EQ(map.size(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(serialize_test, serialize_int)
        {
            MockLogger logger;
            fs::remove(tempSerializePath());

            Serializer serialize;

            serialize.setFile(tempSerializePath());

            int val = 5;

            serialize.serializeObject("test", val);

            auto map = serialize.getSerializedMap();

            EXPECT_EQ(map.size(), 1);

            auto file = UniversalFileAccessor::openTextFile(tempSerializePath());

            EXPECT_EQ(file.data, serialize.getVersion() + "\ntest: __PGSA int {5}");

            EXPECT_EQ(logger.getNbError(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(serialize_test, deserialize_int)
        {
            MockLogger logger;
            fs::remove(tempSerializePath());

            Serializer serialize;

            serialize.setFile(tempSerializePath());

            int val = 5;

            serialize.serializeObject("test", val);

            auto map = serialize.getSerializedMap();

            EXPECT_EQ(map.size(), 1);

            auto file = UniversalFileAccessor::openTextFile(tempSerializePath());

            EXPECT_EQ(file.data, serialize.getVersion() + "\ntest: __PGSA int {5}");

            int ret = serialize.deserializeObject<int>("test");

            EXPECT_EQ(ret, 5);

            EXPECT_EQ(logger.getNbError(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(serialize_test, serialize_deserialize)
        {
            MockLogger logger;
            fs::remove(tempSerializePath());

            Serializer serialize;

            serialize.setFile(tempSerializePath());

            int val = 5;

            serialize.serializeObject("test", val);

            auto map = serialize.getSerializedMap();

            EXPECT_EQ(map.size(), 1);

            auto file = UniversalFileAccessor::openTextFile(tempSerializePath());

            EXPECT_EQ(file.data, serialize.getVersion() + "\ntest: __PGSA int {5}");

            int ret = serialize.deserializeObject<int>("test");

            EXPECT_EQ(ret, 5);

            serialize.setFile(tempSerializePath());

            ret = serialize.deserializeObject<int>("test");

            EXPECT_EQ(ret, 5);

            EXPECT_EQ(logger.getNbError(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(serialize_test, serialize_multiple_custom)
        {
            MockLogger logger;
            fs::remove(tempSerializePath());

            Serializer serialize;

            serialize.setFile(tempSerializePath());

            TestSerializeA val = 5;

            serialize.serializeObject("test 1", val);

            TestSerializeA val2 = 35;

            serialize.serializeObject("test 2", val2);

            auto map = serialize.getSerializedMap();

            EXPECT_EQ(map.size(), 2);

            auto file = UniversalFileAccessor::openTextFile(tempSerializePath());

            EXPECT_EQ(file.data, serialize.getVersion() + "\ntest 2: Test Serial A {\n\tdata: __PGSA int {35}\n}\ntest 1: Test Serial A {\n\tdata: __PGSA int {5}\n}");

            EXPECT_EQ(logger.getNbError(), 0);
        }

    }
}
