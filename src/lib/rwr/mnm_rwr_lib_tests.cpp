#include "mnm_rwr_lib.cpp"

#undef WARN

#include <catch2/catch_test_macros.hpp>


// -----------------------------------------------------------------------------
// DEFERRED EXECUTION
// -----------------------------------------------------------------------------

TEST_CASE("Deferred Execution", "[basic]")
{
    int value = 1;

    {
        defer(value++);

        {
            defer(
                for (int i = 0; i < 3; i++)
                {
                    value++;
                }
            );

            REQUIRE(value == 1);
        }

        REQUIRE(value == 4);
    }

    REQUIRE(value == 5);
}


// -----------------------------------------------------------------------------
// ARENA ALLOCATOR
// -----------------------------------------------------------------------------

TEST_CASE("Arena Allocator", "[basic]")
{
    BX_ALIGN_DECL_16(struct) Buffer
    {
        u8 data[128] = {};
    };

    Buffer         buffer;
    ArenaAllocator allocator;

    // Initialization.
    init(allocator, buffer.data, sizeof(buffer.data));
    REQUIRE(allocator.size == sizeof(buffer.data));
    REQUIRE(allocator.top == 0);
    REQUIRE(allocator.last == 0);

    // Non-aligned allocation I.
    void* first = BX_ALLOC(&allocator, 13);
    REQUIRE(first != nullptr);
    REQUIRE(allocator.owns(first));
    REQUIRE(allocator.size == sizeof(buffer.data));
    REQUIRE(allocator.top == 13);
    REQUIRE(allocator.last == 0);

    // Aligned allocation.
    void* second = BX_ALIGNED_ALLOC(&allocator, 16, 16);
    REQUIRE(second != nullptr);
    REQUIRE(second > first);
    REQUIRE(allocator.owns(second));
    REQUIRE(allocator.top == 32);
    REQUIRE(allocator.last == 13);

    // Non-aligned allocation I.
    void* third = BX_ALLOC(&allocator, 5);
    REQUIRE(third != nullptr);
    REQUIRE(third > second);
    REQUIRE(allocator.owns(third));
    REQUIRE(allocator.top == 37);
    REQUIRE(allocator.last == 32);

    // Failed in-place reallocation (out of space).
    void* third_realloc_failed = BX_REALLOC(&allocator, third, 100);
    REQUIRE(third_realloc_failed == nullptr);
    REQUIRE(!allocator.owns(third_realloc_failed));
    REQUIRE(allocator.top == 37);
    REQUIRE(allocator.last == 32);

    // Successful in-place reallocation.
    void* third_realloc_succeeded = BX_REALLOC(&allocator, third, 32);
    REQUIRE(third_realloc_succeeded != nullptr);
    REQUIRE(third_realloc_succeeded == third);
    REQUIRE(allocator.owns(third_realloc_succeeded));
    REQUIRE(allocator.top == 64);
    REQUIRE(allocator.last == 32);

    // Freeing of last allocation.
    BX_FREE(&allocator, third);
    REQUIRE(allocator.top == 32);
    REQUIRE(allocator.last == 32);

    // No-op freeing of previous block.
    BX_FREE(&allocator, second);
    REQUIRE(allocator.top == 32);
    REQUIRE(allocator.last == 32);

    // No-op freeing of the first block.
    BX_FREE(&allocator, first);
    REQUIRE(allocator.top == 32);
    REQUIRE(allocator.last == 32);

    // Realloc of second block (can't be done in-place).
    void* second_realloc_succeeded = BX_ALIGNED_REALLOC(&allocator, second, 64, 16);
    REQUIRE(second_realloc_succeeded != nullptr);
    REQUIRE(allocator.owns(second_realloc_succeeded));
    REQUIRE(allocator.top == 96);
    REQUIRE(allocator.last == 32);
    REQUIRE(bx::memCmp(second, second_realloc_succeeded, 16) == 0);
};


// -----------------------------------------------------------------------------
// STACK ALLOCATOR
// -----------------------------------------------------------------------------

