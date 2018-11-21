/// \file
// Range v3 library
//
//  Copyright Eric Niebler 2014-present
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3
//

#ifndef RANGES_V3_VIEW_REVERSE_HPP
#define RANGES_V3_VIEW_REVERSE_HPP

#include <iterator>
#include <utility>
#include <meta/meta.hpp>
#include <range/v3/begin_end.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/range_traits.hpp>
#include <range/v3/size.hpp>
#include <range/v3/view_adaptor.hpp>
#include <range/v3/detail/satisfy_boost_range.hpp>
#include <range/v3/utility/box.hpp>
#include <range/v3/utility/cached_position.hpp>
#include <range/v3/utility/get.hpp>
#include <range/v3/utility/iterator.hpp>
#include <range/v3/utility/static_const.hpp>
#include <range/v3/view/all.hpp>
#include <range/v3/view/view.hpp>

namespace ranges
{
    inline namespace v3
    {
        /// \addtogroup group-views
        /// @{
        template<typename Rng>
        struct RANGES_EMPTY_BASES reverse_view
          : view_interface<reverse_view<Rng>, range_cardinality<Rng>::value>
          , private cached_position<
                Rng, reverse_view<Rng>, !BoundedRange<Rng>()>
        {
        private:
            CONCEPT_ASSERT(BidirectionalRange<Rng>());

            Rng rng_;

            RANGES_CXX14_CONSTEXPR
            reverse_iterator<iterator_t<Rng>> begin_(std::true_type)
            {
                // CONCEPT_ASSERT(BoundedRange<Rng>());
                return make_reverse_iterator(ranges::end(rng_));
            }
            RANGES_CXX14_CONSTEXPR
            reverse_iterator<iterator_t<Rng>> begin_(std::false_type)
            {
                // CONCEPT_ASSERT(!BoundedRange<Rng>());
                using cache_t = cached_position<Rng, reverse_view<Rng>>;
                auto &end_ = static_cast<cache_t &>(*this);
                if(end_)
                    return make_reverse_iterator(end_.get(rng_));

                auto it = ranges::next(ranges::begin(rng_), ranges::end(rng_));
                end_.set(rng_, it);
                return make_reverse_iterator(std::move(it));
            }
            template<typename T>
            using not_self_ =
                meta::if_c<!std::is_same<reverse_view, detail::decay_t<T>>::value, T>;
        public:
            reverse_view() = default;
            explicit constexpr reverse_view(Rng rng)
              : rng_(detail::move(rng))
            {}
            template<typename O,
                CONCEPT_REQUIRES_(ViewableRange<not_self_<O>>() &&
                    BidirectionalRange<O>() && Constructible<Rng, view::all_t<O>>())>
            explicit constexpr reverse_view(O&& o)
              : rng_(view::all(static_cast<O &&>(o)))
            {}
            Rng base() const
            {
                return rng_;
            }
            RANGES_CXX14_CONSTEXPR
            reverse_iterator<iterator_t<Rng>> begin()
            {
                return begin_(meta::bool_<(bool) BoundedRange<Rng>()>{});
            }
            template<typename CRng = Rng const,
                CONCEPT_REQUIRES_(BoundedRange<CRng>())>
            constexpr reverse_iterator<iterator_t<CRng>> begin() const
            {
                return make_reverse_iterator(ranges::end(rng_));
            }
            RANGES_CXX14_CONSTEXPR
            reverse_iterator<iterator_t<Rng>> end()
            {
                return make_reverse_iterator(ranges::begin(rng_));
            }
            template<typename CRng = Rng const,
                CONCEPT_REQUIRES_(BoundedRange<CRng>())>
            constexpr reverse_iterator<iterator_t<CRng>> end() const
            {
                return make_reverse_iterator(ranges::begin(rng_));
            }
            CONCEPT_REQUIRES(SizedRange<Rng const>())
            constexpr range_size_type_t<Rng> size() const
            {
                return ranges::size(rng_);
            }
        };

        namespace detail
        {
            template<class>
            struct is_reversed_iterator_range
              : std::false_type
            {};
            template<class I>
            struct is_reversed_iterator_range<iterator_range<reverse_iterator<I>>>
              : std::true_type
            {
                static constexpr bool is_sized = false;
            };
            template<class I>
            struct is_reversed_iterator_range<iterator_range<std::reverse_iterator<I>>>
              : std::true_type
            {
                static constexpr bool is_sized = false;
            };
            template<class I>
            struct is_reversed_iterator_range<sized_iterator_range<reverse_iterator<I>>>
              : std::true_type
            {
                static constexpr bool is_sized = true;
            };
            template<class I>
            struct is_reversed_iterator_range<sized_iterator_range<std::reverse_iterator<I>>>
              : std::true_type
            {
                static constexpr bool is_sized = true;
            };
        }

        namespace view
        {
            struct reverse_fn
            {
#if RANGES_CXX_IF_CONSTEXPR >= RANGES_CXX_IF_CONSTEXPR_17
                template<typename Rng, CONCEPT_REQUIRES_(BidirectionalRange<Rng>())>
                constexpr auto operator()(Rng &&rng) const
                {
                    if constexpr(meta::is<uncvref_t<Rng>, reverse_view>::value)
                        return all(static_cast<Rng &&>(rng).base());
                    else if constexpr(detail::is_reversed_iterator_range<uncvref_t<Rng>>::value)
                    {
                        if constexpr(detail::is_reversed_iterator_range<uncvref_t<Rng>>::is_sized)
                            return make_iterator_range(rng.end().base(), rng.begin().base(), rng.size());
                        else
                            return make_iterator_range(rng.end().base(), rng.begin().base());
                    }
                    else
                        return reverse_view<all_t<Rng>>{all(static_cast<Rng &&>(rng))};
                }
#else // ^^^ have "if constexpr" / no "if constexpr" vvv
    #if defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 5
    // Avoid GCC5 C++11 bug that ODR-uses std::declval?!?
    #define RANGES_GCC5_HACK RANGES_CXX14_CONSTEXPR
    #else
    #define RANGES_GCC5_HACK constexpr
    #endif
            private:
                template<typename Rng>
                static RANGES_GCC5_HACK auto impl_(Rng &&rng, std::false_type)
                RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
                (
                    reverse_view<all_t<Rng>>{all(static_cast<Rng &&>(rng))}
                )
                template<typename Rng>
                static RANGES_GCC5_HACK auto impl_(Rng &&rng, std::true_type)
                RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
                (
                    all(static_cast<Rng &&>(rng).base())
                )
            public:
                template<typename Rng, CONCEPT_REQUIRES_(BidirectionalRange<Rng>())>
                RANGES_GCC5_HACK auto operator()(Rng &&rng) const
                RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
                (
                    impl_(static_cast<Rng &&>(rng),
                        meta::bool_<meta::is<uncvref_t<Rng>, reverse_view>::value>{})
                )
    #undef RANGES_GCC5_HACK
#endif // RANGES_CXX_IF_CONSTEXPR >= RANGES_CXX_IF_CONSTEXPR_17

            #ifndef RANGES_DOXYGEN_INVOKED
                // For error reporting
                template<typename Rng, CONCEPT_REQUIRES_(!BidirectionalRange<Rng>())>
                void operator()(Rng &&) const
                {
                    CONCEPT_ASSERT_MSG(BidirectionalRange<Rng>(),
                        "The object on which view::reverse operates must model the "
                        "BidirectionalRange concept.");
                }
            #endif
            };

            /// \relates reverse_fn
            /// \ingroup group-views
            RANGES_INLINE_VARIABLE(view<reverse_fn>, reverse)
        }
        /// @}
    }
}

RANGES_SATISFY_BOOST_RANGE(::ranges::v3::reverse_view)

#endif
