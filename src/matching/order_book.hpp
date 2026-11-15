/*
Standard price time priority orderbook.

Price always wins. 
For buyers, the person willing to pay the highest price gets to skip the line and trade first. 
For sellers, the person willing to accept the lowest price trades first

A limit order to buy means  buy at this price or lower. A limit order to sell means sell at this price or higher

If two or more traders bid the exact same price, the system uses a strict FIFO queue... whoever's packet hits
 the matching engine's network port first gets their order filled first.
*/

#pragma once
#include "../common/fixed_string.hpp"
#include "../common/sbe_messages.hpp"
#include <algorithm>
#include <boost/circular_buffer.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/static_vector.hpp>
#include <cstdint>
#include <string_view>
#include <utility>

constexpr std::size_t MAX_ORDER_PER_PRICE = 100;
constexpr std::size_t MAX_ORDER_LEVELS = 16384;

struct RestingOrder {
    common::FixedString<32> client_order_id;
    common::FixedString<32> sender_comp_id;
    uint32_t quantity{0};
};

struct PriceLevel {
    boost::circular_buffer<RestingOrder> orders{MAX_ORDER_PER_PRICE};
    uint32_t total_volume{0};
};

class OrderBook {
public:
    explicit OrderBook(std::string_view symbol) : m_symbol(symbol) {}

    template <typename OnOrderExeutedCallback, typename MarketDataCallback>
    void process_order(const common::SbeNewOrderSingle& inbound_order, OnOrderExeutedCallback&& on_order_executed_cb,
                       MarketDataCallback&& market_data_cb) noexcept {
        if (inbound_order.side == common::SbeSide::Buy) {
            match_buy_order(inbound_order, std::forward<OnOrderExeutedCallback>(on_order_executed_cb),
                            std::forward<MarketDataCallback>(market_data_cb));
        } else if (inbound_order.side == common::SbeSide::Sell) {
            match_sell_order(inbound_order, std::forward<OnOrderExeutedCallback>(on_order_executed_cb),
                             std::forward<MarketDataCallback>(market_data_cb));
        }
    }

private:
    /* Bid: The key is the price and the value is the actual queue of bids with the highest price at the top
       Ask: The key is the price and the value is the actual queue of asks with the lowest price at the top 
       
       flat_map : Underlying storage uses a static_vector which is stack allocated [zero heap allocations, high CPU cache locality].
       Insertion/deletion uses binary search O(log N) (halving) to find the correct index, shifts elements O(N) to keep the array sorted at all times.
       Top of Book Access: Because it is sorted, accessing the best Bid or Ask at index 0 is instant O(1).
       
    */

    using BidMap =
        boost::container::flat_map<int64_t, PriceLevel, std::greater<int64_t>,
                                   boost::container::static_vector<std::pair<int64_t, PriceLevel>, MAX_ORDER_LEVELS>>;

    using AskMap =
        boost::container::flat_map<int64_t, PriceLevel, std::less<int64_t>,
                                   boost::container::static_vector<std::pair<int64_t, PriceLevel>, MAX_ORDER_LEVELS>>;

    BidMap m_bids;
    AskMap m_asks;

    template <typename MapType, typename MarketDataCallback>
    void add_resting_order(MapType& book, int64_t price, std::string_view order_id, std::string_view sender_id,
                           uint32_t qty, common::SbeSide side, MarketDataCallback&& market_data_cb) noexcept {
        if (book.size() >= MAX_ORDER_LEVELS && book.find(price) == book.end()) {
            return; // Book depth full..production system would use huge pages
        }

        PriceLevel& level = book[price];
        if (level.orders.full()) {
            return; // Level queue full, drop or reject
        }

        level.orders.push_back(RestingOrder{order_id, sender_id, qty});
        level.total_volume += qty;

        common::SbeMarketDataUpdate md_update{};
        std::copy_n(m_symbol.data(), std::min<size_t>(m_symbol.size(), 15), md_update.symbol);
        md_update.symbol[15] = '\0';
        md_update.side = side;
        md_update.action = common::SbeMarketDataAction::Update;
        md_update.price_mantissa = price;
        md_update.price_level_volume = level.total_volume;
        market_data_cb(md_update);
    }

