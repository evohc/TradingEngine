#pragma once

#include "fix_parser.hpp"
#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

// standard Tcp receiver using epoll...however certain prod scenaios would use kernel bypass
// single threaded approach to handling multiple clients.

struct ClientBuffer {
    alignas(64) char rx_buffer[8192]{};
    std::size_t rx_write_pos{0};
};

class TcpReceiver {
public:
    explicit TcpReceiver(uint16_t port) : m_port(port) {};
    ~TcpReceiver() { stop(); }

    TcpReceiver(const TcpReceiver&) = delete;
    TcpReceiver& operator=(const TcpReceiver&) = delete;

    [[nodiscard]] bool start() noexcept {
        m_server_fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

        if (m_server_fd < 0) {
            return false;
        }

        //standard low latency options
        const int opt = 1;
        ::setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        ::setsockopt(m_server_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(m_port);

        if (::bind(m_server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address))) {
            return false;
        };

        if (::listen(m_server_fd, 128) < 0) {
            return false;
        }

        m_epoll_fd = ::epoll_create1(0);
        if (m_epoll_fd < 0) {
            return false;
        }

        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = m_server_fd;
        if (::epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_server_fd, &ev) < 0) {
            return false;
        }

        m_running = true;
        return true;
    }

    void stop() {
        if (!m_running) {
            return;
        }

        m_running = false;

        for (auto& [fd, buf] : m_client_buffers) {
            ::close(fd);
        }
        m_client_buffers.clear();

        if (m_server_fd >= 0)
            ::close(m_server_fd);

        if (m_epoll_fd >= 0)
            ::close(m_epoll_fd);
    }

    template <typename FrameCallback>
    bool poll(FrameCallback&& on_frame_received) noexcept {
        if (!m_running) {
            return false;
        }

        epoll_event events[64];

        // Non-blocking wait (0 timeout)
        int nfds = ::epoll_wait(m_epoll_fd, events, 64, 0);
        if (nfds < 0) {
            if (errno == EINTR) {
                return true; // Interrupted by OS signal, safe to continue loop
            }
            return false; // Real socket error
        }

        for (auto i = 0; i < nfds; ++i) {
            const auto fd = events[i].data.fd; //what client
            uint32_t returned_events = events[i].events;

            // A new client wants to connect
            // drains the queue of incoming connection requests and registers them with epoll for future event tracking.
            if (fd == m_server_fd) {
                while (true) {
                    sockaddr_in client_addr{};
                    socklen_t client_len = sizeof(client_addr);

                    auto client_fd =
                        ::accept4(m_server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len, SOCK_NONBLOCK);

                    if (client_fd < 0) {
                        break;
                    }

                    auto opt = 1;
                    ::setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

                    epoll_event ev{};
                    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
                    ev.data.fd = client_fd;

                    if (::epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == 0) {
                        m_client_buffers[client_fd] = ClientBuffer{};
                        read_client_bytes(client_fd, on_frame_received);
                    } else {
                        ::close(client_fd);
                    }
                }
            }
            //existing connected client
            else {
                // Read bytes FIRST (even if the client initiated a disconnect)
                if (returned_events & (EPOLLIN | EPOLLRDHUP | EPOLLHUP)) {
                    read_client_bytes(fd, on_frame_received);
                }

                if (returned_events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
                    ::epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                    ::close(fd);
                    m_client_buffers.erase(fd);
                }
            }
        }

        return true;
    }

private:
    /* keep writing into a buffer, examining it for a full frame. If we find a full self contained FIX message 
       send and reset buffer, when spills over (i.e. we have a full frame and partial frame) we move the partial 
       frame to the start of the buffer */
    template <typename FrameCallback>
    void extract_fix_frames(int fd, ClientBuffer& client, FrameCallback&& on_frame_received) noexcept {
        std::string_view stream(client.rx_buffer, client.rx_write_pos);
        std::size_t cursor = 0;

        while (cursor < stream.size()) {
            std::size_t checksum_pos = stream.find("10=", cursor);
            if (checksum_pos == std::string_view::npos) {
                break;
            }

            std::size_t soh_pos = stream.find('\x01', checksum_pos);
            if (soh_pos == std::string_view::npos) {
                break;
            }

            std::size_t frame_len = (soh_pos + 1) - cursor;
            std::string_view fix_frame = stream.substr(cursor, frame_len);

            on_frame_received(fd, fix_frame);

            cursor = soh_pos + 1;
        }

        if (cursor > 0) {
            std::size_t remaining = client.rx_write_pos - cursor;
            if (remaining > 0) {
                std::memmove(client.rx_buffer, client.rx_buffer + cursor, remaining);
            }
            client.rx_write_pos = remaining;
        }
    }
    template <typename FrameCallback>
    void read_client_bytes(int fd, FrameCallback&& on_frame_received) noexcept {
        const auto it = m_client_buffers.find(fd);
        if (it == m_client_buffers.end()) {
            return;
        }

        ClientBuffer& client = it->second;

        while (true) {
            const ssize_t bytes_read = ::recv(fd, client.rx_buffer + client.rx_write_pos,
                                              sizeof(client.rx_buffer) - client.rx_write_pos, MSG_DONTWAIT);

            if (bytes_read > 0) {
                client.rx_write_pos += bytes_read;
                extract_fix_frames(fd, client, on_frame_received);
            } else {
                break;
            }
        }
    }

    uint16_t m_port{0};
    int m_server_fd{-1};
    int m_epoll_fd{-1};
    bool m_running{false};
    //TODO use std::array!
    std::unordered_map<int, ClientBuffer> m_client_buffers; //multiple clients
};
