#include "../../src/gateway/fix_parser.hpp"
#include "../release_assert.hpp"
#include <cassert>
#include <string_view>

// 11-ClientOrderID, 55-Symbol, 44-Price, 38-Quantity, 54-Side, 34-MsgSeqNum 49-SenderCompID 56-TargetCompID

namespace {
    void test_validate_order_parsing() {
        std::string_view valid_frame =
            "35=D34=10211=Test1OrderId55=BTC_USD54=138=5044=95250.7549=Sender56=Target";

        ParsedFixFields fields;
        const bool status = parse_fix_message(valid_frame, fields);

        TEST_ASSERT(status == true);
        TEST_ASSERT(fields.msg_seq_num == 102);
        TEST_ASSERT(fields.client_order_id.to_string_view() == "Test1OrderId");
        TEST_ASSERT(fields.symbol.to_string_view() == "BTC_USD");
        TEST_ASSERT(fields.sender_comp_id.to_string_view() == "Sender");
        TEST_ASSERT(fields.target_comp_id.to_string_view() == "Target");
        TEST_ASSERT(fields.side == Side::Buy);
        TEST_ASSERT(fields.quantity == 50);
        TEST_ASSERT(fields.price == 95250.75);
        TEST_ASSERT(fields.msg_type == FixMsgType::NewSingleOrder);
    }

    void test_malformed_order_drops() {
        ParsedFixFields fields;

        std::string_view invalid_frame_1 =
            "35=D11=BadOrder55=ETH_USD38=1044=3120.0049=Sender56=Target"; //no side
        bool status = parse_fix_message(invalid_frame_1, fields);
        TEST_ASSERT(status == false);
        fields.reset();

        std::string_view invalid_frame_2 =
            "35=D54=155=ETH_USD38=1044=3120.0049=Sender56=Target"; //no client order id
        status = parse_fix_message(invalid_frame_2, fields);
        TEST_ASSERT(status == false);
        fields.reset();

        std::string_view invalid_frame_3 = "35=D54=111=BadOrder38=1044=3120.0049=Sender56=Target"; //no symbol
        status = parse_fix_message(invalid_frame_3, fields);
        TEST_ASSERT(status == false);
        fields.reset();

        std::string_view invalid_frame_4 = "";
        status = parse_fix_message(invalid_frame_4, fields);
        TEST_ASSERT(status == false);
        fields.reset();

        std::string_view invalid_frame_5 = "35=D54=1>11=BadOrder>38=10>44=3120.00>";
        status = parse_fix_message(invalid_frame_5, fields);
        TEST_ASSERT(status == false);
        fields.reset();

        std::string_view invalid_frame_6 =
            "35=D34=10211=Test1OrderId55=BTC_USD54=138=5044=95250.7549=Sender"; //no target
        status = parse_fix_message(invalid_frame_6, fields);
        TEST_ASSERT(status == false);
        fields.reset();

        std::string_view invalid_frame_7 =
            "35=D34=10211=Test1OrderId55=BTC_USD54=138=5044=95250.7556=Target"; //no sender
        status = parse_fix_message(invalid_frame_7, fields);
        TEST_ASSERT(status == false);
    }

} // namespace

void run_fix_parser_unit_tests() {
    test_validate_order_parsing();
    test_malformed_order_drops();
}