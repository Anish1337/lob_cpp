#pragma once

#include "order_book.hpp"
#include "types.hpp"
#include <vector>
#include <functional>

namespace lob {

class MatchingEngine {
public:
    using TradeCallback = std::function<void(const Trade&)>;
    
    explicit MatchingEngine(TradeCallback trade_callback = nullptr);
    
    [[nodiscard]] OrderStatus submit_order(OrderId id, Side side, OrderType type,
                                           Price price, Quantity quantity);
    [[nodiscard]] bool cancel_order(OrderId id);
    [[nodiscard]] bool modify_order(OrderId id, Price new_price, Quantity new_quantity);
    [[nodiscard]] const OrderBook& get_order_book() const noexcept {
        return order_book_;
    }
    [[nodiscard]] OrderBook& get_order_book() noexcept {
        return order_book_;
    }
    [[nodiscard]] std::vector<Trade> get_trades() noexcept {
        std::vector<Trade> result;
        trades_.swap(result);
        return result;
    }
    
private:
    void match_order(Order* order);
    void match_limit_order(Order* order);
    void match_market_order(Order* order);
    void match_ioc_order(Order* order);
    void match_fok_order(Order* order);
    
    void execute_trade(Order* buy_order, Order* sell_order, Price price, Quantity quantity);
    
    OrderBook order_book_;
    std::vector<Trade> trades_;
    TradeCallback trade_callback_;
};

} // namespace lob

