#include "../../src/gateway/data_pipeline.hpp"
#include "../release_assert.hpp"
#include <cassert>
#include <string_view>

using namespace common;

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

    const std::string FIX_MSG_LOGON = "8=FIX.4.4\x01"
                                      "9=65\x01"
                                      "35=A\x01"
                                      "34=1\x01"
                                      "49=CLIENT_A\x01"
                                      "56=TradingExchangeDemo\x01"
                                      "10=128\x01";

    const std::string FIX_MSG_BUY = "8=FIX.4.4\x01"              // BeginString
                                    "9=105\x01"                  // Length
                                    "35=D\x01"                   // MsgType
                                    "34=2\x01"                   // MsgSeqNum
                                    "49=CLIENT_A\x01"            // SenderCompID
                                    "56=TradingExchangeDemo\x01" // TargetCompID
                                    "11=ORD999888\x01"           // ClOrdID
                                    "55=SYM_1\x01"               // Symbol
                                    "54=1\x01"                   // Side
                                    "38=100\x01"                 // quantity
                                    "44=65000.50\x01"            // Price
                                    "10=128\x01";                // CheckSum

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
    void test_complete_pipeline() {
        const uint16_t port = 8793;
        DataPipeline pipeline(port);

        TEST_ASSERT(pipeline.start());

        std::vector<common::SbeNewOrderSingle> received_messages;

        std::thread client([port]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            int sock = connect_test_client(port);

            ::send(sock, FIX_MSG_LOGON.data(), FIX_MSG_LOGON.size(), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));

            ::send(sock, FIX_MSG_BUY.data(), FIX_MSG_BUY.size(), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            ::close(sock);
        });

        for (int i = 0; i < 20; ++i) {
            pipeline.poll([&](const common::SbeNewOrderSingle& msg) { received_messages.push_back(msg); });
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        client.join();
        pipeline.stop();

        TEST_ASSERT(received_messages.size() == 1);

        const auto& sbe_msg = received_messages[0];

        TEST_ASSERT(sbe_msg.header.template_id == static_cast<uint16_t>(common::MessageType::NewOrderSingle));
        TEST_ASSERT(std::string_view(sbe_msg.client_order_id, 9) == "ORD999888");
        TEST_ASSERT(std::string_view(sbe_msg.symbol, 5) == "SYM_1");
        TEST_ASSERT(sbe_msg.quantity == 100);
        TEST_ASSERT(sbe_msg.price_mantissa == 650005000); // 65000.50 * 10000
        TEST_ASSERT(sbe_msg.side == common::SbeSide::Buy);
    }

} // namespace

void run_data_pipeline_unit_tests() {
    test_complete_pipeline();
}