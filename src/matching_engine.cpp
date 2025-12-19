#include "matching_engine.hpp"
#include <algorithm>
#include <optional>  // C++23: std::optional monadic operations

namespace lob {

MatchingEngine::MatchingEngine(TradeCallback trade_callback)
    : order_book_(trade_callback)
    , trade_callback_(trade_callback)
{
}

OrderStatus MatchingEngine::submit_order(OrderId id, Side side, OrderType type,
                                        Price price, Quantity quantity) {
    if (quantity == 0) {
        return OrderStatus::Rejected;
    }
    
    if (!order_book_.add_order(id, side, type, price, quantity)) {
        return OrderStatus::Rejected;
    }
    
    const Order* order = order_book_.get_order(id);
    if (!order) {
        return OrderStatus::Rejected;
    }
    
    // TODO: Refactor OrderBook to return non-const Order* to avoid const_cast
    // We need mutable access to update filled_quantity during matching
    Order* mutable_order = const_cast<Order*>(order);
    match_order(mutable_order);
    
    // C++23: Use optional monadic operations for cleaner null checking
    auto final_status = std::optional<const Order*>{order_book_.get_order(id)}
        .transform([this](const Order* o) {
            Order* mutable_o = const_cast<Order*>(o);
            OrderStatus status = mutable_o->status;
            if (mutable_o->is_filled()) {
                order_book_.remove_filled_order(mutable_o);
                status = OrderStatus::Filled;
            }
            return status;
        })
        .value_or(OrderStatus::Cancelled);
    
    return final_status;
}

bool MatchingEngine::cancel_order(OrderId id) {
    return order_book_.cancel_order(id);
}

bool MatchingEngine::modify_order(OrderId id, Price new_price, Quantity new_quantity) {
    // Modification is implemented as cancel + re-add with remaining quantity
    // This preserves filled quantity and maintains order book integrity
    const Order* old_order = order_book_.get_order(id);
    if (!old_order) {
        return false;
    }
    
    Side side = old_order->side;
    OrderType type = old_order->type;
    Quantity filled = old_order->filled_quantity;
    
    // Can't reduce quantity below already filled amount
    if (new_quantity < filled) {
        return false;
    }
    
    // Cancel existing order
    if (!order_book_.cancel_order(id)) {
        return false;
    }
    
    // Re-add with new price/quantity (only remaining unfilled portion)
    Quantity remaining = new_quantity - filled;
    if (remaining > 0) {
        return order_book_.add_order(id, side, type, new_price, remaining);
    }
    
    return true;
}

void MatchingEngine::match_order(Order* order) {
    switch (order->type) {
        case OrderType::Limit:
            match_limit_order(order);
            break;
        case OrderType::Market:
            match_market_order(order);
            break;
        case OrderType::IOC:
            match_ioc_order(order);
            break;
        case OrderType::FOK:
            match_fok_order(order);
            break;
    }
}

void MatchingEngine::match_limit_order(Order* order) {
    // TODO: Extract common matching logic to reduce duplication between buy/sell paths
    // The buy and sell matching logic is nearly identical - could be refactored
    
    if (order->side == Side::Buy) {
        // Buy order matches against sell orders (asks)
        // Match as long as our bid price >= best ask price (price-time priority)
        while (!order->is_filled()) {
            auto best_ask = order_book_.best_ask();
            // Stop if no asks available or our price is too low
            if (!best_ask || order->price < *best_ask) {
                break;
            }
            
            // Match at the best ask price (price priority)
            Price match_price = *best_ask;
            // Get first order at this price level (time priority - FIFO)
            Order* ask_order = order_book_.get_first_order_at_price(Side::Sell, match_price);
            if (!ask_order) {
                break;
            }
            
            // Fill the smaller of the two remaining quantities
            Quantity trade_qty = std::min(order->remaining(), ask_order->remaining());
            Quantity old_remaining = ask_order->remaining();
            
            order->filled_quantity += trade_qty;
            ask_order->filled_quantity += trade_qty;
            
            // Update price level total quantity efficiently (incremental update)
            order_book_.update_price_level_quantity_incremental(ask_order, old_remaining);
            
            // Record trade if callback exists or we're tracking trades
            if (trade_callback_ || !trades_.empty()) {
                Trade trade{
                    .buy_order_id = order->id,
                    .sell_order_id = ask_order->id,
                    .price = match_price,
                    .quantity = trade_qty,
                    .timestamp = std::chrono::duration_cast<Timestamp>(
                        std::chrono::steady_clock::now().time_since_epoch()
                    )
                };
                if (trade_callback_) {
                    trade_callback_(trade);
                }
                trades_.push_back(trade);
            }
            
            // Remove filled orders from the book to maintain FIFO ordering
            if (ask_order->is_filled()) {
                order_book_.remove_filled_order(ask_order);
            }
        }
    } else {
        // Sell order matches against buy orders (bids)
        // Match as long as our ask price <= best bid price
        while (!order->is_filled()) {
            auto best_bid = order_book_.best_bid();
            // Stop if no bids available or our price is too high
            if (!best_bid || order->price > *best_bid) {
                break;
            }
            
            // Match at the best bid price (price priority)
            Price match_price = *best_bid;
            // Get first order at this price level (time priority - FIFO)
            Order* bid_order = order_book_.get_first_order_at_price(Side::Buy, match_price);
            if (!bid_order) {
                break;
            }
            
            // Fill the smaller of the two remaining quantities
            Quantity trade_qty = std::min(order->remaining(), bid_order->remaining());
            Quantity old_remaining = bid_order->remaining();
            
            bid_order->filled_quantity += trade_qty;
            order->filled_quantity += trade_qty;
            
            // Update price level total quantity efficiently (incremental update)
            order_book_.update_price_level_quantity_incremental(bid_order, old_remaining);
            
            if (trade_callback_ || !trades_.empty()) {
                Trade trade{
                    .buy_order_id = bid_order->id,
                    .sell_order_id = order->id,
                    .price = match_price,
                    .quantity = trade_qty,
                    .timestamp = std::chrono::duration_cast<Timestamp>(
                        std::chrono::steady_clock::now().time_since_epoch()
                    )
                };
                if (trade_callback_) {
                    trade_callback_(trade);
                }
                trades_.push_back(trade);
            }
            
            if (bid_order->is_filled()) {
                order_book_.remove_filled_order(bid_order);
            }
        }
    }
    
    if (order->is_filled()) {
        order->status = OrderStatus::Filled;
    } else if (order->filled_quantity > 0) {
        order->status = OrderStatus::PartiallyFilled;
    }
}

void MatchingEngine::match_market_order(Order* order) {
    if (order->side == Side::Buy) {
        while (!order->is_filled()) {
            auto best_ask = order_book_.best_ask();
            if (!best_ask) break;
            
            Order* ask_order = order_book_.get_first_order_at_price(Side::Sell, *best_ask);
            if (!ask_order) break;
            
            Quantity trade_qty = std::min(order->remaining(), ask_order->remaining());
            Quantity old_remaining = ask_order->remaining();
            
            order->filled_quantity += trade_qty;
            ask_order->filled_quantity += trade_qty;
            
            order_book_.update_price_level_quantity_incremental(ask_order, old_remaining);
            
            if (trade_callback_ || !trades_.empty()) {
                Trade trade{
                    .buy_order_id = order->id,
                    .sell_order_id = ask_order->id,
                    .price = *best_ask,
                    .quantity = trade_qty,
                    .timestamp = std::chrono::duration_cast<Timestamp>(
                        std::chrono::steady_clock::now().time_since_epoch()
                    )
                };
                if (trade_callback_) trade_callback_(trade);
                trades_.push_back(trade);
            }
            
            if (ask_order->is_filled()) {
                order_book_.remove_filled_order(ask_order);
            }
        }
    } else {
        while (!order->is_filled()) {
            auto best_bid = order_book_.best_bid();
            if (!best_bid) break;
            
            Order* bid_order = order_book_.get_first_order_at_price(Side::Buy, *best_bid);
            if (!bid_order) break;
            
            Quantity trade_qty = std::min(order->remaining(), bid_order->remaining());
            Quantity old_remaining = bid_order->remaining();
            
            bid_order->filled_quantity += trade_qty;
            order->filled_quantity += trade_qty;
            
            order_book_.update_price_level_quantity_incremental(bid_order, old_remaining);
            
            if (trade_callback_ || !trades_.empty()) {
                Trade trade{
                    .buy_order_id = bid_order->id,
                    .sell_order_id = order->id,
                    .price = *best_bid,
                    .quantity = trade_qty,
                    .timestamp = std::chrono::duration_cast<Timestamp>(
                        std::chrono::steady_clock::now().time_since_epoch()
                    )
                };
                if (trade_callback_) trade_callback_(trade);
                trades_.push_back(trade);
            }
            
            if (bid_order->is_filled()) {
                order_book_.remove_filled_order(bid_order);
            }
        }
    }
    
    if (order->is_filled()) {
        order->status = OrderStatus::Filled;
    } else if (order->filled_quantity > 0) {
        order->status = OrderStatus::PartiallyFilled;
    }
}

void MatchingEngine::match_ioc_order(Order* order) {
    // IOC (Immediate or Cancel): Match immediately, cancel any unfilled portion
    match_limit_order(order);
    if (!order->is_filled()) {
        // Cancel remaining quantity - (void) suppresses unused return value warning
        (void)order_book_.cancel_order(order->id);
    }
}

void MatchingEngine::match_fok_order(Order* order) {
    // FOK (Fill or Kill): Must fill completely or cancel entire order
    // TODO: Current implementation allows partial fills - should check if full fill
    // is possible before matching, otherwise reject immediately
    match_limit_order(order);
    if (!order->is_filled()) {
        // Cancel remaining quantity - (void) suppresses unused return value warning
        (void)order_book_.cancel_order(order->id);
    }
}

// TODO: This function is currently unused - consider removing or using it to
// reduce code duplication in match_limit_order/match_market_order
void MatchingEngine::execute_trade(Order* buy_order, Order* sell_order, 
                                   Price price, Quantity quantity) {
    Quantity fill_qty = std::min({buy_order->remaining(), 
                                  sell_order->remaining(), 
                                  quantity});
    
    buy_order->filled_quantity += fill_qty;
    sell_order->filled_quantity += fill_qty;
    
    Trade trade{
        .buy_order_id = buy_order->id,
        .sell_order_id = sell_order->id,
        .price = price,
        .quantity = fill_qty,
        .timestamp = std::chrono::duration_cast<Timestamp>(
            std::chrono::steady_clock::now().time_since_epoch()
        )
    };
    
    trades_.push_back(trade);
    
    if (trade_callback_) {
        trade_callback_(trade);
    }
}

} // namespace lob