    template <typename OnOrderExeutedCallback, typename MarketDataCallback>
    void match_buy_order(const common::SbeNewOrderSingle& order, OnOrderExeutedCallback&& on_order_executed_cb,
                         MarketDataCallback&& market_data_cb) noexcept {
        uint32_t remaining_qty = order.quantity;
        const int64_t limit_price = order.price_mantissa;

        auto it = m_asks.begin();

        //start at best ask/the cheapest seller : sellers price <= to the buyers : incoming order still needs to buy more
        while (it != m_asks.end() && it->first <= limit_price && remaining_qty > 0) {
            PriceLevel& level = it->second; //represents all sellers resting at this exact price

            while (!level.orders.empty() && remaining_qty > 0) {
                RestingOrder& resting = level.orders.front();

                //how many units can actually cross right now
                //Buyer wants 10, seller has 3 fill_qty = 3
                //Buyer wants 2 , seller has 10 fill_qty = 2
                const uint32_t fill_qty = std::min(remaining_qty, resting.quantity);
                remaining_qty -= fill_qty;
                resting.quantity -= fill_qty;
                level.total_volume -= fill_qty;

                //mv: volume is subtracted
                common::SbeMarketDataUpdate md_update{};
                std::copy_n(m_symbol.data(), std::min<size_t>(m_symbol.size(), 15), md_update.symbol);
                md_update.symbol[15] = '\0';
                md_update.side = common::SbeSide::Sell;
                md_update.action = (level.total_volume == 0) ? common::SbeMarketDataAction::Delete
                                                             : common::SbeMarketDataAction::Update;
                md_update.price_mantissa = it->first;
                md_update.price_level_volume = level.total_volume;
                market_data_cb(md_update);

                uint64_t exec_id = m_exec_id++;

                // notify the resting seller
                common::SbeExecutionReport resting_exec{};

                std::strncpy(resting_exec.client_order_id, resting.client_order_id.data(),
                             sizeof(resting_exec.client_order_id));
                std::strncpy(resting_exec.sender_comp_id, resting.sender_comp_id.data(),
                             sizeof(resting_exec.sender_comp_id));
                std::strncpy(resting_exec.symbol, m_symbol.data(), sizeof(resting_exec.symbol));
                resting_exec.exec_id = exec_id;
                resting_exec.side = common::SbeSide::Sell;
                resting_exec.exec_type =
                    (resting.quantity == 0) ? common::SbeExecType::Fill : common::SbeExecType::PartialFill;
                resting_exec.match_price = it->first;
                resting_exec.filled_quantity = fill_qty;
                resting_exec.leaves_quantity = resting.quantity;
                on_order_executed_cb(resting_exec);

                //notify the buyer
                common::SbeExecutionReport inbound_exec{};

                std::strncpy(inbound_exec.client_order_id, order.client_order_id, sizeof(inbound_exec.client_order_id));
                std::strncpy(inbound_exec.sender_comp_id, order.sender_comp_id, sizeof(inbound_exec.sender_comp_id));
                std::strncpy(inbound_exec.symbol, m_symbol.data(), sizeof(inbound_exec.symbol));
                inbound_exec.exec_id = exec_id;
                inbound_exec.side = common::SbeSide::Buy;
                inbound_exec.exec_type =
                    (remaining_qty == 0) ? common::SbeExecType::Fill : common::SbeExecType::PartialFill;
                inbound_exec.match_price = it->first;
                inbound_exec.filled_quantity = fill_qty;
                inbound_exec.leaves_quantity = remaining_qty;
                on_order_executed_cb(inbound_exec);

                //If the resting sellers order was completely filled remove it from  queue of asks, some quantity left leave it
                if (resting.quantity == 0) {
                    level.orders.pop_front();
                }
            }

            //If all sellers at this price level were cleared  removes the price level entirely
            if (level.orders.empty()) {
                it = m_asks.erase(it);
            } else {
                ++it;
            }
        }

        //If the incoming buy order couldn't be fully filled e.g. there were no asks below its limit price at all its now a resting bid
        if (remaining_qty > 0) {
            add_resting_order(m_bids, limit_price, order.client_order_id, order.sender_comp_id, remaining_qty,
                              common::SbeSide::Buy, market_data_cb);
        }
    }

