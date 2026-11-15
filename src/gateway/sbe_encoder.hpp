#pragma once
#include "fix_parser.hpp"
#include "sbe_messages.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

class SbeEncoder { //TODO namespace as irt holds no state
public:
    static inline common::SbeNewOrderSingle encode_new_order(const ParsedFixFields& fix_fields) noexcept {
        common::SbeNewOrderSingle sbe_msg{};

        sbe_msg.header.block_length = sizeof(common::SbeNewOrderSingle) - sizeof(common::SbeHeader);
        sbe_msg.header.template_id = static_cast<uint16_t>(common::MessageType::NewOrderSingle);
        sbe_msg.header.schema_id = 1;
        sbe_msg.header.version = 1;

        sbe_msg.msg_seq_num = fix_fields.msg_seq_num;

        const std::string_view cl_ord_id = fix_fields.client_order_id.to_string_view();
        const std::size_t cl_len = std::min(cl_ord_id.size(), sizeof(sbe_msg.client_order_id));
        std::memcpy(sbe_msg.client_order_id, cl_ord_id.data(), cl_len);

        const std::string_view sym = fix_fields.symbol.to_string_view();
        const std::size_t sym_len = std::min(sym.size(), sizeof(sbe_msg.symbol));
        std::memcpy(sbe_msg.symbol, sym.data(), sym_len);

        const std::string_view comp_id = fix_fields.sender_comp_id.to_string_view();
        const std::size_t comp_id_len = std::min(comp_id.size(), sizeof(sbe_msg.sender_comp_id));
        std::memcpy(sbe_msg.sender_comp_id, comp_id.data(), comp_id_len);

        sbe_msg.price_mantissa = static_cast<int64_t>(std::round(fix_fields.price * 10000.0));
        sbe_msg.quantity = static_cast<uint32_t>(fix_fields.quantity);

        if (fix_fields.side == Side::Buy) {
            sbe_msg.side = common::SbeSide::Buy;
        } else if (fix_fields.side == Side::Sell) {
            sbe_msg.side = common::SbeSide::Sell;
        } else {
            sbe_msg.side = common::SbeSide::Unknown;
        }

        return sbe_msg;
    }
};