TEST_CASE("Stack Allocator", "[basic]")
{
    BX_ALIGN_DECL_16(struct) Buffer
    {
        u8 data[128] = {};
    };

    Buffer         buffer;
    StackAllocator allocator;

    // Initialization.
    init(allocator, buffer.data, sizeof(buffer.data));
    REQUIRE(allocator.size == sizeof(buffer.data));
    REQUIRE(allocator.top == 8);
    REQUIRE(allocator.last == 0);

    // Non-aligned allocation I.
    void* first = BX_ALLOC(&allocator, 13);
    REQUIRE(first != nullptr);
    REQUIRE(allocator.owns(first));
    REQUIRE(allocator.size == sizeof(buffer.data));
    REQUIRE(allocator.top == 29);
    REQUIRE(allocator.last == 8);

    // Aligned allocation.
    void* second = BX_ALIGNED_ALLOC(&allocator, 8, 16);
    REQUIRE(second != nullptr);
    REQUIRE(allocator.owns(second));
    REQUIRE(allocator.top == 56);
    REQUIRE(allocator.last == 40);

    // Failed allocation (out of space).
    void* third = BX_ALLOC(&allocator, 128);
    REQUIRE(third == nullptr);
    REQUIRE(!allocator.owns(third));
    REQUIRE(allocator.top == 56);
    REQUIRE(allocator.last == 40);

    // No-op freeing of a `NULL` pointer.
    BX_FREE(&allocator, nullptr);
    REQUIRE(allocator.top == 56);
    REQUIRE(allocator.last == 40);

    // In-place reallocation.
    void* second_realloced = BX_ALIGNED_REALLOC(&allocator, second, 16, 16);
    REQUIRE(second == second_realloced);
    REQUIRE(allocator.owns(second_realloced));
    REQUIRE(allocator.top == 64);
    REQUIRE(allocator.last == 40);

    // Reallocation "anew".
    void* first_realloced = BX_REALLOC(&allocator, first, 8);
    REQUIRE(first != first_realloced);
    REQUIRE(allocator.owns(first_realloced));
    REQUIRE(allocator.top == 80);
    REQUIRE(allocator.last == 64);

    // Freeing the middle block.
    BX_FREE(&allocator, second_realloced);
    REQUIRE(allocator.top == 80);
    REQUIRE(allocator.last == 64);

    // Freeing last block (and also the last valid one).
    BX_FREE(&allocator, first_realloced);
    REQUIRE(allocator.top == 8);
    REQUIRE(allocator.last == 0);
}


// -----------------------------------------------------------------------------
// DYNAMIC ARRAY
// -----------------------------------------------------------------------------

TEST_CASE("Dynamic Array", "[basic]")
{
    CrtAllocator allocator;

    DynamicArray<int> array = {};
    init(array, &allocator);
    REQUIRE(array.allocator == &allocator);

    reserve(array, 3);
    REQUIRE(array.size == 0);
    REQUIRE(array.capacity >= 3);

    int val = append(array, 10);
    REQUIRE(array.size == 1);
    REQUIRE(array[0] == 10);
    REQUIRE(val == 10);

    val = append(array, 20);
    REQUIRE(array.size == 2);
    REQUIRE(array[1] == 20);
    REQUIRE(val == 20);

    val = append(array, 30);
    REQUIRE(array.size == 3);
    REQUIRE(array[2] == 30);
    REQUIRE(val == 30);

    val = pop(array);
    REQUIRE(array.size == 2);
    REQUIRE(val == 30);

    resize(array, 10, 100);
    REQUIRE(array.size == 10);
    REQUIRE(array.capacity >= 10);

    for (u32 i = 2; i < array.size; i++)
    {
        REQUIRE(array[i] == 100);
    }

    deinit(array);
    REQUIRE(array.data == nullptr);
    REQUIRE(array.size == 0);
    REQUIRE(array.capacity == 0);
    REQUIRE(array.allocator == nullptr);
}


// -----------------------------------------------------------------------------

