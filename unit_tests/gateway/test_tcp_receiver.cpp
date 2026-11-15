#include "../../src/gateway/fix_parser.hpp"
#include "../../src/gateway/tcp_receiver.hpp"
#include "../release_assert.hpp"
#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

    const std::string FIX_MSG_BUY = "8=FIX.4.4\x01"    // BeginString
                                    "9=105\x01"        // Length
                                    "35=D\x01"         // MsgType
                                    "34=101\x01"       // MsgSeqNum
                                    "49=CLIENT_A\x01"  // SenderCompID
                                    "56=EXCHANGE\x01"  // TargetCompID
                                    "11=ORD999888\x01" // ClOrdID
                                    "55=SYM_1\x01"     // Symbol
                                    "54=1\x01"         // Side
                                    "38=100\x01"       // quantity
                                    "44=65000.50\x01"  // Price
                                    "10=128\x01";      // CheckSum

    const std::string FIX_MSG_SELL = "8=FIX.4.4\x01"
                                     "9=105\x01"
                                     "35=D\x01"
                                     "34=102\x01"
                                     "49=CLIENT_B\x01"
                                     "56=EXCHANGE\x01"
                                     "11=ORD777666\x01"
                                     "55=SYM_2\x01"
                                     "54=2\x01"
                                     "38=50\x01"
                                     "44=3450.75\x01"
                                     "10=210\x01";

    int connect_test_client(const uint16_t port) {
        const int sock = ::socket(AF_INET, SOCK_STREAM, 0);
        TEST_ASSERT(sock > 0);

        sockaddr_in serv_addr{};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        ::inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

        int res = ::connect(sock, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr));
        TEST_ASSERT(res == 0);
        return sock;
    }
    void test_single_complete_frame_delivery() {
        const uint16_t port = 8793;
        TcpReceiver receiver(port);

        TEST_ASSERT(receiver.start());

        std::vector<std::string> received_frames;

        std::thread client([port]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            int sock = connect_test_client(port);
            ::send(sock, FIX_MSG_BUY.data(), FIX_MSG_BUY.size(), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            ::close(sock);
        });

        for (int i = 0; i < 20; ++i) {
            receiver.poll(
                [&]([[maybe_unused]] int client_fd, std::string_view frame) { received_frames.emplace_back(frame); });
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        client.join();
        receiver.stop();

        TEST_ASSERT(received_frames.size() == 1);
        TEST_ASSERT(received_frames[0] == FIX_MSG_BUY);
    }

    void test_spliced_frame_delivery() {
        const uint16_t port = 8793;
        TcpReceiver receiver(port);

        TEST_ASSERT(receiver.start());

        std::vector<std::string> received_frames;

        std::thread client([port]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            int sock = connect_test_client(port);
            std::string combined = FIX_MSG_BUY + FIX_MSG_SELL;
            ::send(sock, combined.data(), combined.size(), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            ::close(sock);
        });

        for (int i = 0; i < 20; ++i) {
            receiver.poll(
                [&]([[maybe_unused]] int client_fd, std::string_view frame) { received_frames.emplace_back(frame); });
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        client.join();
        receiver.stop();

        TEST_ASSERT(received_frames.size() == 2);
        TEST_ASSERT(received_frames[0] == FIX_MSG_BUY);
        TEST_ASSERT(received_frames[1] == FIX_MSG_SELL);
    }

    void test_fragmented_frame_delivery() {
        const uint16_t port = 8793;
        TcpReceiver receiver(port);

        TEST_ASSERT(receiver.start());

        std::vector<std::string> received_frames;

        std::thread client([port]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            int sock = connect_test_client(port);

            std::string chunk1 = FIX_MSG_BUY.substr(0, FIX_MSG_BUY.size() / 2);
            std::string chunk2 = FIX_MSG_BUY.substr(FIX_MSG_BUY.size() / 2);

            ::send(sock, chunk1.data(), chunk1.size(), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));

            ::send(sock, chunk2.data(), chunk2.size(), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            ::close(sock);
        });

        for (int i = 0; i < 30; ++i) {
            receiver.poll(
                [&]([[maybe_unused]] int client_fd, std::string_view frame) { received_frames.emplace_back(frame); });
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        client.join();
        receiver.stop();

        TEST_ASSERT(received_frames.size() == 1);
        TEST_ASSERT(received_frames[0] == FIX_MSG_BUY);
    }

    void test_multi_client_isolation() {
        const uint16_t port = 8797;
        TcpReceiver receiver(port);
        TEST_ASSERT(receiver.start());

        std::vector<std::string> received_frames;

        std::thread client_A([port]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            int sock = connect_test_client(port);
            ::send(sock, FIX_MSG_BUY.data(), FIX_MSG_BUY.size(), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            ::close(sock);
        });

        std::thread client_B([port]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            int sock = connect_test_client(port);
            ::send(sock, FIX_MSG_SELL.data(), FIX_MSG_SELL.size(), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            ::close(sock);
        });

        for (int i = 0; i < 30; ++i) {
            receiver.poll(
                [&]([[maybe_unused]] int client_fd, std::string_view frame) { received_frames.emplace_back(frame); });
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        client_A.join();
        client_B.join();
        receiver.stop();

        TEST_ASSERT(received_frames.size() == 2);
        bool found_buy = false, found_sell = false;
        for (const auto& f : received_frames) {
            if (f == FIX_MSG_BUY)
                found_buy = true;
            if (f == FIX_MSG_SELL)
                found_sell = true;
        }
        TEST_ASSERT(found_buy && found_sell);
    }

} // namespace

void run_tcp_receiver_unit_tests() {
    test_single_complete_frame_delivery();
    test_spliced_frame_delivery();
    test_fragmented_frame_delivery();
    test_multi_client_isolation();
}