#include <benchmark/benchmark.h>
#include "matching_engine.hpp"
#include <random>
#include <vector>

static void BM_MatchLimitOrders(benchmark::State& state) {
    lob::MatchingEngine engine;
    
    // Pre-populate with resting orders
    const std::size_t num_resting = state.range(0);
    for (lob::OrderId id = 1; id <= num_resting; ++id) {
        engine.submit_order(id, lob::Side::Sell, lob::OrderType::Limit, 
                           100 + (id % 10), 10);
    }
    
    lob::OrderId new_id = 10000;
    for (auto _ : state) {
        benchmark::DoNotOptimize(
            engine.submit_order(new_id++, lob::Side::Buy, lob::OrderType::Limit, 
                               105, 5)
        );
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MatchLimitOrders)->Arg(10)->Arg(100)->Arg(1000)->Unit(benchmark::kMicrosecond);

static void BM_MatchMarketOrders(benchmark::State& state) {
    lob::MatchingEngine engine;
    
    // Pre-populate with resting orders
    const std::size_t num_resting = state.range(0);
    for (lob::OrderId id = 1; id <= num_resting; ++id) {
        engine.submit_order(id, lob::Side::Sell, lob::OrderType::Limit, 
                           100 + (id % 10), 10);
    }
    
    lob::OrderId new_id = 10000;
    for (auto _ : state) {
        benchmark::DoNotOptimize(
            engine.submit_order(new_id++, lob::Side::Buy, lob::OrderType::Market, 
                               0, 5)
        );
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MatchMarketOrders)->Arg(10)->Arg(100)->Arg(1000)->Unit(benchmark::kMicrosecond);

static void BM_MatchIOCOrders(benchmark::State& state) {
    lob::MatchingEngine engine;
    
    // Pre-populate with resting orders
    for (lob::OrderId id = 1; id <= 100; ++id) {
        engine.submit_order(id, lob::Side::Sell, lob::OrderType::Limit, 
                           100 + (id % 10), 10);
    }
    
    lob::OrderId new_id = 10000;
    for (auto _ : state) {
        benchmark::DoNotOptimize(
            engine.submit_order(new_id++, lob::Side::Buy, lob::OrderType::IOC, 
                               105, 5)
        );
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MatchIOCOrders)->Unit(benchmark::kMicrosecond);

static void BM_MatchFOKOrders(benchmark::State& state) {
    lob::MatchingEngine engine;
    
    // Pre-populate with resting orders
    for (lob::OrderId id = 1; id <= 100; ++id) {
        engine.submit_order(id, lob::Side::Sell, lob::OrderType::Limit, 
                           100 + (id % 10), 10);
    }
    
    lob::OrderId new_id = 10000;
    for (auto _ : state) {
        benchmark::DoNotOptimize(
            engine.submit_order(new_id++, lob::Side::Buy, lob::OrderType::FOK, 
                               105, 5)
        );
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MatchFOKOrders)->Unit(benchmark::kMicrosecond);

static void BM_Throughput_MixedOrders(benchmark::State& state) {
    lob::MatchingEngine engine;
    
    // Pre-populate book
    for (lob::OrderId id = 1; id <= 500; ++id) {
        engine.submit_order(id, (id % 2 == 0) ? lob::Side::Buy : lob::Side::Sell,
                           lob::OrderType::Limit, 100 + (id % 20), 10);
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> type_dist(0, 3);
    std::uniform_int_distribution<lob::Price> price_dist(95, 105);
    std::uniform_int_distribution<lob::Quantity> qty_dist(1, 20);
    
    lob::OrderId id = 10000;
    for (auto _ : state) {
        lob::Side side = (side_dist(gen) == 0) ? lob::Side::Buy : lob::Side::Sell;
        lob::OrderType type = static_cast<lob::OrderType>(type_dist(gen));
        benchmark::DoNotOptimize(
            engine.submit_order(id++, side, type, price_dist(gen), qty_dist(gen))
        );
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Throughput_MixedOrders)->Unit(benchmark::kMicrosecond);

static void BM_PriceTimePriority(benchmark::State& state) {
    lob::MatchingEngine engine;
    
    // Create many orders at same price level to test FIFO
    const std::size_t orders_per_level = state.range(0);
    for (lob::OrderId id = 1; id <= orders_per_level; ++id) {
        engine.submit_order(id, lob::Side::Sell, lob::OrderType::Limit, 100, 1);
    }
    
    // Submit one large buy order that should match all
    for (auto _ : state) {
        engine.submit_order(10000, lob::Side::Buy, lob::OrderType::Limit, 
                           100, orders_per_level);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_PriceTimePriority)->Arg(10)->Arg(100)->Arg(1000)->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();

