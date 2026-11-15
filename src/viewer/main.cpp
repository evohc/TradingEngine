#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <Aeron.h>
#pragma GCC diagnostic pop

#include "sbe_messages.hpp"
#include <atomic>
#include <csignal>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <thread>

std::atomic<bool> g_running{true};

void signit_handler(int) {
    g_running = false;
}

struct ShadowBook {
    std::map<int64_t, uint32_t, std::greater<int64_t>> bids; //highest first
    std::map<int64_t, uint32_t, std::less<int64_t>> asks;    //lowest first
};

std::unordered_map<std::string, ShadowBook> g_market_state;
std::string g_active_symbol = "SYM_1";

// if there was a busrt of order, cout can impact draining the aeron buffer and cause back pressure...refactor
void draw_dashboard() {
    std::cout << "\033[H"; //moves cursor to top-left (no flicker)

    for (const auto& [symbol, book] : g_market_state) {
        std::cout << "\033[1;36mMarket Data : " << symbol << "\033[0m\033[K\n";
        std::cout << "   Bids (qty @ price) |   Asks (price @ qty)      \033[K\n";
        std::cout << "---------------------------------------------\033[K\n";

        auto bid_it = book.bids.begin();
        auto ask_it = book.asks.begin();

        for (int i = 0; i < 5; ++i) { //display only five rows here.
            // draw bids (green)
            if (bid_it != book.bids.end()) {
                double price = bid_it->first / 10000.0;
                std::ostringstream ss;
                ss << bid_it->second << " @  " << std::fixed << std::setprecision(2) << price;

                std::cout << "\033[32m   " << std::left << std::setw(19) << ss.str() << "\033[0m|";
                ++bid_it;
            } else {
                std::cout << std::setw(22) << " " << "|";
            }

            // draw asks (red)
            if (ask_it != book.asks.end()) {
                double price = ask_it->first / 10000.0;
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(2) << price << " @ " << ask_it->second;

                std::cout << "\033[31m   " << std::left << std::setw(19) << ss.str() << "\033[0m";
                ++ask_it;
            } else {
                std::cout << std::setw(22) << " ";
            }
            std::cout << "\033[K\n";
        }
        std::cout << "\033[K\n";
    }

    std::cout << "\033[J";
}

int main() {
    std::signal(SIGINT, signit_handler);

    std::cout << "\033[2J";

    try {
        std::cout << "Starting market viewer engine..." << std::endl;

        aeron::Context aCtx;
        std::shared_ptr<aeron::Aeron> aeron_client = aeron::Aeron::connect(aCtx);

        const std::int64_t sub_id = aeron_client->addSubscription("aeron:ipc", 30);
        std::shared_ptr<aeron::Subscription> subscription;
        while (!(subscription = aeron_client->findSubscription(sub_id))) {
            std::this_thread::yield();
        }
        std::cout << "Aeron subscriber setup on stream 30 (from matching engine)." << std::endl;

        auto fragment_handler = [&](const aeron::concurrent::AtomicBuffer& buffer, aeron::util::index_t offset,
                                    aeron::util::index_t lenght, [[maybe_unused]] aeron::Header& header) {
            if (lenght == sizeof(common::SbeMarketDataUpdate)) {

                auto* update = reinterpret_cast<const common::SbeMarketDataUpdate*>(buffer.buffer() + offset);

                std::string symbol{update->symbol};

                auto& book = g_market_state[symbol];

                if (update->side == common::SbeSide::Buy) {
                    if (update->action == common::SbeMarketDataAction::Delete || update->price_level_volume == 0) {
                        book.bids.erase(update->price_mantissa);
                    } else {
                        book.bids[update->price_mantissa] = update->price_level_volume;
                    }
                } else {
                    if (update->action == common::SbeMarketDataAction::Delete || update->price_level_volume == 0) {
                        book.asks.erase(update->price_mantissa);
                    } else {
                        book.asks[update->price_mantissa] = update->price_level_volume;
                    }
                }

                draw_dashboard();
            }
        };

        aeron::fragment_handler_t aeron_handler = fragment_handler;

        std::cout << "Matching engine up.\n";

        while (g_running) {
            subscription->poll(aeron_handler, 30);
            std::this_thread::yield();
        }

        std::cout << "\033[0m\nnMarket viewer shutting down cleanly.\n";

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