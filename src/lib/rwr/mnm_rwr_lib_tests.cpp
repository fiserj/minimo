#include "mnm_rwr_lib.cpp"

#undef WARN

#include <catch2/catch_test_macros.hpp>


// -----------------------------------------------------------------------------
// DEFERRED EXECUTION
// -----------------------------------------------------------------------------

TEST_CASE("Deferred Execution")
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
// STACK ALLOCATOR
// -----------------------------------------------------------------------------

TEST_CASE("Stack Allocator")
{
    u64 buffer[16];

    StackAllocator allocator;
    init(allocator, buffer, sizeof(buffer));
    REQUIRE(allocator.size == sizeof(buffer));
    REQUIRE(allocator.top == 8);
    REQUIRE(allocator.last == 0);

    void* first = BX_ALLOC(&allocator, 16);
    REQUIRE(first != nullptr);
    REQUIRE(allocator.owns(first));
    REQUIRE(allocator.size == sizeof(buffer));
    REQUIRE(allocator.top == 32);
    REQUIRE(allocator.last == 8);

    void* second = BX_ALLOC(&allocator, 8);
    REQUIRE(second != nullptr);
    REQUIRE(allocator.owns(second));
    REQUIRE(allocator.top == 48);
    REQUIRE(allocator.last == 32);

    void* third = BX_ALLOC(&allocator, 128);
    REQUIRE(third == nullptr);
    REQUIRE(!allocator.owns(third));
    REQUIRE(allocator.top == 48);
    REQUIRE(allocator.last == 32);

    BX_FREE(&allocator, third);
    REQUIRE(allocator.top == 48);
    REQUIRE(allocator.last == 32);

    void* second_realloced = BX_REALLOC(&allocator, second, 16);
    REQUIRE(second == second_realloced);
    REQUIRE(allocator.owns(second_realloced));
    REQUIRE(allocator.top == 56);
    REQUIRE(allocator.last == 32);

    void* first_realloced = BX_REALLOC(&allocator, first, 8);
    REQUIRE(first != first_realloced);
    REQUIRE(allocator.owns(first_realloced));
    REQUIRE(allocator.top == 72);
    REQUIRE(allocator.last == 56);

    BX_FREE(&allocator, second_realloced);
    REQUIRE(allocator.top == 72);
    REQUIRE(allocator.last == 56);

    BX_FREE(&allocator, first_realloced);
    REQUIRE(allocator.top == 8);
    REQUIRE(allocator.last == 0);
}


// -----------------------------------------------------------------------------
// DYNAMIC ARRAY
// -----------------------------------------------------------------------------

TEST_CASE("Dynamic Array")
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

