#pragma once

#include "../common/sbe_messages.hpp"
#include "fix_parser.hpp"
#include "sbe_encoder.hpp"
#include "session_mgr.hpp"
#include "tcp_receiver.hpp"
#include <cstdint>
#include <functional>
#include <string_view>

class DataPipeline {
public:
    explicit DataPipeline(uint16_t port) : m_receiver(port) {}

    ~DataPipeline() = default;

    DataPipeline(const DataPipeline&) = delete;
    DataPipeline& operator=(const DataPipeline&) = delete;

    [[nodiscard]] bool start() noexcept { return m_receiver.start(); }

    void stop() noexcept { m_receiver.stop(); }

    template <typename SbeDispatchCallback>
    void poll(SbeDispatchCallback&& on_sbe_encoded) noexcept {
        m_receiver.poll([this, &on_sbe_encoded](int fd, std::string_view raw_fix_frame) {
            process_frame(fd, raw_fix_frame, on_sbe_encoded);
        });
    }

    void dispatch_execution_report(const common::SbeExecutionReport& report) noexcept {
        size_t id_len = strnlen(report.sender_comp_id, sizeof(report.sender_comp_id));
        int fd = m_session_mgr.get_client_fd(std::string_view(report.sender_comp_id, id_len));

        if (fd >= 0) {
            // Format a simple FIX ExecutionReport (MsgType=8)
            // TODO dont use snprintf...
            int fix_side = (report.side == common::SbeSide::Buy) ? 1 : 2;
            char buffer[256];
            int len = snprintf(
                buffer, sizeof(buffer),
                "8=FIX.4.4|9=000|35=8|49=TradingExchangeDemo|56=%s|11=%s|55=%s|54=%d|31=%ld|32=%u|150=%d|10=000|",
                report.sender_comp_id, report.client_order_id, report.symbol, fix_side, report.match_price,
                report.filled_quantity, static_cast<int>(report.exec_type));

            // Replace pipes with SOH (0x01) for actual FIX compliance
            for (int i = 0; i < len; ++i) {
                if (buffer[i] == '|')
                    buffer[i] = '\x01';
            }

            // back to the client socket..check for error
            ::send(fd, buffer, len, MSG_NOSIGNAL | MSG_DONTWAIT);
        } else {
        }
    }

private:
    template <typename SbeDispatchCallback>
    void process_frame(int fd, std::string_view raw_frame, SbeDispatchCallback&& on_sbe_encoded) noexcept {
        //parse raw string from network
        ParsedFixFields fields{};

        if (!parse_fix_message(raw_frame, fields)) {
            return;
        }

        if (fields.target_comp_id != "TradingExchangeDemo") {
            return;
        }

        const std::string_view session_id = fields.sender_comp_id.to_string_view();

        switch (fields.msg_type) {
        case FixMsgType::Logon: {
            m_session_mgr.handle_logon(session_id, fd, fields.msg_seq_num, false); //add Tag 141 (ResetSeqNumFlag)
            break;
        }
        case FixMsgType::Logout: {
            m_session_mgr.handle_logout(session_id);
            break;
        }
        case FixMsgType::NewSingleOrder: {
            if (!m_session_mgr.is_active(session_id)) {
                break;
            }
            if (!m_session_mgr.validate_message(session_id, fields.msg_seq_num)) {
                return; //Out-of-sequence message or gap
            }
            m_session_mgr.increment_expected_seq(session_id);

            common::SbeNewOrderSingle sbe_msg = SbeEncoder::encode_new_order(fields);
            on_sbe_encoded(sbe_msg);
            break;
        }
        case FixMsgType::Unknown:
            break;
        }
    }

    TcpReceiver m_receiver;
    SessionManager m_session_mgr;
};

// namespace ingestion