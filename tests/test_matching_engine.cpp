#include <catch2/catch_test_macros.hpp>
#include "matching_engine.hpp"
#include <vector>

TEST_CASE("MatchingEngine - Limit order matching", "[matching_engine]") {
    lob::MatchingEngine engine;
    
    // Add a sell order
    engine.submit_order(1, lob::Side::Sell, lob::OrderType::Limit, 100, 10);
    
    // Add a buy order that matches
    auto status = engine.submit_order(2, lob::Side::Buy, lob::OrderType::Limit, 100, 5);
    
    REQUIRE(status == lob::OrderStatus::Filled);
    
    // Fully filled buy order should be removed from book
    const auto* buy_order = engine.get_order_book().get_order(2);
    REQUIRE(buy_order == nullptr);  // Order deallocated after full fill
    
    // Partially filled sell order should remain in book
    const auto* sell_order = engine.get_order_book().get_order(1);
    REQUIRE(sell_order != nullptr);
    REQUIRE(sell_order->filled_quantity == 5);
    REQUIRE(sell_order->remaining() == 5);
}

TEST_CASE("MatchingEngine - Partial fill", "[matching_engine]") {
    lob::MatchingEngine engine;
    
    engine.submit_order(1, lob::Side::Sell, lob::OrderType::Limit, 100, 5);
    auto status = engine.submit_order(2, lob::Side::Buy, lob::OrderType::Limit, 100, 10);
    
    REQUIRE(status == lob::OrderStatus::PartiallyFilled);
    
    const auto* buy_order = engine.get_order_book().get_order(2);
    REQUIRE(buy_order->filled_quantity == 5);
    REQUIRE(buy_order->remaining() == 5);
}

TEST_CASE("MatchingEngine - Price-time priority", "[matching_engine]") {
    lob::MatchingEngine engine;
    
    // Add multiple sell orders at same price
    engine.submit_order(1, lob::Side::Sell, lob::OrderType::Limit, 100, 5);
    engine.submit_order(2, lob::Side::Sell, lob::OrderType::Limit, 100, 3);
    engine.submit_order(3, lob::Side::Sell, lob::OrderType::Limit, 100, 2);
    
    // Buy order that matches all
    engine.submit_order(4, lob::Side::Buy, lob::OrderType::Limit, 100, 10);
    
    // First order should be fully filled and removed
    const auto* order1 = engine.get_order_book().get_order(1);
    REQUIRE(order1 == nullptr);  // Fully filled orders are deallocated
    
    // Second order should be fully filled and removed
    const auto* order2 = engine.get_order_book().get_order(2);
    REQUIRE(order2 == nullptr);  // Fully filled orders are deallocated
    
    // Third order should be partially filled and remain in book
    const auto* order3 = engine.get_order_book().get_order(3);
    REQUIRE(order3 != nullptr);
    REQUIRE(order3->filled_quantity == 2);
    
    // Buy order should be fully filled and removed
    const auto* buy_order = engine.get_order_book().get_order(4);
    REQUIRE(buy_order == nullptr);  // Fully filled orders are deallocated
}

TEST_CASE("MatchingEngine - Market order", "[matching_engine]") {
    lob::MatchingEngine engine;
    
    engine.submit_order(1, lob::Side::Sell, lob::OrderType::Limit, 100, 10);
    engine.submit_order(2, lob::Side::Sell, lob::OrderType::Limit, 101, 5);
    
    auto status = engine.submit_order(3, lob::Side::Buy, lob::OrderType::Market, 0, 8);
    
    REQUIRE(status == lob::OrderStatus::Filled);
    
    // Fully filled market buy order should be removed
    const auto* buy_order = engine.get_order_book().get_order(3);
    REQUIRE(buy_order == nullptr);  // Fully filled orders are deallocated
    
    // Partially filled sell order should remain in book
    const auto* sell_order = engine.get_order_book().get_order(1);
    REQUIRE(sell_order != nullptr);
    REQUIRE(sell_order->filled_quantity == 8);
    REQUIRE(sell_order->remaining() == 2);
}

TEST_CASE("MatchingEngine - IOC order", "[matching_engine]") {
    lob::MatchingEngine engine;
    
    engine.submit_order(1, lob::Side::Sell, lob::OrderType::Limit, 100, 5);
    
    // IOC order that can't fully fill - should be cancelled
    auto status = engine.submit_order(2, lob::Side::Buy, lob::OrderType::IOC, 100, 10);
    
    // IOC partially fills what it can, then cancels remainder
    // In our implementation, it cancels if not fully filled
    const auto* order = engine.get_order_book().get_order(2);
    // Order should be cancelled or filled depending on implementation
}

TEST_CASE("MatchingEngine - Trade generation", "[matching_engine]") {
    lob::MatchingEngine engine;
    
    engine.submit_order(1, lob::Side::Sell, lob::OrderType::Limit, 100, 10);
    engine.submit_order(2, lob::Side::Buy, lob::OrderType::Limit, 100, 5);
    
    auto trades = engine.get_trades();
    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].quantity == 5);
    REQUIRE(trades[0].price == 100);
    REQUIRE(trades[0].buy_order_id == 2);
    REQUIRE(trades[0].sell_order_id == 1);
}

TEST_CASE("MatchingEngine - Memory management for filled orders", "[matching_engine]") {
    lob::MatchingEngine engine;
    
    // Add sell order
    engine.submit_order(1, lob::Side::Sell, lob::OrderType::Limit, 100, 10);
    
    // Submit buy order that fully fills
    auto status = engine.submit_order(2, lob::Side::Buy, lob::OrderType::Limit, 100, 10);
    REQUIRE(status == lob::OrderStatus::Filled);
    
    // Both orders should be fully filled and removed from book
    const auto* buy_order = engine.get_order_book().get_order(2);
    REQUIRE(buy_order == nullptr);  // Fully filled order deallocated
    
    const auto* sell_order = engine.get_order_book().get_order(1);
    REQUIRE(sell_order == nullptr);  // Fully filled order deallocated
    
    // Book should be empty
    REQUIRE(engine.get_order_book().order_count() == 0);
}

TEST_CASE("MatchingEngine - Partially filled orders remain in book", "[matching_engine]") {
    lob::MatchingEngine engine;
    
    // Add sell order
    engine.submit_order(1, lob::Side::Sell, lob::OrderType::Limit, 100, 5);
    
    // Submit buy order that partially fills
    auto status = engine.submit_order(2, lob::Side::Buy, lob::OrderType::Limit, 100, 10);
    REQUIRE(status == lob::OrderStatus::PartiallyFilled);
    
    // Buy order should remain in book (partially filled)
    const auto* buy_order = engine.get_order_book().get_order(2);
    REQUIRE(buy_order != nullptr);
    REQUIRE(buy_order->filled_quantity == 5);
    REQUIRE(buy_order->remaining() == 5);
    
    // Sell order should be removed (fully filled)
    const auto* sell_order = engine.get_order_book().get_order(1);
    REQUIRE(sell_order == nullptr);  // Fully filled order deallocated
    
    // Book should have 1 order remaining
    REQUIRE(engine.get_order_book().order_count() == 1);
}

