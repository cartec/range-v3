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

#ifndef RANGES_V3_UTILITY_CACHED_POSITION_HPP
#define RANGES_V3_UTILITY_CACHED_POSITION_HPP

#include <range/v3/begin_end.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/range_concepts.hpp>
#include <range/v3/range_traits.hpp>
#include <range/v3/utility/optional.hpp>

namespace ranges
{
    inline namespace v3
    {
        /// \addtogroup group-utility
        /// @{

        /// \brief Caches a position in a range.
        template<typename R, typename Tag = void,
            bool Enable = true,
            bool IsRandomAccess = RandomAccessRange<R>()>
        struct cached_position
        {
            CONCEPT_ASSERT(Range<R>());

            /// \brief Returns `true` if and only if a position is cached.
            constexpr explicit operator bool() const noexcept
            {
                return false;
            }
            /// \brief Retrieves an `iterator_t<R>` denoting the cached position.
            ///
            /// \pre `*this` caches a position within the \c Range `r`.
            [[noreturn]] iterator_t<R> get(R &r) const
            {
                (void) r;
                RANGES_EXPECT(false);
            }
            /// \brief Caches the position of the `iterator_t<R>` argument.
            ///
            /// \pre The \c Iterator `it` denotes a position in \c Range r.
            RANGES_CXX14_CONSTEXPR
            void set(R &r, iterator_t<R> const &it)
            {
                (void) r;
                (void) it;
            }
        };
        /// @}

        /// \cond
        template<typename R, typename Tag>
        struct cached_position<R, Tag, true, false>
        {
            CONCEPT_ASSERT(Range<R>());

            constexpr explicit operator bool() const noexcept
            {
                return static_cast<bool>(cache_);
            }
            constexpr iterator_t<R> get(R &) const
            {
                return RANGES_EXPECT(*this), *cache_;
            }
            RANGES_CXX14_CONSTEXPR
            void set(R &, iterator_t<R> const &it)
            {
                cache_ = it;
            }
        private:
            detail::non_propagating_cache<iterator_t<R>> cache_;
        };
        template<typename R, typename Tag>
        struct cached_position<R, Tag, true, true>
        {
            CONCEPT_ASSERT(Range<R>());

            constexpr explicit operator bool() const noexcept
            {
                return offset_ >= 0;
            }
            constexpr iterator_t<R> get(R &r) const
            {
                return RANGES_EXPECT(*this), ranges::begin(r) + offset_;
            }
            RANGES_CXX14_CONSTEXPR
            void set(R &r, iterator_t<R> const &it)
            {
                offset_ = it - ranges::begin(r);
            }
        private:
            range_difference_type_t<R> offset_ = -1;
        };
        /// \endcond
    } // namespace v3
} // namespace ranges

#endif // RANGES_V3_UTILITY_CACHED_POSITION_HPP
