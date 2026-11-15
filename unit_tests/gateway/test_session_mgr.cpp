#include "../../src/gateway/session_mgr.hpp"
#include "../release_assert.hpp"
#include <cassert>

namespace {
    void test_successful_logon() {
        SessionManager mgr;

        const bool success = mgr.handle_logon("client_1", 1, 1, false);
        TEST_ASSERT(success == true);
        TEST_ASSERT(mgr.is_active("client_1") == true);
        TEST_ASSERT(mgr.get_expected_seq("client_1") == 2);

        bool valid = mgr.validate_message("client_1", 2);
        TEST_ASSERT(valid == true);

        mgr.increment_expected_seq("client_1");
        TEST_ASSERT(mgr.get_expected_seq("client_1") == 3);
    }

    void test_replay_rejection() {
        SessionManager mgr;

        //client logs on successfully at sequence 10
        bool success = mgr.handle_logon("client_1", 1, 10, false);
        TEST_ASSERT(success == true);
        TEST_ASSERT(mgr.get_expected_seq("client_1") == 11);

        //client disconnects/reconnects, but sends invalid sequence no
        success = mgr.handle_logon("client_1", 1, 2, false);
        TEST_ASSERT(success == false);
    }

    void test_sequence_gap() {
        SessionManager mgr;

        mgr.handle_logon("client_1", 1, 1, false);
        mgr.increment_expected_seq("client_1");

        const bool valid = mgr.validate_message("CLIENT_C", 4);
        TEST_ASSERT(valid == false);
    }

    void test_forced_reset() {
        SessionManager mgr;

        mgr.handle_logon("client_1", 1, 1, false);
        mgr.increment_expected_seq("client_1"); // expected is now 2

        const bool success = mgr.handle_logon("client_1", 1, 1, true);
        TEST_ASSERT(success == true);
        TEST_ASSERT(mgr.get_expected_seq("client_1") == 2);
    }

    void test_logout_handling() {
        SessionManager mgr;

        mgr.handle_logon("client_1", 1, 1, false);
        TEST_ASSERT(mgr.is_active("client_1") == true);

        mgr.handle_logout("client_1");
        TEST_ASSERT(mgr.is_active("client_1") == false);

        bool valid = mgr.validate_message("client_1", 1);
        TEST_ASSERT(valid == false);
    }
} // namespace

void run_session_manager_unit_tests() {
    test_successful_logon();
    test_replay_rejection();
    test_sequence_gap();
    test_forced_reset();
    test_logout_handling();
}