#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "Compiler/value_nanbox.h"
#include "Compiler/chunk_serializer.h"

#include <sstream>

namespace pg {
namespace test {

class VMMemoryTest : public ::testing::Test
{
protected:
    VM* vm;

    void SetUp() override
    {
        vm = new VM();
    }

    void TearDown() override
    {
        delete vm;
        vm = nullptr;
    }

    // Helper to get refcount for a value
    int getRefCount(Value v)
    {
        if (not requiresRefCount(v))
            return -1; // Not ref-counted

        uint32_t index = GET_INDEX(v);
        auto& refCounts = vm->pools.getRefCountVector(v);

        if (index < refCounts.size())
        {
            return refCounts[index];
        }

        return -1;
    }
};

// ===================================================================
// Basic Retain/Release Tests
// ===================================================================

TEST_F(VMMemoryTest, PrimitivesDontNeedRefCount)
{
    Value intVal = makeIntValue(42);
    Value boolVal = makeBoolValue(true);
    Value doubleVal = makeDoubleValue(3.14);

    EXPECT_FALSE(requiresRefCount(intVal));
    EXPECT_FALSE(requiresRefCount(boolVal));
    EXPECT_FALSE(requiresRefCount(doubleVal));
}

TEST_F(VMMemoryTest, SmallStringsDontNeedRefCount)
{
    // Small strings (≤5 chars) are stored inline
    Value smallStr = makeSmallStringValue("hello", 5);
    EXPECT_TRUE(IS_SMALL_STRING(smallStr));
    EXPECT_FALSE(requiresRefCount(smallStr));
}

TEST_F(VMMemoryTest, InternedStringsDontNeedRefCount)
{
    // Interned strings live in VM's constantStrings vector
    vm->constantStrings.push_back("testString");
    Value internedStr = makeInternedStringValue(0);

    EXPECT_TRUE(IS_INTERNED_STRING(internedStr));
    EXPECT_FALSE(requiresRefCount(internedStr));
}

TEST_F(VMMemoryTest, LongStringsNeedRefCount)
{
    // Long strings (>5 chars) are heap-allocated
    Value longStr = vm->createString("this is a long string");

    EXPECT_TRUE(IS_STRING(longStr));
    EXPECT_FALSE(IS_SMALL_STRING(longStr));
    EXPECT_FALSE(IS_INTERNED_STRING(longStr));
    EXPECT_TRUE(requiresRefCount(longStr));
}

TEST_F(VMMemoryTest, NewStringHasRefCountOne)
{
    Value str = vm->createString("test string");

    int refCount = getRefCount(str);
    EXPECT_EQ(refCount, 1) << "Newly created string should have refcount=1";
}

TEST_F(VMMemoryTest, RetainIncrementsRefCount)
{
    Value str = vm->createString("test string");
    EXPECT_EQ(getRefCount(str), 1);

    vm->retainValue(str);
    EXPECT_EQ(getRefCount(str), 2);

    vm->retainValue(str);
    EXPECT_EQ(getRefCount(str), 3);
}

TEST_F(VMMemoryTest, ReleaseDecrementsRefCount)
{
    Value str = vm->createString("test string");
    vm->retainValue(str);
    vm->retainValue(str);
    EXPECT_EQ(getRefCount(str), 3);

    bool shouldDelete = vm->releaseValue(str);
    EXPECT_FALSE(shouldDelete);
    EXPECT_EQ(getRefCount(str), 2);

    shouldDelete = vm->releaseValue(str);
    EXPECT_FALSE(shouldDelete);
    EXPECT_EQ(getRefCount(str), 1);

    shouldDelete = vm->releaseValue(str);
    EXPECT_TRUE(shouldDelete) << "Release should return true when refcount reaches 0";
}

TEST_F(VMMemoryTest, ReleaseAndDeleteCleansUp)
{
    Value str = vm->createString("test string");
    uint32_t index = GET_INDEX(str);

    EXPECT_EQ(getRefCount(str), 1);

    // Store the index before deletion
    auto& stringPool = vm->pools.stringPool;
    size_t poolSizeBefore = stringPool.getNbElements();

    vm->releaseAndDelete(str);

    // After deletion, the pool slot should be freed
    // Note: The pool might reuse the slot, but the refcount should be 0 or undefined
    int refCountAfter = getRefCount(str);
    EXPECT_TRUE(refCountAfter == 0 || refCountAfter == -1)
        << "After releaseAndDelete, refcount should be 0 or invalid";
}

// ===================================================================
// Interned String Tests
// ===================================================================

TEST_F(VMMemoryTest, InternedStringNotInPool)
{
    vm->constantStrings.push_back("myConstant");
    Value internedStr = makeInternedStringValue(0);

    EXPECT_TRUE(IS_INTERNED_STRING(internedStr));
    EXPECT_FALSE(requiresRefCount(internedStr));

    // Should be marked as constant
    EXPECT_TRUE(vm->pools.isConstant(internedStr));
}

TEST_F(VMMemoryTest, InternedStringNoRetain)
{
    vm->constantStrings.push_back("myConstant");
    Value internedStr = makeInternedStringValue(0);

    // Retain should be a no-op for interned strings
    Value retained = vm->retainValue(internedStr);
    EXPECT_EQ(retained, internedStr);

    // Release should also be a no-op
    bool shouldDelete = vm->releaseValue(internedStr);
    EXPECT_FALSE(shouldDelete);
}

TEST_F(VMMemoryTest, InternedStringCanBeUsedMultipleTimes)
{
    vm->constantStrings.push_back("sharedConstant");
    Value interned1 = makeInternedStringValue(0);
    Value interned2 = makeInternedStringValue(0);

    EXPECT_EQ(interned1, interned2);
    EXPECT_TRUE(IS_INTERNED_STRING(interned1));
    EXPECT_TRUE(IS_INTERNED_STRING(interned2));

    // Both can be used without affecting refcounts
    vm->retainValue(interned1);
    vm->retainValue(interned2);
    vm->releaseValue(interned1);
    vm->releaseValue(interned2);

    // No crashes, no memory issues
}

TEST_F(VMMemoryTest, AsStringWorksForInternedStrings) {
    vm->constantStrings.push_back("myInternedString");
    Value internedStr = makeInternedStringValue(0);

    std::string result = vm->asString(internedStr);
    EXPECT_EQ(result, "myInternedString");
}

// ===================================================================
// Chunk Constants Tests
// ===================================================================

TEST_F(VMMemoryTest, ChunkConstantsNotRefCounted) {
    // Create a chunk with a constant string
    Chunk chunk;
    Value str = vm->createString("constant in chunk");

    // Mark string as constant by setting the max index
    uint32_t index = GET_INDEX(str);
    vm->pools.maxConstantStringIndex = index;

    chunk.constants.push_back(str);

    // Should be marked as constant
    EXPECT_TRUE(vm->pools.isConstant(str));

    // Retaining a constant should be a no-op
    vm->retainValue(str);
    // Release should also be a no-op for constants
    bool shouldDelete = vm->releaseValue(str);
    EXPECT_FALSE(shouldDelete);
}

// ===================================================================
// Serialization Tests
// ===================================================================

TEST_F(VMMemoryTest, SerializeInternedString) {
    // Add interned string to VM
    vm->constantStrings.push_back("internedConstant");
    Value internedStr = makeInternedStringValue(0);

    // Create a chunk with the interned string in constants
    Chunk chunk;
    chunk.constants.push_back(internedStr);

    // Serialize
    std::ostringstream out(std::ios::binary);
    bool success = ChunkSerializer::serialize(chunk, out, vm);
    EXPECT_TRUE(success);

    // Deserialize into a new VM
    VM* vm2 = new VM();
    Chunk chunk2;
    std::istringstream in(out.str(), std::ios::binary);
    success = ChunkSerializer::deserialize(chunk2, in, vm2);
    EXPECT_TRUE(success);

    // Check that constantStrings were transferred
    EXPECT_EQ(vm2->constantStrings.size(), 1);
    EXPECT_EQ(vm2->constantStrings[0], "internedConstant");

    // Check that the value was correctly deserialized
    EXPECT_EQ(chunk2.constants.size(), 1);
    Value deserializedStr = chunk2.constants[0];
    EXPECT_TRUE(IS_INTERNED_STRING(deserializedStr));
    EXPECT_EQ(AS_INTERNED_STRING_INDEX(deserializedStr), 0);

    // Verify we can read the string
    std::string result = vm2->asString(deserializedStr);
    EXPECT_EQ(result, "internedConstant");

    delete vm2;
}

TEST_F(VMMemoryTest, SerializeMixedStrings) {
    // Setup: interned string, long string, small string
    vm->constantStrings.push_back("interned");
    Value internedStr = makeInternedStringValue(0);
    Value longStr = vm->createString("this is a long heap string");
    Value smallStr = makeSmallStringValue("small", 5);

    Chunk chunk;
    chunk.constants.push_back(internedStr);
    chunk.constants.push_back(longStr);
    chunk.constants.push_back(smallStr);

    // Mark the long string as a constant (in a real chunk it would be)
    vm->pools.maxConstantStringIndex = GET_INDEX(longStr);

    // Serialize
    std::ostringstream out(std::ios::binary);
    bool success = ChunkSerializer::serialize(chunk, out, vm);
    EXPECT_TRUE(success);

    // Deserialize
    VM* vm2 = new VM();
    Chunk chunk2;
    std::istringstream in(out.str(), std::ios::binary);
    success = ChunkSerializer::deserialize(chunk2, in, vm2);
    EXPECT_TRUE(success);

    EXPECT_EQ(chunk2.constants.size(), 3);

    // Verify each type
    Value v0 = chunk2.constants[0];
    EXPECT_TRUE(IS_INTERNED_STRING(v0));
    EXPECT_EQ(vm2->asString(v0), "interned");

    Value v1 = chunk2.constants[1];
    EXPECT_TRUE(IS_STRING(v1));
    EXPECT_EQ(vm2->asString(v1), "this is a long heap string");

    Value v2 = chunk2.constants[2];
    EXPECT_TRUE(IS_SMALL_STRING(v2));
    EXPECT_EQ(vm2->asString(v2), "small");

    delete vm2;
}

// ===================================================================
// Stress Tests
// ===================================================================

TEST_F(VMMemoryTest, MultipleInternedStringsNoLeaks) {
    // Create many interned strings
    for (int i = 0; i < 100; i++) {
        vm->constantStrings.push_back("string_" + std::to_string(i));
    }

    // Create values for all of them
    std::vector<Value> values;
    for (size_t i = 0; i < 100; i++) {
        values.push_back(makeInternedStringValue(i));
    }

    // Use them multiple times
    for (auto v : values) {
        EXPECT_TRUE(IS_INTERNED_STRING(v));
        vm->retainValue(v);
        vm->releaseValue(v);
        std::string str = vm->asString(v);
        EXPECT_FALSE(str.empty());
    }

    // No memory issues should occur in TearDown
}

TEST_F(VMMemoryTest, MixedStringOperationsNoLeaks) {
    // Mix of all string types
    Value interned = makeInternedStringValue(0);
    vm->constantStrings.push_back("interned");

    Value small = makeSmallStringValue("short", 5);
    Value long1 = vm->createString("long string 1");
    Value long2 = vm->createString("long string 2");

    // Simulate typical usage patterns
    vm->retainValue(long1);
    vm->retainValue(long2);

    // Copy values
    Value copy1 = long1;
    vm->retainValue(copy1);

    // Release in different order
    vm->releaseAndDelete(copy1);
    vm->releaseAndDelete(long1);
    vm->releaseAndDelete(long2);

    // Small and interned don't need cleanup
    EXPECT_FALSE(requiresRefCount(small));
    EXPECT_FALSE(requiresRefCount(interned));
}

} // namespace test
} // namespace pg