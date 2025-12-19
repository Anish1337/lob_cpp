#include "order_book.hpp"
#include <chrono>
#include <ranges>  // C++23: std::ranges::to
#include <source_location>  // C++23: std::source_location

namespace lob {

OrderBook::OrderBook(TradeCallback trade_callback)
    : trade_callback_(std::move(trade_callback))
{
}

OrderBook::~OrderBook() {
    clear();
}

bool OrderBook::add_order(OrderId id, Side side, OrderType type, 
                          Price price, Quantity quantity,
                          std::source_location loc) {
    // C++23: std::source_location provides compile-time file/line info for debugging
    if (quantity == 0) {
        return false;
    }
    
    if (orders_.find(id) != orders_.end()) {
        return false;  // Order ID already exists
    }
    
    // Allocate order from custom slab allocator (zero-allocation after initial setup)
    Order* order = allocator_.allocate();
    if (!order) {
        return false;
    }
    
    // Initialize order fields
    order->id = id;
    order->side = side;
    order->type = type;
    order->price = price;
    order->quantity = quantity;
    order->filled_quantity = 0;
    order->timestamp = get_timestamp();  // Used for FIFO ordering within price level
    order->status = OrderStatus::New;
    order->next = nullptr;
    order->prev = nullptr;
    
    // Get or create price level (bids use descending order, asks use ascending)
    PriceLevel* level = get_price_level(side, price);
    if (!level) {
        // Create new price level if it doesn't exist
        // TODO: Consider extracting this into a helper function to reduce duplication
        if (side == Side::Buy) {
            auto [it, inserted] = bid_levels_.emplace(price, PriceLevel{});
            level = &it->second;
            level->price = price;
        } else {
            auto [it, inserted] = ask_levels_.emplace(price, PriceLevel{});
            level = &it->second;
            level->price = price;
        }
    }
    
    // Add to price level's linked list (maintains FIFO order)
    add_order_to_level(order, *level);
    // Track order for O(1) lookup by ID
    orders_[id] = order;
    
    return true;
}

bool OrderBook::cancel_order(OrderId id) {
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return false;
    }
    
    Order* order = it->second;
    if (order->is_filled()) {
        return false;
    }
    
    remove_order_from_level(order);
    orders_.erase(it);
    allocator_.deallocate(order);
    
    return true;
}

bool OrderBook::modify_order(OrderId id, Price new_price, Quantity new_quantity) {
    if (new_quantity == 0) {
        return false;
    }
    
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return false;
    }
    
    Order* order = it->second;
    if (order->is_filled()) {
        return false;
    }
    
    if (new_quantity < order->filled_quantity) {
        return false;
    }
    
    Side side = order->side;
    OrderType type = order->type;
    Quantity filled = order->filled_quantity;
    
    // If price hasn't changed and new quantity >= old quantity, just update quantity
    if (order->price == new_price && new_quantity >= order->quantity) {
        Quantity old_remaining = order->remaining();
        order->quantity = new_quantity;
        PriceLevel* level = get_price_level(side, new_price);
        if (level) {
            level->update_quantity(order, old_remaining);
        }
        return true;
    }
    
    // Remove old order
    remove_order_from_level(order);
    orders_.erase(it);
    allocator_.deallocate(order);
    
    // Add new order with remaining quantity
    Quantity remaining = new_quantity - filled;
    if (remaining > 0) {
        if (!add_order(id, side, type, new_price, remaining)) {
            return false;
        }
        // Restore filled quantity
        auto new_it = orders_.find(id);
        if (new_it != orders_.end()) {
            new_it->second->filled_quantity = filled;
        }
        return true;
    }
    
    return true;
}

