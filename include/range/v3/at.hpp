/// \file
// Range v3 library
//
//  Copyright Eric Niebler 2014
//  Copyright Gonzalo Brito Gadeschi 2017
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3
//

#ifndef RANGES_V3_AT_HPP
#define RANGES_V3_AT_HPP

#include <stdexcept>
#include <range/v3/range_fwd.hpp>
#include <range/v3/begin_end.hpp>
#include <range/v3/range_traits.hpp>
#include <range/v3/range_concepts.hpp>
#include <range/v3/distance.hpp>
#include <range/v3/utility/static_const.hpp>
#include <range/v3/index.hpp>

namespace ranges
{
    inline namespace v3
    {
        /// Checked indexed range access.
        ///
        /// \ingroup group-core
        struct at_fn
        {
        private:
            template<class = void>
            [[noreturn]] static void throw_out_of_range()
            {
                throw std::out_of_range("ranges::at");
            }
            template<class Rng,
                class U = meta::_t<std::make_unsigned<range_difference_type_t<Rng>>>>
            static constexpr auto check_throw(Rng &&rng, range_difference_type_t<Rng> n)
            RANGES_DECLTYPE_AUTO_RETURN
            (
                (static_cast<U>(n) >= static_cast<U>(ranges::distance(rng)))
                    ? (throw_out_of_range(), 0)
                    : 0
            )
        public:
            /// \return `ranges::begin(rng)[n]`
            template<typename Rng,
                CONCEPT_REQUIRES_(RandomAccessRange<Rng>() && SizedRange<Rng>())>
            constexpr auto operator()(Rng &&rng, range_difference_type_t<Rng> n) const
            RANGES_DECLTYPE_AUTO_RETURN
            (
                // Workaround https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67371 in GCC 5
                ((void)check_throw(rng, n),
                    ranges::begin(rng)[n])
            )
            /// \return `ranges::begin(rng)[n]`
            template<typename Rng,
                CONCEPT_REQUIRES_(RandomAccessRange<Rng>() && !SizedRange<Rng>())>
            RANGES_DEPRECATED(
                "Checked indexed range access (ranges::at) on !SizedRanges is deprecated: "
                "this version performs unchecked access (the range size cannot be computed in O(1) for !SizedRanges). "
                "Use ranges::index for unchecked access instead.")
            constexpr auto operator()(Rng &&rng, range_difference_type_t<Rng> n) const
            RANGES_DECLTYPE_AUTO_RETURN
            (
                index(std::forward<Rng>(rng), n)
            )

            /// \cond
            template<typename Rng,
                CONCEPT_REQUIRES_(!RandomAccessRange<Rng>())>
            void operator()(Rng&&, range_difference_type_t<Rng>) const
            {
                CONCEPT_ASSERT_MSG(RandomAccessRange<Rng>(),
                    "ranges::at(rng, idx): rng argument must satisfy the RandomAccessRange concept.");
            }
            /// \endcond
        };

        /// Checked indexed range access.
        ///
        /// \ingroup group-core
        /// \sa `at_fn`
        RANGES_INLINE_VARIABLE(at_fn, at)
    }
}

#endif
