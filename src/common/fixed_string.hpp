#pragma once

#include "concepts.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <string_view>

namespace common {

    template <std::size_t Capacity>
    class FixedString {
        static_assert(Capacity > 0, "Fixed string capacity must be greater than 0");

    public:
        constexpr FixedString() noexcept = default;

        constexpr FixedString(std::string_view str) noexcept {
            const std::size_t copy_len = std::min(str.size(), Capacity);
            std::copy_n(str.data(), copy_len, m_data.begin());
            if (copy_len < Capacity) {
                m_data[copy_len] = '\0';
            }
            m_length = copy_len;
        }

        // modern C++ should mark all "getter" functions as nodiscard, enforce caller usage of data...
        [[nodiscard]] constexpr const char* data() const noexcept { return m_data.data(); }
        [[nodiscard]] constexpr char* data() noexcept { return m_data.data(); }
        [[nodiscard]] constexpr std::size_t size() const noexcept { return m_length; }
        [[nodiscard]] constexpr bool empty() const noexcept { return m_length == 0; }
        [[nodiscard]] constexpr std::size_t capacity() const noexcept { return Capacity; }

        [[nodiscard]] constexpr std::string_view to_string_view() const noexcept {
            return std::string_view(m_data.data(), m_length);
        }

        [[nodiscard]] constexpr bool operator==(const FixedString& other) const noexcept {
            if (m_length != other.m_length) {
                return false;
            }

            return std::equal(m_data.begin(), m_data.begin() + m_length, other.m_data.begin());
        }

        [[nodiscard]] constexpr bool operator==(std::string_view str) const noexcept { return to_string_view() == str; }

        constexpr void clear() noexcept {
            m_length = 0;
            m_data[0] = '\0';
        }

        constexpr FixedString& operator=(std::string_view sv) noexcept {
            const std::size_t copy_len = (sv.size() < Capacity) ? sv.size() : Capacity;
            std::copy_n(sv.data(), copy_len, m_data.begin());

            m_length = copy_len;
            if (m_length < Capacity) {
                m_data[m_length] = '\0';
            }
            return *this;
        }

        constexpr FixedString& operator=(const char* str) noexcept { return operator=(std::string_view(str)); }

    private:
        std::array<char, Capacity> m_data{};
        std::size_t m_length{0};
    };

    static_assert(PodType<FixedString<32>>,
                  "FixedString layout must be trivially copyable and standard layout...plain old data");
    static_assert(ContiguousBuffer<FixedString<32>>, "FixedString must conform to the ContiguousBuffer concept.");

} // namespace common
