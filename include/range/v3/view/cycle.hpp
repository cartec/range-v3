/// \file cycle.hpp
// Range v3 library
//
//  Copyright Eric Niebler 2013-present
//  Copyright Gonzalo Brito Gadeschi 2015
//  Copyright Casey Carter 2015
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3
//

#ifndef RANGES_V3_VIEW_CYCLE_HPP
#define RANGES_V3_VIEW_CYCLE_HPP

#include <type_traits>
#include <utility>
#include <meta/meta.hpp>
#include <range/v3/begin_end.hpp>
#include <range/v3/empty.hpp>
#include <range/v3/range_concepts.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/range_traits.hpp>
#include <range/v3/size.hpp>
#include <range/v3/view_facade.hpp>
#include <range/v3/detail/satisfy_boost_range.hpp>
#include <range/v3/utility/box.hpp>
#include <range/v3/utility/cached_position.hpp>
#include <range/v3/utility/get.hpp>
#include <range/v3/utility/iterator.hpp>
#include <range/v3/utility/static_const.hpp>
#include <range/v3/view/all.hpp>
#include <range/v3/view/view.hpp>

// TODO:
// * Implement bespoke caching. cached_position isn't optimal for Forward &&
//   SizedSentinel, which could cache the (propagatable) size of the base range
//   instead of the end iterator.

