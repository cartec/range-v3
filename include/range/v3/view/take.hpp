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

#ifndef RANGES_V3_VIEW_TAKE_HPP
#define RANGES_V3_VIEW_TAKE_HPP

#include <type_traits>
#include <range/v3/range_fwd.hpp>
#include <range/v3/range_concepts.hpp>
#include <range/v3/range_traits.hpp>
#include <range/v3/view_adaptor.hpp>
#include <range/v3/algorithm/min.hpp>
#include <range/v3/detail/satisfy_boost_range.hpp>
#include <range/v3/utility/counted_iterator.hpp>
#include <range/v3/utility/functional.hpp>
#include <range/v3/utility/static_const.hpp>
#include <range/v3/view/take_exactly.hpp>
#include <range/v3/view/view.hpp>

namespace ranges
{
    inline namespace v3
    {
        template<typename V, CONCEPT_REQUIRES_(View<V>())>
        struct take_view
          : view_adaptor<take_view<V>, V, finite>
        {
        private:
            friend range_access;

            range_difference_type_t<V> n_ = 0;

            template<bool IsConst>
            using CI = counted_iterator<iterator_t<meta::const_if_c<IsConst, V>>>;
            template<bool IsConst>
            using S = sentinel_t<meta::const_if_c<IsConst, V>>;

            template<bool IsConst>
            struct adaptor : adaptor_base
            {
                adaptor() = default;
                template<bool Other,
                    CONCEPT_REQUIRES_(IsConst && !Other)>
                adaptor(adaptor<Other>)
                {}
                CI<IsConst> begin(meta::const_if_c<IsConst, take_view> &v) const
                {
                    return {ranges::begin(v.base()), v.n_};
                }
            };

            template<bool IsConst>
            struct sentinel_adaptor : adaptor_base
            {
                sentinel_adaptor() = default;
                template<bool Other,
                    CONCEPT_REQUIRES_(IsConst && !Other)>
                sentinel_adaptor(sentinel_adaptor<Other>)
                {}
                bool empty(CI<IsConst> const &that, S<IsConst> const &sent) const
                {
                    return 0 == that.count() || sent == that.base();
                }
            };

            adaptor<simple_view<V>()> begin_adaptor()
            {
                return {};
            }
            sentinel_adaptor<simple_view<V>()> end_adaptor()
            {
                return {};
            }
            CONCEPT_REQUIRES(Range<V const>())
            adaptor<true> begin_adaptor() const
            {
                return {};
            }
            CONCEPT_REQUIRES(Range<V const>())
            sentinel_adaptor<true> end_adaptor() const
            {
                return {};
            }
        public:
            take_view() = default;
            explicit take_view(V v, range_difference_type_t<V> n)
              : take_view::view_adaptor(std::move(v)), n_{n}
            {
                RANGES_EXPECT(n >= 0);
            }
            template<typename R,
                CONCEPT_REQUIRES_(ViewableRange<R>() &&
                    Constructible<V, view::all_t<R>>())>
            explicit take_view(R &&r, range_difference_type_t<V> n)
              : take_view{V(view::all(static_cast<R &&>(r))), n}
            {}
        };

#if RANGES_CXX_DEDUCTION_GUIDES >= RANGES_CXX_DEDUCTION_GUIDES_17
        template<typename R, CONCEPT_REQUIRES_(ViewableRange<R>())>
        take_view(R &&, range_difference_type_t<R>) -> take_view<view::all_t<R>>;
#endif // RANGES_CXX_DEDUCTION_GUIDES >= RANGES_CXX_DEDUCTION_GUIDES_17

        namespace view
        {
            struct take_fn
            {
            private:
                friend view_access;

                template<typename Int, CONCEPT_REQUIRES_(Integral<Int>())>
                static auto bind(take_fn take, Int n)
                RANGES_DECLTYPE_AUTO_RETURN
                (
                    make_pipeable(std::bind(take, std::placeholders::_1, n))
                )

            #ifndef RANGES_DOXYGEN_INVOKED
                template<typename Int, CONCEPT_REQUIRES_(!Integral<Int>())>
                static detail::null_pipe bind(take_fn, Int)
                {
                    CONCEPT_ASSERT_MSG(Integral<Int>(),
                        "The object passed to view::take must be a model of the Integral concept.");
                    return {};
                }
            #endif

#if RANGES_CXX_IF_CONSTEXPR >= RANGES_CXX_IF_CONSTEXPR_17
            public:
                template<typename R, CONCEPT_REQUIRES_(ViewableRange<R>())>
                auto operator()(R &&r, range_difference_type_t<R> n) const
                {
                    if constexpr(is_infinite<R>())
                        return take_exactly(static_cast<R &&>(r), n);
                    else if constexpr(SizedRange<R>())
                        return take_exactly(static_cast<R &&>(r),
                            ranges::min(n, distance(r)));
                    else
                        return take_view<all_t<R>>{all(static_cast<R &&>(r)), n};
                }
#else
                template<typename R>
                static auto impl_(R &&r, range_difference_type_t<R> n, std::false_type)
                RANGES_DECLTYPE_AUTO_RETURN
                (
                    take_view<all_t<R>>{all(static_cast<R &&>(r)), n}
                )
                template<typename R>
                static auto impl_(R &&r, range_difference_type_t<R> n, std::true_type)
                RANGES_DECLTYPE_AUTO_RETURN
                (
                    take_exactly(
                        static_cast<R &&>(r),
                        is_infinite<R>() ? n : ranges::min(n, distance(r)))
                )
            public:
                template<typename R, CONCEPT_REQUIRES_(ViewableRange<R>())>
                auto operator()(R &&r, range_difference_type_t<R> n) const
                RANGES_DECLTYPE_AUTO_RETURN
                (
                    impl_(static_cast<R &&>(r), n,
                        meta::or_<is_infinite<R>, SizedRange<R>>())
                )
#endif

            #ifndef RANGES_DOXYGEN_INVOKED
                template<typename R, typename T, CONCEPT_REQUIRES_(!ViewableRange<R>())>
                void operator()(R &&, T &&) const
                {
                    CONCEPT_ASSERT_MSG(ViewableRange<R>(),
                        "The object on which view::take operates must model the Range concept.");
                    CONCEPT_ASSERT_MSG(Integral<T>(),
                        "The second argument to view::take must model the Integral concept.");
                }
            #endif
            };

            /// \relates take_fn
            /// \ingroup group-views
            RANGES_INLINE_VARIABLE(view<take_fn>, take)
        }
        /// @}
    }
}

RANGES_SATISFY_BOOST_RANGE(::ranges::v3::take_view)

#endif
