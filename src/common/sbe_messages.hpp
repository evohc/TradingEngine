#pragma once

#include <cstdint>

namespace common {
    enum class SbeSide : uint8_t { Buy = 1, Sell = 2, Unknown = 0 };

    enum class SbeExecType : uint8_t { New = 0, PartialFill = 1, Fill = 2, Rejected = 3 };

    enum class MessageType : uint8_t { NewOrderSingle = 101, OrderCancelRequest = 102, ExecutionReport = 201 };

    struct alignas(8) SbeExecutionReport {
        char client_order_id[32]; // Tag 11: original order ID
        char sender_comp_id[32];  // Tag 49: routing id for the gateway
        uint64_t exec_id;         // Tag 17: unique ID for this specific match
        char symbol[16];          // Tag 55: symbol
        SbeSide side;             // Tag 54: 1=Buy, 2=Sell
        SbeExecType exec_type;    // Tag 150: partialFill or Fill
        uint8_t padding[6];       // Explicit padding to keep 8-byte alignment
        int64_t match_price;      // Tag 31 (LastPx): Price this trade executed at
        uint32_t filled_quantity; // Tag 32 (LastQty): Shares matched in this exact trade
        uint32_t leaves_quantity; // Tag 151 (LeavesQty): Shares remaining on the order
    }; //padded to 128 bytes 2 cache lines...two CPU clock cycles, wonder is this right
    static_assert(sizeof(SbeExecutionReport) == 112, "SbeExecutionReport must be exactly 112 bytes");

    //standard SBE message header
    struct alignas(8) SbeHeader {
        uint16_t block_length;
        uint16_t template_id;
        uint16_t schema_id;
        uint16_t version;
    };

    struct alignas(64) SbeNewOrderSingle {
        SbeHeader header;         // 8 Bytes (0-7)
        uint64_t msg_seq_num;     // 8 Bytes (8-15)
        char client_order_id[20]; // 20 Bytes (16-35)
        char symbol[12];          // 12 Bytes (36-47)
        char sender_comp_id[32];  // 32 Bytes (48-79)
        int64_t price_mantissa;   // 8 Bytes (80-87)
        uint32_t quantity;        // 4 Bytes (88-91)
        SbeSide side;             // 1 Byte  (92)
        char padding[3];          // 3 Bytes (93-95)
    };
    // Because alignas(64) pads the 96 bytes up to the nearest multiple of 64...
    static_assert(sizeof(SbeNewOrderSingle) == 128, "SbeNewOrderSingle must be 128 bytes (2 cache lines)");

    enum class SbeMarketDataAction : uint8_t { Add = 1, Update = 2, Delete = 3 };

    struct alignas(32) SbeMarketDataUpdate {
        common::SbeHeader header;
        char symbol[16];
        common::SbeSide side;
        SbeMarketDataAction action;
        int64_t price_mantissa;
        uint32_t price_level_volume;
    };

    static_assert(sizeof(SbeMarketDataUpdate) == 64, "SbeMarketDataUpdate must be 64 bytes (1 cache line)");

} // namespace common