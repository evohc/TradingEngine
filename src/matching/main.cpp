#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <Aeron.h>
#pragma GCC diagnostic pop

#include "order_book.hpp"
#include "sbe_messages.hpp"
#include <atomic>
#include <csignal>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>

std::atomic<bool> g_running{true};

void signit_handler(int) {
    g_running = false;
}

struct TransparentStringHash {
    using is_transparent = void; //stop conversion as of C++20

    [[nodiscard]] std::size_t operator()(std::string_view txt) const { return std::hash<std::string_view>{}(txt); }
};

int main() {
    std::signal(SIGINT, signit_handler);

    try {
        std::cout << "Starting matching engine..." << std::endl;

        aeron::Context aCtx;
        std::shared_ptr<aeron::Aeron> aeron_client = aeron::Aeron::connect(aCtx);

        const std::int64_t pub_id = aeron_client->addExclusivePublication("aeron:ipc", 20);
        std::shared_ptr<aeron::ExclusivePublication> publication;
        while (!(publication = aeron_client->findExclusivePublication(pub_id))) {
            std::this_thread::yield();
        }
        std::cout << "Aeron publisher setup on stream 20 (to gateway)." << std::endl;

        const std::int64_t pub_id_mv = aeron_client->addExclusivePublication("aeron:ipc", 30);
        std::shared_ptr<aeron::ExclusivePublication> publication_mv;
        while (!(publication_mv = aeron_client->findExclusivePublication(pub_id_mv))) {
            std::this_thread::yield();
        }
        std::cout << "Aeron publisher setup on stream 30 (to market viewer)." << std::endl;

        const std::int64_t sub_id = aeron_client->addSubscription("aeron:ipc", 10);
        std::shared_ptr<aeron::Subscription> subscription;
        while (!(subscription = aeron_client->findSubscription(sub_id))) {
            std::this_thread::yield();
        }
        std::cout << "Aeron subscriber setup on stream 10 (from gateway)." << std::endl;

        //fired when order matches
        auto on_execution = [&publication](const common::SbeExecutionReport& report) {
            aeron::concurrent::AtomicBuffer buffer(
                reinterpret_cast<std::uint8_t*>(const_cast<common::SbeExecutionReport*>(&report)),
                sizeof(common::SbeExecutionReport));

            std::int64_t result{0};
            while ((result = publication->offer(buffer, 0, sizeof(common::SbeExecutionReport))) < 0) {
                if (result == aeron::NOT_CONNECTED || result == aeron::PUBLICATION_CLOSED) {
                    std::cerr << "Gateway disconnected, dropping execution report.\n";
                    break;
                } else if (result == aeron::BACK_PRESSURED || result == aeron::ADMIN_ACTION) {
                    std::this_thread::yield(); // spin-wait on backpressure
                }
            }
            if (result > 0) {
                std::cout << "Order sent for : " << report.client_order_id << std::endl;
            }
        };

        //fired when symbol updates volume
        auto on_symbol_update = [&publication_mv](const common::SbeMarketDataUpdate& update) {
            aeron::concurrent::AtomicBuffer buffer(
                reinterpret_cast<std::uint8_t*>(const_cast<common::SbeMarketDataUpdate*>(&update)),
                sizeof(common::SbeMarketDataUpdate));

            std::int64_t result{0};
            while ((result = publication_mv->offer(buffer, 0, sizeof(common::SbeMarketDataUpdate))) < 0) {
                if (result == aeron::NOT_CONNECTED || result == aeron::PUBLICATION_CLOSED) {
                    std::cerr << "Market viewer disconnected, dropping symbol update.\n";
                    break;
                } else if (result == aeron::BACK_PRESSURED || result == aeron::ADMIN_ACTION) {
                    std::this_thread::yield(); // spin-wait on backpressure
                }
            }
            if (result > 0) {
                std::cout << "Order update sent for : " << update.symbol << std::endl;
            }
        };

        std::unordered_map<std::string, std::unique_ptr<OrderBook>, TransparentStringHash, std::equal_to<>> order_books;

        order_books["SYM_1"] = std::make_unique<OrderBook>("SYM_1");
        order_books["SYM_2"] = std::make_unique<OrderBook>("SYM_2");
        order_books["SYM_3"] = std::make_unique<OrderBook>("SYM_3");
        order_books["SYM_4"] = std::make_unique<OrderBook>("SYM_4");

        auto fragment_handler = [&](const aeron::concurrent::AtomicBuffer& buffer, aeron::util::index_t offset,
                                    aeron::util::index_t lenght, [[maybe_unused]] aeron::Header& header) {
            if (lenght == sizeof(common::SbeNewOrderSingle)) {

                auto* order = reinterpret_cast<const common::SbeNewOrderSingle*>(buffer.buffer() + offset);

                std::cout << "Recieved order " << order->client_order_id << "\n";

                std::size_t sym_len = strnlen(order->symbol, sizeof(order->symbol));
                std::string_view symbol_sv(order->symbol, sym_len);

                auto it = order_books.find(symbol_sv);
                if (it != order_books.end()) {
                    it->second->process_order(*order, on_execution, on_symbol_update);
                } else {
                    std::cerr << "Unknown symbol: " << symbol_sv << "\n";
                }
            }
        };

        aeron::fragment_handler_t aeron_handler = fragment_handler;

        std::cout << "Matching engine up.\n";

        while (g_running) {
            subscription->poll(aeron_handler, 10);

            // In a production HFT system, we would pin this thread to an isolated CPU core
            // and run a busy-wait loop without sleeping. For this demo, yielding prevents 100% CPU.
            std::this_thread::yield();
        }

        std::cout << "\nMatching Engine shutting down cleanly." << std::endl;
    }

    catch (const aeron::SourcedException& e) {
        std::cerr << "Aeron exception: " << e.what() << " " << e.where() << std::endl;
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "Standard exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Unknown exception." << std::endl;
    }
}