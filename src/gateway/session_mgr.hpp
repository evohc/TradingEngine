
/*
Session manager needs to handle FIX MsgSeqNum - Aeron does not handle applicaton layer data. It
handles UDP/IPC packet sequencing, network drops, retransmissions etc. If a UDP packet drops on 
the network, Aeron detects it and requests a retransmit (at the UDP/IP level) so 
the subscriber gets a nice clean stream.

If a client's TCP connection drops and they reconnect 10 minutes later, Aeron treats this 
as a brand new transport stream starting at zero. However, the client's FIX session must
resume exactly where it left off (e.g., expecting sequence 457 because 456 was 
the last order sent before the disconnect). This is what the session manager does.

//why are we only tracking these 3 only...administrative FIX messages (Heartbeats, ResendRequests) are omitted for demo scope and brevity
Logon (MsgType = A): Establishes or restores the session, checks credentials/API keys, and handles sequence number resets.
Logout (MsgType = 5): Orderly tear-down of the session.
NewOrderSingle (MsgType = D): Validate the incoming sequence number (MsgSeqNum) against our expected tracker.
*/

#pragma once

#include "fixed_string.hpp"
#include <cstdint>
#include <string_view>
#include <unordered_map>

struct FixedStringHasher {
    using is_transparent = void;
    //C++20 flag. For std::unordered_map, hasher is flexible. It allows us to look up keys using different type

    template <std::size_t N>
    std::size_t operator()(const common::FixedString<N>& fs) const noexcept {
        return std::hash<std::string_view>{}(fs.to_string_view());
    }

    std::size_t operator()(std::string_view sv) const noexcept { return std::hash<std::string_view>{}(sv); }
};

enum class SessionStatus : uint8_t { LoggedOut, LoggedIn, SequenceResetPending };

struct alignas(64) SessionState {
    common::FixedString<32> client_id;
    uint64_t next_expected_seq_num{1};
    SessionStatus status{SessionStatus::LoggedOut};
    int client_fd;
};

class SessionManager {
public:
    SessionManager() noexcept = default;

    //Returns true if session is authenticated, false if rejected (e.g. sequence number is too low)
    //force_reset' is set to true if the client requested a clean sequence restart.
    bool handle_logon(std::string_view session_id, int client_fd, uint64_t msg_seq_num, bool reset_seq_flag) noexcept;

    void handle_logout(std::string_view session_id) noexcept;

    //check sequence number  and the session is active
    [[nodiscard]] bool validate_message(std::string_view session_id, uint64_t msg_seq_num) noexcept;

    // Increment sequence no after a message has been  processed.
    void increment_expected_seq(std::string_view session_id) noexcept;

    [[nodiscard]] uint64_t get_expected_seq(std::string_view session_id) noexcept;
    [[nodiscard]] bool is_active(std::string_view session_id) noexcept;
    [[nodiscard]] int get_client_fd(std::string_view session_id) noexcept;

    void clear() noexcept;

private:
    std::unordered_map<common::FixedString<32>, SessionState, FixedStringHasher, std::equal_to<>> m_sessions;
    //std::equal_to<> uses C++'s operator== that compares FixedString with std::string_view directly (avoiding tempoary construction)
};
