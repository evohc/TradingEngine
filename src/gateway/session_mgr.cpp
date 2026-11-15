#include "session_mgr.hpp"

bool SessionManager::handle_logon(std::string_view session_id, int client_fd, uint64_t msg_seq_num,
                                  bool reset_seq_flag) noexcept {

    // We must construct the FixedString here because operator[] might need to insert a new key
    const common::FixedString<32> key(session_id);
    auto& state = m_sessions[key];

    if (state.client_id.empty()) {
        state.client_id = key;
    }

    if (reset_seq_flag) {
        state.next_expected_seq_num = 1;
    }

    if (msg_seq_num < state.next_expected_seq_num) {
        return false;
    }

    state.status = SessionStatus::LoggedIn;
    state.next_expected_seq_num = msg_seq_num + 1;
    state.client_fd = client_fd;

    return true;
}

void SessionManager::handle_logout(std::string_view session_id) noexcept {
    // ZERO ALLOCATION LOOKUP: pass string_view directly!
    const auto it = m_sessions.find(session_id);
    if (it != m_sessions.end()) {
        it->second.status = SessionStatus::LoggedOut;
    }
}

bool SessionManager::validate_message(std::string_view session_id, uint64_t msg_seq_num) noexcept {
    const auto it = m_sessions.find(session_id);

    if (it == m_sessions.end() || it->second.status != SessionStatus::LoggedIn) {
        return false;
    }

    return (msg_seq_num == it->second.next_expected_seq_num);
}

void SessionManager::increment_expected_seq(std::string_view session_id) noexcept {
    const auto it = m_sessions.find(session_id);
    if (it != m_sessions.end()) {
        it->second.next_expected_seq_num++;
    }
}

uint64_t SessionManager::get_expected_seq(std::string_view session_id) noexcept {
    const auto it = m_sessions.find(session_id);
    return (it != m_sessions.end()) ? it->second.next_expected_seq_num : 1;
}

bool SessionManager::is_active(std::string_view session_id) noexcept {
    const auto it = m_sessions.find(session_id);
    return (it != m_sessions.end() && it->second.status == SessionStatus::LoggedIn);
}

int SessionManager::get_client_fd(std::string_view session_id) noexcept {
    const auto it = m_sessions.find(session_id);
    if (it != m_sessions.end() && it->second.status == SessionStatus::LoggedIn) {
        return it->second.client_fd;
    }
    return -1;
}

void SessionManager::clear() noexcept {
    m_sessions.clear();
}