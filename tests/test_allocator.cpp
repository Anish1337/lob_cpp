#include <catch2/catch_test_macros.hpp>
#include "allocator/slab_allocator.hpp"
#include "types.hpp"

TEST_CASE("SlabAllocator - Basic allocation", "[allocator]") {
    lob::allocator::SlabAllocator<lob::Order> allocator(1024);
    
    auto* order1 = allocator.allocate();
    REQUIRE(order1 != nullptr);
    
    auto* order2 = allocator.allocate();
    REQUIRE(order2 != nullptr);
    REQUIRE(order1 != order2);
    
    allocator.deallocate(order1);
    allocator.deallocate(order2);
}

TEST_CASE("SlabAllocator - Reuse from free list", "[allocator]") {
    lob::allocator::SlabAllocator<lob::Order> allocator(1024);
    
    auto* order1 = allocator.allocate();
    REQUIRE(order1 != nullptr);
    
    allocator.deallocate(order1);
    
    auto* order2 = allocator.allocate();
    REQUIRE(order2 != nullptr);
    // Should reuse the same memory
    REQUIRE(order1 == order2);
}

TEST_CASE("SlabAllocator - Statistics", "[allocator]") {
    lob::allocator::SlabAllocator<lob::Order> allocator(1024);
    
    auto stats1 = allocator.get_stats();
    REQUIRE(stats1.total_slabs >= 1);
    
    std::vector<lob::Order*> orders;
    for (int i = 0; i < 10; ++i) {
        orders.push_back(allocator.allocate());
    }
    
    auto stats2 = allocator.get_stats();
    REQUIRE(stats2.objects_allocated >= 10);
    
    for (auto* order : orders) {
        allocator.deallocate(order);
    }
    
    auto stats3 = allocator.get_stats();
    REQUIRE(stats3.objects_in_free_list >= 10);
}