namespace ranges
 {
    inline namespace v3
    {
        /// \cond
        namespace detail
        {
            template<class V>
            using CycledNeedsCache = meta::bool_<
                !BoundedRange<V>() &&
                (BidirectionalRange<V>() ||
                 (SizedSentinel<iterator_t<V>, iterator_t<V>>() &&
                  !SizedRange<V>()))>;
        }
        /// \endcond

        /// \addtogroup group-views
        ///@{
        template<typename V,
            CONCEPT_REQUIRES_(ForwardView<V>() && !is_infinite<V>())>
        struct RANGES_EMPTY_BASES cycled_view
          : view_facade<cycled_view<V>, infinite>
          , private cached_position<V, cycled_view<V>,
                detail::CycledNeedsCache<V>::value>
        {
        private:
            friend range_access;

            V v_;

            template<bool IsConst>
            struct cursor
            {
            private:
                friend cursor<true>;
                template<typename T>
                using constify_if = meta::const_if_c<IsConst, T>;
                using cycled_view_t = constify_if<cycled_view>;
                using CRng = constify_if<V>;
                using iterator = iterator_t<CRng>;

                cycled_view_t *v_{};
                iterator it_{};
                std::intmax_t n_ = 0;

            public:
                cursor() = default;
                constexpr cursor(cycled_view_t &v)
                  : v_(&v), it_(ranges::begin(v.v_))
                {}
                template<bool Other,
                    CONCEPT_REQUIRES_(IsConst && !Other)>
                constexpr cursor(cursor<Other> that)
                  : v_(that.v_)
                  , it_(detail::move(that.it_))
                {}
                constexpr bool equal(default_sentinel) const
                {
                    return false;
                }
                constexpr auto read() const
                RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
                (
                    *it_
                )
                constexpr bool equal(cursor const &pos) const
                {
                    return RANGES_EXPECT(v_ == pos.v_),
                        n_ == pos.n_ && it_ == pos.it_;
                }
                RANGES_CXX14_CONSTEXPR void next()
                {
                    auto const end = ranges::end(v_->v_);
                    RANGES_EXPECT(it_ != end);
                    if(++it_ == end)
                    {
                        v_->set_end_(it_);
                        it_ = ranges::begin(v_->v_);
                        ++n_;
                    }
                }
                CONCEPT_REQUIRES(BidirectionalRange<CRng>())
                RANGES_CXX14_CONSTEXPR void prev()
                {
                    if(it_ == ranges::begin(v_->v_))
                    {
                        RANGES_EXPECT(n_ > 0); // decrementing the begin iterator?!
                        it_ = v_->get_end_();
                        --n_;
                    }
                    --it_;
                }
                CONCEPT_REQUIRES(RandomAccessRange<CRng>())
                RANGES_CXX14_CONSTEXPR void advance(std::intmax_t n)
                {
                    v_->cache_end_(it_);
                    auto const len = v_->get_base_size_();
                    auto const begin = ranges::begin(v_->v_);
                    auto const d = it_ - begin;
                    n_ += (d + n) / len;
                    std::intmax_t off = (d + n) % len;
                    RANGES_EXPECT(n_ >= 0);
                    if(off < 0)
                        off += len;
                    it_ = begin + static_cast<range_difference_type_t<V>>(off);
                }
                CONCEPT_REQUIRES(SizedSentinel<iterator, iterator>())
                RANGES_CXX14_CONSTEXPR
                std::intmax_t distance_to(cursor const &that) const
                {
                    RANGES_EXPECT(that.v_ == v_);
                    std::intmax_t result = that.it_ - it_;
                    if(that.n_ != n_)
                        result += (that.n_ - n_) * v_->get_base_size_();
                    return result;
                }
            };

            cached_position<V, cycled_view<V>,
                detail::CycledNeedsCache<V>::value> &
            cache_() noexcept
            {
                return *this;
            }

            CONCEPT_REQUIRES(!simple_view<V>() || !BoundedRange<V const>())
            void set_end_(iterator_t<V> const &it)
            {
                if(cache_())
                    RANGES_ASSERT(it == cache_().get(v_));
                else
                    cache_().set(v_, it);
            }
            CONCEPT_REQUIRES(!simple_view<V>() || !BoundedRange<V const>())
            void cache_end_(iterator_t<V> const &hint)
            {
                if(!cache_())
                    cache_().set(v_, ranges::next(hint, ranges::end(v_)));
            }
            CONCEPT_REQUIRES(!simple_view<V>() || !BoundedRange<V const>())
            iterator_t<V> get_end_()
            {
                RANGES_EXPECT(cache_());
                return cache_().get(v_);
            }
            CONCEPT_REQUIRES(!simple_view<V>() || !BoundedRange<V const>())
            RANGES_CXX14_CONSTEXPR cursor<false> begin_cursor()
            {
                return {*this};
            }

            CONCEPT_REQUIRES(BoundedRange<V const>())
            constexpr void set_end_(iterator_t<V const> const &) const
            {}
            CONCEPT_REQUIRES(BoundedRange<V const>())
            constexpr void cache_end_(iterator_t<V const> const &) const
            {}
            CONCEPT_REQUIRES(BoundedRange<V const>())
            constexpr iterator_t<V const> get_end_() const
            {
                return ranges::end(v_);
            }
            CONCEPT_REQUIRES(BoundedRange<V const>())
            constexpr cursor<true> begin_cursor() const
            {
                return {*this};
            }

            CONCEPT_REQUIRES(!SizedRange<V const>())
            RANGES_CXX14_CONSTEXPR range_difference_type_t<V> get_base_size_()
            {
                if RANGES_CONSTEXPR_IF(SizedRange<V>())
                    return ranges::distance(v_);
                else
                    return ranges::distance(ranges::begin(v_), get_end_());
            }
            CONCEPT_REQUIRES(SizedRange<V const>())
            constexpr range_difference_type_t<V> get_base_size_() const
            {
                return ranges::distance(v_);
            }

        public:
            cycled_view() = default;
            /// \pre <tt>!empty(v)</tt>
            constexpr explicit cycled_view(V v)
              : v_((RANGES_EXPECT(!ranges::empty(v)), detail::move(v)))
            {}
        };

        namespace view
        {
            /// Returns an infinite range that endlessly repeats the source
            /// range.
            struct cycle_fn
            {
#if RANGES_CXX_IF_CONSTEXPR >= RANGES_CXX_IF_CONSTEXPR_17
                /// \pre <tt>!empty(r)</tt>
                template<typename R, CONCEPT_REQUIRES_(ForwardRange<R>())>
                constexpr auto operator()(R &&r) const
                {
                    if constexpr(is_infinite<R>())
                        return all(static_cast<R &&>(r));
                    else
                        return cycled_view<all_t<R>>{all(static_cast<R &&>(r))};
                }
#else // ^^^ have "if constexpr" / no "if constexpr" vvv
            private:
                template<typename R>
                static constexpr cycled_view<all_t<R>> impl_(R &&r, std::false_type)
                {
                    return cycled_view<all_t<R>>{all(static_cast<R &&>(r))};
                }
                template<typename R>
                static constexpr all_t<R> impl_(R &&r, std::true_type)
                {
                    CONCEPT_ASSERT(ForwardRange<R>() && is_infinite<R>());
                    return all(static_cast<R &&>(r));
                }
            public:
                /// \pre <tt>!empty(r)</tt>
                template<typename R, CONCEPT_REQUIRES_(ForwardRange<R>())>
                constexpr auto operator()(R &&r) const
                RANGES_DECLTYPE_AUTO_RETURN
                (
                    impl_(static_cast<R &&>(r), is_infinite<R>())
                )
#endif // RANGES_CXX_IF_CONSTEXPR >= RANGES_CXX_IF_CONSTEXPR_17

#ifndef RANGES_DOXYGEN_INVOKED
                template<typename R, CONCEPT_REQUIRES_(!ForwardRange<R>())>
                void operator()(R &&) const
                {
                    CONCEPT_ASSERT_MSG(ForwardRange<R>(),
                        "The object on which view::cycle operates must model "
                        "the ForwardRange concept.");
                }
#endif
            };

            /// \relates cycle_fn
            /// \ingroup group-views
            RANGES_INLINE_VARIABLE(view<cycle_fn>, cycle)
       } // namespace view
       /// @}
    } // namespace v3
} // namespace ranges

RANGES_SATISFY_BOOST_RANGE(::ranges::v3::cycled_view)

#endif
