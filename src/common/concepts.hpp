#pragma once
#include <concepts>
#include <cstddef>
#include <type_traits>

namespace common {
    // type is compatible with raw byte-casting and copy-free memory
    template <typename T>
    concept StandardLayout = std::is_standard_layout_v<T>;

    // no oop stuff
    template <typename T>
    concept NonPolymorphic = !std::is_polymorphic_v<T> && !std::is_abstract_v<T>;

    // type can be safely copied byte-by-byte (e.g. via std::memcpy)
    template <typename T>
    concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

    template <typename T>
    concept PodType = StandardLayout<T> && TriviallyCopyable<T> && NonPolymorphic<T>;

    //container that provides continuous memory access via data() and size()
    template <typename T>
    concept ContiguousBuffer = requires(T t) {
        { t.data() } -> std::convertible_to<const char*>;
        { t.size() } -> std::convertible_to<std::size_t>;
    };

} // namespace common