    template <typename OnOrderExeutedCallback, typename MarketDataCallback>
    void match_sell_order(const common::SbeNewOrderSingle& order, OnOrderExeutedCallback&& on_order_executed_cb,
                          MarketDataCallback&& market_data_cb) noexcept {
        uint32_t remaining_qty = order.quantity;
        const int64_t limit_price = order.price_mantissa;

        auto it = m_bids.begin();

        //start at best bid/the highest bidder : bidder price >= to the buyers : incoming order still needs to sell more
        while (it != m_bids.end() && it->first >= limit_price && remaining_qty > 0) {
            PriceLevel& level = it->second; //represents all buyers resting at this exact price

            while (!level.orders.empty() && remaining_qty > 0) {
                RestingOrder& resting = level.orders.front();

                const uint32_t fill_qty = std::min(remaining_qty, resting.quantity);
                remaining_qty -= fill_qty;
                resting.quantity -= fill_qty;
                level.total_volume -= fill_qty;

                common::SbeMarketDataUpdate md_update{};
                std::copy_n(m_symbol.data(), std::min<size_t>(m_symbol.size(), 15), md_update.symbol);
                md_update.symbol[15] = '\0';
                md_update.side = common::SbeSide::Buy;
                md_update.action = (level.total_volume == 0) ? common::SbeMarketDataAction::Delete
                                                             : common::SbeMarketDataAction::Update;
                md_update.price_mantissa = it->first;
                md_update.price_level_volume = level.total_volume;
                market_data_cb(md_update);

                uint64_t exec_id = m_exec_id++;

                // Notify the resting buyer
                common::SbeExecutionReport resting_exec{};

                std::strncpy(resting_exec.client_order_id, resting.client_order_id.data(),
                             sizeof(resting_exec.client_order_id));
                std::strncpy(resting_exec.sender_comp_id, resting.sender_comp_id.data(),
                             sizeof(resting_exec.sender_comp_id));
                std::strncpy(resting_exec.symbol, m_symbol.data(), sizeof(resting_exec.symbol));

                resting_exec.exec_id = exec_id;
                resting_exec.side = common::SbeSide::Buy;
                resting_exec.exec_type =
                    (resting.quantity == 0) ? common::SbeExecType::Fill : common::SbeExecType::PartialFill;
                resting_exec.match_price = it->first;
                resting_exec.filled_quantity = fill_qty;
                resting_exec.leaves_quantity = resting.quantity;

                on_order_executed_cb(resting_exec);

                // Notify the inbound seller
                common::SbeExecutionReport inbound_exec{};
                std::strncpy(inbound_exec.client_order_id, order.client_order_id, sizeof(inbound_exec.client_order_id));
                std::strncpy(inbound_exec.sender_comp_id, order.sender_comp_id, sizeof(inbound_exec.sender_comp_id));
                std::strncpy(inbound_exec.symbol, m_symbol.data(), sizeof(inbound_exec.symbol));
                inbound_exec.exec_id = exec_id;
                inbound_exec.side = common::SbeSide::Sell;
                inbound_exec.exec_type =
                    (remaining_qty == 0) ? common::SbeExecType::Fill : common::SbeExecType::PartialFill;
                inbound_exec.match_price = it->first;
                inbound_exec.filled_quantity = fill_qty;
                inbound_exec.leaves_quantity = remaining_qty;
                on_order_executed_cb(inbound_exec);

                //If the resting buyers order was completely filled remove it from  queue of bids, some quantity left leave it
                if (resting.quantity == 0) {
                    level.orders.pop_front();
                }
            }

            //If all buyers at this price level were cleared  removes the price level entirely
            if (level.orders.empty()) {
                it = m_bids.erase(it);
            } else {
                ++it;
            }
        }

        //If the incoming sell order couldn't be fully filled...
        if (remaining_qty > 0) {
            add_resting_order(m_asks, limit_price, order.client_order_id, order.sender_comp_id, remaining_qty,
                              common::SbeSide::Sell, market_data_cb);
        }
    }

    common::FixedString<16> m_symbol;
    uint64_t m_exec_id = 0;
};
