#include "../../src/matching/order_book.hpp"
#include "../release_assert.hpp"
#include <cassert>
#include <vector>

namespace {
    common::SbeNewOrderSingle make_order(const char* id, common::SbeSide side, uint32_t qty, int64_t price) {
        common::SbeNewOrderSingle o{};
        std::strncpy(o.client_order_id, id, sizeof(o.client_order_id) - 1);
        std::strncpy(o.symbol, "SYM_1", sizeof(o.symbol) - 1);
        o.side = side;
        o.quantity = qty;
        o.price_mantissa = price;
        return o;
    }

    void test_exact_match() {
        OrderBook ob("SYM_1");

        std::vector<common::SbeExecutionReport> execution_reports;

        auto cb = [&](const common::SbeExecutionReport& exec) { execution_reports.push_back(exec); };
        auto cb_mv = [&]([[maybe_unused]] const common::SbeMarketDataUpdate& update) {};

        ob.process_order(make_order("sell_1", common::SbeSide::Sell, 10, 65000), cb, cb_mv);
        TEST_ASSERT(execution_reports.empty()); // Order should rest on the book, no trade yet

        ob.process_order(make_order("buy_1", common::SbeSide::Buy, 10, 65000), cb, cb_mv);
        TEST_ASSERT(execution_reports.size() == 2); // full order matched.

        //check sell
        TEST_ASSERT(strcmp(execution_reports[0].client_order_id, "sell_1") == 0);
        TEST_ASSERT(execution_reports[0].side == common::SbeSide::Sell);
        TEST_ASSERT(execution_reports[0].exec_type == common::SbeExecType::Fill);
        TEST_ASSERT(execution_reports[0].filled_quantity == 10);
        TEST_ASSERT(execution_reports[0].leaves_quantity == 0);
        TEST_ASSERT(execution_reports[0].match_price == 65000);

        //check buy
        TEST_ASSERT(strcmp(execution_reports[1].client_order_id, "buy_1") == 0);
        TEST_ASSERT(execution_reports[1].side == common::SbeSide::Buy);
        TEST_ASSERT(execution_reports[1].exec_type == common::SbeExecType::Fill);
        TEST_ASSERT(execution_reports[1].filled_quantity == 10);
        TEST_ASSERT(execution_reports[1].leaves_quantity == 0);

        TEST_ASSERT(execution_reports[0].exec_id == execution_reports[1].exec_id);
    }

    void test_price_priority() {
        OrderBook ob{"BTC-EUR"};
        std::vector<common::SbeExecutionReport> execution_reports;

        auto cb = [&](const common::SbeExecutionReport& exec) { execution_reports.push_back(exec); };
        auto cb_mv = [&]([[maybe_unused]] const common::SbeMarketDataUpdate& update) {};

        ob.process_order(make_order("sell_expensive", common::SbeSide::Sell, 10, 66000), cb, cb_mv);
        ob.process_order(make_order("sell_cheap", common::SbeSide::Sell, 5, 64000), cb, cb_mv);

        // Buyer takes 8 at 66000.
        // Should clear the 5 cheap ones first, then take 3 from the expensive ones.
        ob.process_order(make_order("buy_1", common::SbeSide::Buy, 8, 66000), cb, cb_mv);

        TEST_ASSERT(execution_reports.size() == 4);

        //Trade 1: Matches against sell_cheap first due to price priority
        TEST_ASSERT(strcmp(execution_reports[0].client_order_id, "sell_cheap") == 0);
        TEST_ASSERT(execution_reports[0].match_price == 64000);
        TEST_ASSERT(execution_reports[0].exec_type == common::SbeExecType::Fill);
        TEST_ASSERT(execution_reports[0].filled_quantity == 5);
        TEST_ASSERT(execution_reports[0].leaves_quantity == 0);
        TEST_ASSERT(strcmp(execution_reports[1].client_order_id, "buy_1") == 0);
        TEST_ASSERT(execution_reports[1].match_price == 64000); // Buyer gets better price
        TEST_ASSERT(execution_reports[1].filled_quantity == 5);
        TEST_ASSERT(execution_reports[1].leaves_quantity == 3);
        TEST_ASSERT(execution_reports[1].match_price == 64000);
        TEST_ASSERT(execution_reports[1].exec_type == common::SbeExecType::PartialFill);

        //Trade 2: Matches remaining against sell_expensive
        TEST_ASSERT(strcmp(execution_reports[2].client_order_id, "sell_expensive") == 0);
        TEST_ASSERT(execution_reports[2].match_price == 66000);
        TEST_ASSERT(execution_reports[2].exec_type == common::SbeExecType::PartialFill);
        TEST_ASSERT(execution_reports[2].filled_quantity == 3);
        TEST_ASSERT(execution_reports[2].leaves_quantity == 7); //7 remains on book
        TEST_ASSERT(strcmp(execution_reports[3].client_order_id, "buy_1") == 0);
        TEST_ASSERT(execution_reports[3].filled_quantity == 3);
        TEST_ASSERT(execution_reports[3].leaves_quantity == 0);
        TEST_ASSERT(execution_reports[3].match_price == 66000);
        TEST_ASSERT(execution_reports[3].exec_type == common::SbeExecType::Fill); // Buyer is now fully filled
    }

