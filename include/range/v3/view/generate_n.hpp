/// \file
// Range v3 library
//
//  Copyright Eric Niebler 2014
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3
//

#ifndef RANGES_V3_VIEW_GENERATE_N_HPP
#define RANGES_V3_VIEW_GENERATE_N_HPP

#include <type_traits>
#include <utility>
#include <meta/meta.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/utility/static_const.hpp>
#include <range/v3/view/generate.hpp>
#include <range/v3/view/take_exactly.hpp>

namespace ranges
{
    inline namespace v3
    {
        /// \addtogroup group-views
        /// @{
        template <typename F>
        using generate_n_view = take_exactly_view<generate_view<F>>;

        namespace view
        {
            struct generate_n_fn
            {
                template<typename G,
                    CONCEPT_REQUIRES_(generate_fn::Concept<G>())>
                generate_n_view<G> operator()(G g, std::size_t n) const
                    noexcept(std::is_nothrow_move_constructible<G>::value)
                {
                    return generate_n_view<G>{generate_view<G>{std::move(g)}, n};
                }
            #ifndef RANGES_DOXYGEN_INVOKED
                template<typename G,
                    CONCEPT_REQUIRES_(!generate_fn::Concept<G>())>
                void operator()(G, std::size_t) const
                {
                    generate_fn::check<G>();
                }
            #endif
            };

            /// \relates generate_n_fn
            /// \ingroup group-views
            RANGES_INLINE_VARIABLE(generate_n_fn, generate_n)
        }
        /// @}
    }
}

#endif
