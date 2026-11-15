#include "fix_parser.hpp"
#include <charconv>

[[nodiscard]] inline uint64_t parse_uint64(std::string_view sv) noexcept {
    uint64_t val = 0;
    std::from_chars(sv.data(), sv.data() + sv.size(), val); //TODO error code and below..
    return val;
}

[[nodiscard]] inline double parse_double(std::string_view sv) noexcept {
    double val = 0.0;
    std::from_chars(sv.data(), sv.data() + sv.size(), val);
    return val;
}

bool parse_fix_message(std::string_view raw_frame, ParsedFixFields& out_fields) noexcept {
    if (raw_frame.empty()) {
        return false;
    }

    std::size_t cursor = 0;
    const std::size_t total_len = raw_frame.size();

    //single pass check...Get =, check number behind it. If releveant grab data.
    while (cursor < total_len) {
        const std::size_t eq_pos = raw_frame.find('=', cursor);
        if (eq_pos == std::string_view::npos) {
            break;
        }

        const std::string_view tag = raw_frame.substr(cursor, eq_pos - cursor);
        uint32_t tag_id = 0;
        std::from_chars(tag.data(), tag.data() + tag.size(), tag_id);

        //scan for the soh delimiter to get the value
        cursor = eq_pos + 1;
        std::size_t soh_pos = raw_frame.find('\x01', cursor);
        if (soh_pos == std::string_view::npos) {
            return false; //malformed cant find delimitier
        }

        const std::string_view value = raw_frame.substr(cursor, soh_pos - cursor);

        switch (tag_id) {

        case 11: // ClientOrderID
            out_fields.client_order_id = value;
            break;
        case 55: // Symbol
            out_fields.symbol = value;
            break;
        case 44: // Price..//TODO rounding errors...read string character by character.
            out_fields.price = parse_double(value);
            break;
        case 38: // Quantity
            out_fields.quantity = parse_uint64(value);
            break;

        case 35: // MsgType
            if (!value.empty()) {
                out_fields.msg_type = static_cast<FixMsgType>(value[0]);
            }
            break;
        case 49: // SenderCompID
            out_fields.sender_comp_id = value;
            break;

        case 56: // TargetCompID
            out_fields.target_comp_id = value;
            break;
        case 54: // Side
            if (!value.empty()) {
                out_fields.side = static_cast<Side>(value[0]);
            }
            break;
        case 34: // MsgSeqNum
            out_fields.msg_seq_num = static_cast<uint32_t>(parse_uint64(value));
            break;
        default:
            break;
        }
        cursor = soh_pos + 1; //advance the cursor to the start of the next tag
    }

    // All messages must have Sender and Target IDs
    if (out_fields.sender_comp_id.empty() || out_fields.target_comp_id.empty()) {
        return false;
    }

    if (out_fields.msg_type == FixMsgType::NewSingleOrder) {
        return !out_fields.symbol.empty() && !out_fields.client_order_id.empty() && out_fields.side != Side::Unknown;
    }

    if (out_fields.msg_type == FixMsgType::Logon || out_fields.msg_type == FixMsgType::Logout) {
        // We already validated the routing IDs above, so these are good to go
        return true;
    }

    return false;
}