    void test_time_priority() {
        //test two sellers at exact same price, first one in gets filled first

        OrderBook ob{"BTC-EUR"};
        std::vector<common::SbeExecutionReport> execution_reports;

        auto cb = [&](const common::SbeExecutionReport& exec) { execution_reports.push_back(exec); };
        auto cb_mv = [&]([[maybe_unused]] const common::SbeMarketDataUpdate& update) {};

        ob.process_order(make_order("sell_1", common::SbeSide::Sell, 5, 64000), cb, cb_mv);
        ob.process_order(make_order("sell_2", common::SbeSide::Sell, 10, 64000), cb, cb_mv);

        // Buyer takes 8 at 64000.
        // Should clear sell_1 first, then take 3 from sell_2.
        ob.process_order(make_order("buy_1", common::SbeSide::Buy, 8, 66000), cb, cb_mv);

        TEST_ASSERT(execution_reports.size() == 4);

        //Trade 1: Matches against sell_1 first due to time priority
        TEST_ASSERT(strcmp(execution_reports[0].client_order_id, "sell_1") == 0);
        TEST_ASSERT(execution_reports[0].match_price == 64000);
        TEST_ASSERT(execution_reports[0].exec_type == common::SbeExecType::Fill);
        TEST_ASSERT(execution_reports[0].filled_quantity == 5);
        TEST_ASSERT(execution_reports[0].leaves_quantity == 0);
        TEST_ASSERT(strcmp(execution_reports[1].client_order_id, "buy_1") == 0);
        TEST_ASSERT(execution_reports[1].match_price == 64000); // Buyer gets better price
        TEST_ASSERT(execution_reports[1].filled_quantity == 5);
        TEST_ASSERT(execution_reports[1].leaves_quantity == 3);
        TEST_ASSERT(execution_reports[1].match_price == 64000);
        TEST_ASSERT(execution_reports[1].exec_type == common::SbeExecType::PartialFill);

        //Trade 2: Matches remaining against sell_2
        TEST_ASSERT(strcmp(execution_reports[2].client_order_id, "sell_2") == 0);
        TEST_ASSERT(execution_reports[2].match_price == 64000);
        TEST_ASSERT(execution_reports[2].exec_type == common::SbeExecType::PartialFill);
        TEST_ASSERT(execution_reports[2].filled_quantity == 3);
        TEST_ASSERT(execution_reports[2].leaves_quantity == 7); //7 remains on book
        TEST_ASSERT(strcmp(execution_reports[3].client_order_id, "buy_1") == 0);
        TEST_ASSERT(execution_reports[3].filled_quantity == 3);
        TEST_ASSERT(execution_reports[3].leaves_quantity == 0);
        TEST_ASSERT(execution_reports[3].match_price == 64000);
        TEST_ASSERT(execution_reports[3].exec_type == common::SbeExecType::Fill); // Buyer is now fully filled
    }

} // namespace

void run_order_book_unit_tests() {
    test_exact_match();
    test_price_priority();
    test_time_priority();
}