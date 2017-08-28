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

#ifndef RANGES_V3_INDEX_HPP
#define RANGES_V3_INDEX_HPP

#include <range/v3/range_fwd.hpp>
#include <range/v3/begin_end.hpp>
#include <range/v3/range_traits.hpp>
#include <range/v3/range_concepts.hpp>
#include <range/v3/distance.hpp>
#include <range/v3/utility/static_const.hpp>

namespace ranges
{
    inline namespace v3
    {
        /// Unchecked indexed range access.
        ///
        /// \ingroup group-core
        struct index_fn
        {
            /// \return `ranges::begin(rng)[n]`
            template<typename Rng,
                CONCEPT_REQUIRES_(RandomAccessRange<Rng>())>
            constexpr auto operator()(Rng &&rng, range_difference_type_t<Rng> n) const
                noexcept(noexcept(ranges::begin(rng)[n]) &&
                    (!SizedRange<Rng>() || noexcept(ranges::distance(rng)))) ->
                decltype(ranges::begin(rng)[n])
            {
                return RANGES_EXPECT(!SizedRange<Rng>() || n < ranges::distance(rng)),
                    ranges::begin(rng)[n];
            }

            /// \cond
            template<typename Rng,
                CONCEPT_REQUIRES_(!RandomAccessRange<Rng>())>
            void operator()(Rng&&, range_difference_type_t<Rng>) const
            {
                CONCEPT_ASSERT_MSG(RandomAccessRange<Rng>(),
                    "ranges::index(rng, idx): rng argument must satisfy the RandomAccessRange concept.");
            }
            /// \endcond
        };

        /// Unchecked indexed range access.
        ///
        /// \ingroup group-core
        /// \sa `index_fn`
        RANGES_INLINE_VARIABLE(index_fn, index)
    }
}

#endif  // RANGES_V3_INDEX_HPP
