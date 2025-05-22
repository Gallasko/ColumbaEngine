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
            fs::remove("tmpSerializeTest.sz");

            Serializer serialize;

            serialize.setFile("tmpSerializeTest.sz");

            int val = 5;

            serialize.serializeObject("test", val);

            auto map = serialize.getSerializedMap();

            EXPECT_EQ(map.size(), 1);

            auto file = UniversalFileAccessor::openTextFile("tmpSerializeTest.sz");

            EXPECT_EQ(file.data, serialize.getVersion() + "\ntest: __PGSA int {5}");

            EXPECT_EQ(logger.getNbError(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(serialize_test, deserialize_int)
        {
            MockLogger logger;
            fs::remove("tmpSerializeTest.sz");

            Serializer serialize;

            serialize.setFile("tmpSerializeTest.sz");

            int val = 5;

            serialize.serializeObject("test", val);

            auto map = serialize.getSerializedMap();

            EXPECT_EQ(map.size(), 1);

            auto file = UniversalFileAccessor::openTextFile("tmpSerializeTest.sz");

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
            fs::remove("tmpSerializeTest.sz");

            Serializer serialize;

            serialize.setFile("tmpSerializeTest.sz");

            int val = 5;

            serialize.serializeObject("test", val);

            auto map = serialize.getSerializedMap();

            EXPECT_EQ(map.size(), 1);

            auto file = UniversalFileAccessor::openTextFile("tmpSerializeTest.sz");

            EXPECT_EQ(file.data, serialize.getVersion() + "\ntest: __PGSA int {5}");

            int ret = serialize.deserializeObject<int>("test");

            EXPECT_EQ(ret, 5);

            serialize.setFile("tmpSerializeTest.sz");

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
            fs::remove("tmpSerializeTest.sz");

            Serializer serialize;

            serialize.setFile("tmpSerializeTest.sz");

            TestSerializeA val = 5;

            serialize.serializeObject("test 1", val);

            TestSerializeA val2 = 35;

            serialize.serializeObject("test 2", val2);

            auto map = serialize.getSerializedMap();

            EXPECT_EQ(map.size(), 2);

            auto file = UniversalFileAccessor::openTextFile("tmpSerializeTest.sz");

            EXPECT_EQ(file.data, serialize.getVersion() + "\ntest 2: Test Serial A {\n\tdata: __PGSA int {35}\n}\ntest 1: Test Serial A {\n\tdata: __PGSA int {5}\n}");

            EXPECT_EQ(logger.getNbError(), 0);
        }

    }
}
