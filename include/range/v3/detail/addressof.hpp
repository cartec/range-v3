/// \file
// Range v3 library
//
//  Copyright Casey Carter 2018
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3
//
#ifndef RANGES_V3_DETAIL_ADDRESSOF_HPP
#define RANGES_V3_DETAIL_ADDRESSOF_HPP

#include <memory>
#include <meta/meta.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/utility/concepts.hpp>

namespace ranges
{
    inline namespace v3
    {
        namespace detail
        {
#if defined(__cpp_lib_addressof_constexpr) && __cpp_lib_addressof_constexpr >= 201603

            using std::addressof;

#else
#if defined(__clang__)
#define RANGES_HAS_BUILTIN_ADDRESSOF __has_builtin(__builtin_addressof)
#elif defined(__GNUC__) && __GNUC__ >= 7
#define RANGES_HAS_BUILTIN_ADDRESSOF 1
#else
#define RANGES_HAS_BUILTIN_ADDRESSOF 0
#endif

#if RANGES_HAS_BUILTIN_ADDRESSOF
            template<typename T>
            constexpr T *addressof(T &t) noexcept
            {
                return __builtin_addressof(t);
            }

#else // RANGES_HAS_BUILTIN_ADDRESSOF
            struct MemberAddress
            {
                template<typename T>
                auto requires_(T &&t) -> decltype(concepts::valid_expr(
                    ((static_cast<T &&>(t)).operator&(), 42)
                ));
            };

            struct NonMemberAddress
            {
                template<typename T>
                auto requires_(T &&t) -> decltype(concepts::valid_expr(
                    (operator&(static_cast<T &&>(t)), 42)
                ));
            };

            template<typename T>
            using UserDefinedAddressOf = meta::or_<
                concepts::models<MemberAddress, T>,
                concepts::models<NonMemberAddress, T>>;

            template<typename T,
                CONCEPT_REQUIRES_(UserDefinedAddressOf<T>())>
            T *addressof(T &t) noexcept
            {
                return std::addressof(t);
            }

            template<typename T,
                CONCEPT_REQUIRES_(!UserDefinedAddressOf<T>())>
            constexpr T *addressof(T &t) noexcept
            {
                return &t;
            }
#endif // RANGES_HAS_BUILTIN_ADDRESSOF
#endif // __cpp_lib_addressof_constexpr
#undef RANGES_HAS_BUILTIN_ADDRESSOF
        } // namespace detail
    } // namespace v3
} // namespace ranges

#endif