std::optional<Price> OrderBook::best_bid() const noexcept {
    // Best bid is highest buy price (begin() of descending map)
    if (bid_levels_.empty()) {
        return std::nullopt;
    }
    return bid_levels_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const noexcept {
    // Best ask is lowest sell price (begin() of ascending map)
    if (ask_levels_.empty()) {
        return std::nullopt;
    }
    return ask_levels_.begin()->first;
}

std::optional<Price> OrderBook::spread() const noexcept {
    // C++23: Use monadic operations for cleaner optional handling
    return best_bid().and_then([this](Price bid) {
        return best_ask().transform([bid](Price ask) {
            return ask - bid;
        });
    });
}

Quantity OrderBook::depth_at_price(Side side, Price price) const noexcept {
    const PriceLevel* level = get_price_level(side, price);
    if (!level) {
        return 0;
    }
    return level->total_quantity;
}

std::vector<std::pair<Price, Quantity>> OrderBook::get_levels(Side side, std::size_t n) const {
    // Returns top N price levels (market depth)
    // C++23: Use std::ranges::to for cleaner range-to-container conversion
    namespace r = std::ranges;
    
    if (side == Side::Buy) {
        // Bids: iterate from highest to lowest price
        return bid_levels_
            | r::views::take(n)
            | r::views::transform([](const auto& pair) {
                return std::make_pair(pair.first, pair.second.total_quantity);
            })
            | r::to<std::vector>();
    } else {
        // Asks: iterate from lowest to highest price
        return ask_levels_
            | r::views::take(n)
            | r::views::transform([](const auto& pair) {
                return std::make_pair(pair.first, pair.second.total_quantity);
            })
            | r::to<std::vector>();
    }
}

const Order* OrderBook::get_order(OrderId id) const noexcept {
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return nullptr;
    }
    return it->second;
}

void OrderBook::clear() {
    for (auto& [id, order] : orders_) {
        allocator_.deallocate(order);
    }
    orders_.clear();
    bid_levels_.clear();
    ask_levels_.clear();
}

Order* OrderBook::get_first_order_at_price(Side side, Price price) noexcept {
    PriceLevel* level = get_price_level(side, price);
    if (!level || level->empty()) {
        return nullptr;
    }
    return level->first_order;
}

void OrderBook::remove_order_from_level(Order* order) {
    if (!order) {
        return;
    }
    
    PriceLevel* level = get_price_level(order->side, order->price);
    if (level) {
        level->remove_order(order);
        
        // Remove empty price level
        if (level->empty()) {
            if (order->side == Side::Buy) {
                bid_levels_.erase(order->price);
            } else {
                ask_levels_.erase(order->price);
            }
        }
    }
}

void OrderBook::remove_filled_order(Order* order) {
    if (!order || !order->is_filled()) {
        return;
    }
    
    remove_order_from_level(order);
    auto it = orders_.find(order->id);
    if (it != orders_.end()) {
        orders_.erase(it);
        allocator_.deallocate(order);
    }
}

void OrderBook::update_price_level_quantity_incremental(Order* order, Quantity old_remaining) {
    if (!order) {
        return;
    }
    
    PriceLevel* level = get_price_level(order->side, order->price);
    if (level) {
        level->update_quantity(order, old_remaining);
    }
}

void OrderBook::add_order_to_level(Order* order, PriceLevel& level) {
    level.add_order(order);
}

OrderBook::PriceLevel* OrderBook::get_price_level(Side side, Price price) {
    // O(log n) lookup in price level map
    // TODO: Consider templating this to reduce duplication between const/non-const versions
    if (side == Side::Buy) {
        auto it = bid_levels_.find(price);
        return (it != bid_levels_.end()) ? &it->second : nullptr;
    } else {
        auto it = ask_levels_.find(price);
        return (it != ask_levels_.end()) ? &it->second : nullptr;
    }
}

const OrderBook::PriceLevel* OrderBook::get_price_level(Side side, Price price) const {
    // Const version for read-only access
    if (side == Side::Buy) {
        auto it = bid_levels_.find(price);
        return (it != bid_levels_.end()) ? &it->second : nullptr;
    } else {
        auto it = ask_levels_.find(price);
        return (it != ask_levels_.end()) ? &it->second : nullptr;
    }
}

Timestamp OrderBook::get_timestamp() const noexcept {
    return std::chrono::duration_cast<Timestamp>(
        std::chrono::steady_clock::now().time_since_epoch()
    );
}

} // namespace lob
