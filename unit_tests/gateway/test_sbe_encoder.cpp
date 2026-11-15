#include "../../src/common/sbe_messages.hpp"
#include "../../src/gateway/sbe_encoder.hpp"
#include "../release_assert.hpp"
#include <cassert>
#include <string_view>

namespace {
    void test_encode_buy_order() {
        ParsedFixFields fix_fields{};
        fix_fields.client_order_id = "ORD12345";
        fix_fields.sender_comp_id = "COMPANY1";
        fix_fields.symbol = "SYM_1";
        fix_fields.price = 65000.50;
        fix_fields.quantity = 10;
        fix_fields.side = Side::Buy;
        fix_fields.msg_seq_num = 42;

        common::SbeNewOrderSingle sbe_msg = SbeEncoder::encode_new_order(fix_fields);

        TEST_ASSERT(sbe_msg.header.template_id == static_cast<uint16_t>(common::MessageType::NewOrderSingle));
        TEST_ASSERT(sbe_msg.header.schema_id == 1);
        TEST_ASSERT(sbe_msg.header.version == 1);
        TEST_ASSERT(sbe_msg.header.block_length == sizeof(common::SbeNewOrderSingle) - sizeof(common::SbeHeader));

        TEST_ASSERT(sbe_msg.msg_seq_num == 42);
        TEST_ASSERT(std::string_view(sbe_msg.client_order_id) == "ORD12345");
        TEST_ASSERT(std::string_view(sbe_msg.sender_comp_id) == "COMPANY1");
        TEST_ASSERT(std::string_view(sbe_msg.symbol) == "SYM_1");

        // 65000.50 * 10000 = 650005000
        TEST_ASSERT(sbe_msg.price_mantissa == 650005000);
        TEST_ASSERT(sbe_msg.quantity == 10);
        TEST_ASSERT(sbe_msg.side == common::SbeSide::Buy);
    }

    void test_encode_sell_order() {
        ParsedFixFields fix_fields{};
        fix_fields.client_order_id = "ORDSELL123";
        fix_fields.symbol = "SYM_2";
        fix_fields.price = 3450.25;
        fix_fields.quantity = 250;
        fix_fields.side = Side::Sell;
        fix_fields.msg_seq_num = 101;

        common::SbeNewOrderSingle sbe_msg = SbeEncoder::encode_new_order(fix_fields);

        TEST_ASSERT(sbe_msg.header.template_id == static_cast<uint16_t>(common::MessageType::NewOrderSingle));
        TEST_ASSERT(sbe_msg.header.schema_id == 1);
        TEST_ASSERT(sbe_msg.header.version == 1);
        TEST_ASSERT(sbe_msg.header.block_length == sizeof(common::SbeNewOrderSingle) - sizeof(common::SbeHeader));

        TEST_ASSERT(sbe_msg.msg_seq_num == 101);
        TEST_ASSERT(std::string_view(sbe_msg.client_order_id, 10) == "ORDSELL123");
        TEST_ASSERT(std::string_view(sbe_msg.symbol) == "SYM_2");

        // 3450.25 * 10000 = 34502500
        TEST_ASSERT(sbe_msg.price_mantissa == 34502500);
        TEST_ASSERT(sbe_msg.quantity == 250);
        TEST_ASSERT(sbe_msg.side == common::SbeSide::Sell);
    }

    void test_unknown_side_mapping() {
        ParsedFixFields fix_fields{};
        fix_fields.side = Side::Unknown;

        common::SbeNewOrderSingle sbe_msg = SbeEncoder::encode_new_order(fix_fields);
        TEST_ASSERT(sbe_msg.side == common::SbeSide::Unknown);
    }

    void test_sbe_struct_alignment_and_size() {
        static_assert(sizeof(common::SbeNewOrderSingle) % 8 == 0, "Sbe struct size must be 8-byte aligned");
        static_assert(sizeof(common::SbeNewOrderSingle) == 128, "Sbe struct should fit in 2 cache lines");
    } // namespace
} // namespace

void run_sbe_encoder_unit_tests() {
    test_encode_buy_order();
    test_encode_sell_order();
    test_unknown_side_mapping();
    test_sbe_struct_alignment_and_size();
}