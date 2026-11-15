#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <Aeron.h>
#pragma GCC diagnostic pop

#include "data_pipeline.hpp"
#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>

std::atomic<bool> g_running{true};

void sigint_handler(int) {
    g_running = false;
}

int main() {
    std::signal(SIGINT, sigint_handler);

    try {
        std::cout << "Starting Edge Gateway..." << std::endl;

        aeron::Context aCtx;
        std::shared_ptr<aeron::Aeron> aeron_client = aeron::Aeron::connect(aCtx);

        // Publisher:10 (inbound orders going to matching engine)
        const std::int64_t pub_id = aeron_client->addExclusivePublication("aeron:ipc", 10);
        std::shared_ptr<aeron::ExclusivePublication> publication;
        while (!(publication = aeron_client->findExclusivePublication(pub_id))) {
            std::this_thread::yield();
        }
        std::cout << "Aeron publisher established on stream 10 (to matching engine)." << std::endl;

        //Subscriber:20 (outbound executions coming from matching engine)
        const std::int64_t sub_id = aeron_client->addSubscription("aeron:ipc", 20);
        std::shared_ptr<aeron::Subscription> subscription;
        while (!(subscription = aeron_client->findSubscription(sub_id))) {
            std::this_thread::yield();
        }
        std::cout << "Aeron subscriber established on stream 20 (from matching engine)." << std::endl;

        const uint16_t listen_port = 8888;
        DataPipeline pipeline(listen_port);

        if (!pipeline.start()) {
            std::cerr << "Data pipeline failed to start" << std::endl;
            return EXIT_FAILURE;
        }
        std::cout << "Listening for FIX traffic on port " << listen_port << "..." << std::endl;

        // Handler execution reports coming back from the matching engine
        auto fragment_handler = [&](const aeron::concurrent::AtomicBuffer& buffer, aeron::util::index_t offset,
                                    aeron::util::index_t length, [[maybe_unused]] aeron::Header& header) {
            if (length == sizeof(common::SbeExecutionReport)) {
                auto* report = reinterpret_cast<const common::SbeExecutionReport*>(buffer.buffer() + offset);
                // Dispatch the FIX message back to the correct TCP socket
                pipeline.dispatch_execution_report(*report);
                std::cout << "Gateway routed trade for: " << report->client_order_id << "\n";
            }
        };

        aeron::fragment_handler_t aeron_handler = fragment_handler;

        while (g_running) {
            pipeline.poll([&publication](const common::SbeNewOrderSingle& sbe_msg) {
                aeron::concurrent::AtomicBuffer buffer(
                    reinterpret_cast<std::uint8_t*>(const_cast<common::SbeNewOrderSingle*>(&sbe_msg)),
                    sizeof(common::SbeNewOrderSingle));

                std::int64_t result{0};
                while ((result = publication->offer(buffer, 0, sizeof(common::SbeNewOrderSingle))) < 0) {
                    if (result == aeron::NOT_CONNECTED) {
                        break;
                    } else if (result == aeron::BACK_PRESSURED || result == aeron::ADMIN_ACTION) {
                        std::this_thread::yield();
                    } else if (result == aeron::PUBLICATION_CLOSED) {
                        break;
                    }
                }
                if (result > 0) {
                    std::cout << "Published order ID: " << sbe_msg.client_order_id << "\n";
                }
            });

            subscription->poll(aeron_handler, 10);
        }

        pipeline.stop();
        std::cout << "\n Gateway shutting down." << std::endl;

    } catch (const aeron::SourcedException& e) {
        std::cerr << "Aeron exception: " << e.what() << " " << e.where() << std::endl;
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "Standard exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Unknown exception." << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}