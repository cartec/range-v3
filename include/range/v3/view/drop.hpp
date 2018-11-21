/// \file
// Range v3 library
//
//  Copyright Eric Niebler 2013-present
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3
//

#ifndef RANGES_V3_VIEW_DROP_HPP
#define RANGES_V3_VIEW_DROP_HPP

#include <type_traits>
#include <meta/meta.hpp>
#include <range/v3/iterator_range.hpp>
#include <range/v3/range_concepts.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/range_traits.hpp>
#include <range/v3/view_interface.hpp>
#include <range/v3/algorithm/min.hpp>
#include <range/v3/detail/satisfy_boost_range.hpp>
#include <range/v3/utility/box.hpp>
#include <range/v3/utility/cached_position.hpp>
#include <range/v3/utility/functional.hpp>
#include <range/v3/utility/iterator_traits.hpp>
#include <range/v3/utility/static_const.hpp>
#include <range/v3/view/all.hpp>
#include <range/v3/view/view.hpp>

namespace ranges
{
    inline namespace v3
    {
        namespace detail
        {
            template<typename Rng>
            using O1Droppable = meta::and_<RandomAccessRange<Rng>,
                meta::or_<SizedRange<Rng>, is_infinite<Rng>>>;
        }

        /// \addtogroup group-views
        /// @{
        template<typename Rng>
        struct RANGES_EMPTY_BASES drop_view
          : view_interface<drop_view<Rng>, is_finite<Rng>::value ? finite : range_cardinality<Rng>::value>
          , private cached_position<
                Rng,
                drop_view<Rng>,
                !detail::O1Droppable<Rng>()>
        {
        private:
            friend range_access;
            using difference_type_ = range_difference_type_t<Rng>;
            Rng rng_;
            difference_type_ n_;

            iterator_t<Rng> get_begin_(std::true_type)
            {
                CONCEPT_ASSERT(detail::O1Droppable<Rng>());
                if RANGES_CONSTEXPR_IF(is_infinite<Rng>())
                    return ranges::begin(rng_) + n_;
                else
                    return ranges::begin(rng_) + ranges::min(n_, ranges::distance(rng_));
            }
            iterator_t<Rng> get_begin_(std::false_type)
            {
                CONCEPT_ASSERT(!detail::O1Droppable<Rng>());
                using cache_t = cached_position<Rng, drop_view<Rng>>;
                auto &begin_ = static_cast<cache_t &>(*this);
                if(begin_)
                    return begin_.get(rng_);
                auto it = next(ranges::begin(rng_), n_, ranges::end(rng_));
                begin_.set(rng_, it);
                return it;
            }
        public:
            drop_view() = default;
            drop_view(Rng rng, difference_type_ n)
              : rng_(std::move(rng)), n_(n)
            {
                RANGES_EXPECT(n >= 0);
            }
            iterator_t<Rng> begin()
            {
                return this->get_begin_(detail::O1Droppable<Rng>());
            }
            sentinel_t<Rng> end()
            {
                return ranges::end(rng_);
            }
            template<typename CRng = Rng const,
                CONCEPT_REQUIRES_(detail::O1Droppable<CRng>())>
            iterator_t<CRng> begin() const
            {
                if RANGES_CONSTEXPR_IF(is_infinite<CRng>())
                    return ranges::begin(rng_) + n_;
                else
                    return ranges::begin(rng_) + ranges::min(n_, ranges::distance(rng_));
            }
            template<typename CRng = Rng const,
                CONCEPT_REQUIRES_(Range<CRng>())>
            sentinel_t<CRng> end() const
            {
                return ranges::end(rng_);
            }
            CONCEPT_REQUIRES(SizedRange<Rng const>())
            range_size_type_t<Rng> size() const
            {
                auto const s = static_cast<range_size_type_t<Rng>>(ranges::size(rng_));
                auto const n = static_cast<range_size_type_t<Rng>>(n_);
                return s < n ? 0 : s - n;
            }
            CONCEPT_REQUIRES(SizedRange<Rng>())
            range_size_type_t<Rng> size()
            {
                auto const s = static_cast<range_size_type_t<Rng>>(ranges::size(rng_));
                auto const n = static_cast<range_size_type_t<Rng>>(n_);
                return s < n ? 0 : s - n;
            }
            Rng & base()
            {
                return rng_;
            }
            Rng const & base() const
            {
                return rng_;
            }
        };

        namespace view
        {
            struct drop_fn
            {
            private:
                friend view_access;
                template<typename Int,
                    CONCEPT_REQUIRES_(Integral<Int>())>
                static auto bind(drop_fn drop, Int n)
                RANGES_DECLTYPE_AUTO_RETURN
                (
                    make_pipeable(std::bind(drop, std::placeholders::_1, n))
                )
            #ifndef RANGES_DOXYGEN_INVOKED
                template<typename Int,
                    CONCEPT_REQUIRES_(!Integral<Int>())>
                static detail::null_pipe bind(drop_fn, Int)
                {
                    CONCEPT_ASSERT_MSG(Integral<Int>(),
                        "The argument to view::drop must be Integral");
                    return {};
                }
            #endif
            public:
                template<typename Rng,
                    CONCEPT_REQUIRES_(ViewableRange<Rng>())>
                auto operator()(Rng && rng, range_difference_type_t<Rng> n) const
                RANGES_DECLTYPE_AUTO_RETURN
                (
                    drop_view<all_t<Rng>>{all(static_cast<Rng &&>(rng)), n}
                )
            #ifndef RANGES_DOXYGEN_INVOKED
                template<typename Rng, typename T,
                    CONCEPT_REQUIRES_(!(ViewableRange<Rng>() && Integral<T>()))>
                void operator()(Rng &&, T) const
                {
                    CONCEPT_ASSERT_MSG(Range<Rng>(),
                        "The first argument to view::drop must model the ViewableRange concept");
                    CONCEPT_ASSERT_MSG(Integral<T>(),
                        "The second argument to view::drop must model the Integral concept");
                }
            #endif
            };

            /// \relates drop_fn
            /// \ingroup group-views
            RANGES_INLINE_VARIABLE(view<drop_fn>, drop)
        }
        /// @}
    }
}

RANGES_SATISFY_BOOST_RANGE(::ranges::v3::drop_view)

#endif
