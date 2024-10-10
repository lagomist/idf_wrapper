#pragma once

#include <vector>
#include <string>
#include <tuple>

namespace type_traits2 {

template<template <typename ... arg_t> typename container_t, typename first_t, typename ... rest_t>
struct is_container : std::false_type {};

template<template <typename ... arg_t> typename container_t, typename first_t, typename ... rest_t>
struct is_container<container_t, container_t<first_t, rest_t ...>> : std::true_type {};

template<typename first_t, typename ... rest_t>
using is_tuple = is_container<std::tuple, first_t, rest_t ...>;

template<typename first_t, typename ... rest_t>
using is_pair = is_container<std::pair, first_t, rest_t ...>;

template<typename first_t, typename ... rest_t>
using is_string = is_container<std::basic_string, first_t, rest_t ...>;

template<typename first_t, typename ... rest_t>
using is_string_view = is_container<std::basic_string_view, first_t, rest_t ...>;

template<typename first_t, typename ... rest_t>
using is_vector = is_container<std::vector, first_t, rest_t ...>;

template <typename T>
struct is_array : std::false_type {};

template <typename T, size_t N>
struct is_array<std::array<T, N>> : std::true_type { constexpr static size_t Nm = N; };

} // namespace type_traits2
