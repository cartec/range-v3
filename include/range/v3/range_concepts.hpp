/// \file
// Range v3 library
//
//  Copyright Eric Niebler 2013-2014
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3
//

#ifndef RANGES_V3_RANGE_CONCEPTS_HPP
#define RANGES_V3_RANGE_CONCEPTS_HPP

#include <utility>
#include <type_traits>
#include <initializer_list>
#include <meta/meta.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/begin_end.hpp>
#include <range/v3/data.hpp>
#include <range/v3/utility/iterator.hpp>
#include <range/v3/utility/iterator_concepts.hpp>
#include <range/v3/utility/iterator_traits.hpp>
#include <range/v3/utility/static_const.hpp>

#ifndef RANGES_NO_STD_FORWARD_DECLARATIONS
// Non-portable forward declarations of standard containers
RANGES_BEGIN_NAMESPACE_STD
    template<class Key, class Compare /*= less<Key>*/, class Alloc /*= allocator<Key>*/>
    class set;

    template<class Key, class Compare /*= less<Key>*/, class Alloc /*= allocator<Key>*/>
    class multiset;

    template<class Key, class Hash /*= hash<Key>*/, class Pred /*= equal_to<Key>*/, class Alloc /*= allocator<Key>*/>
    class unordered_set;

    template<class Key, class Hash /*= hash<Key>*/, class Pred /*= equal_to<Key>*/, class Alloc /*= allocator<Key>*/>
    class unordered_multiset;
RANGES_END_NAMESPACE_STD
#else
#include <set>
#include <unordered_set>
#endif

namespace ranges
{
    inline namespace v3
    {
        /// \cond
        namespace detail
        {
            template<typename T>
            struct view_predicate_;
        }
        /// \endcond

        /// \addtogroup group-concepts
        /// @{

        // Specialize this if the default is wrong.
        template<typename T>
        struct enable_view
        {};

        namespace concepts
        {
            ///
            /// Range concepts below
            ///

            struct Range
            {
                // Associated types
                template<typename T>
                using iterator_t = decltype(begin(std::declval<T &>()));

                template<typename T>
                using sentinel_t = decltype(end(std::declval<T &>()));

                template<typename T>
                using difference_t = concepts::WeaklyIncrementable::difference_t<iterator_t<T>>;

                template<typename T>
                auto requires_(T &t) -> decltype(
                    concepts::valid_expr(
                        concepts::model_of<Sentinel>(end(t), begin(t))
                    ));
            };

            struct OutputRange
              : refines<Range(_1)>
            {
                template<typename T, typename V>
                auto requires_() -> decltype(
                    concepts::valid_expr(
                        concepts::model_of<OutputIterator, Range::iterator_t<T>, V>()
                    ));
            };

            struct InputRange
              : refines<Range>
            {
                // Associated types
                template<typename T>
                using category_t = concepts::InputIterator::category_t<iterator_t<T>>;

                template<typename T>
                using value_t = concepts::Readable::value_t<iterator_t<T>>;

                template<typename T>
                using reference_t = concepts::Readable::reference_t<iterator_t<T>>;

                template<typename T>
                using rvalue_reference_t = concepts::Readable::rvalue_reference_t<iterator_t<T>>;

                template<typename T>
                using common_reference_t = concepts::Readable::common_reference_t<iterator_t<T>>;

                template<typename T>
                auto requires_(T &t) -> decltype(
                    concepts::valid_expr(
                        concepts::model_of<InputIterator>(begin(t))
                    ));
            };

            struct ForwardRange
              : refines<InputRange>
            {
                template<typename T>
                auto requires_(T &t) -> decltype(
                    concepts::valid_expr(
                        concepts::model_of<ForwardIterator>(begin(t))
                    ));
            };

            struct BidirectionalRange
              : refines<ForwardRange>
            {
                template<typename T>
                auto requires_(T &t) -> decltype(
                    concepts::valid_expr(
                        concepts::model_of<BidirectionalIterator>(begin(t))
                    ));
            };

            struct RandomAccessRange
              : refines<BidirectionalRange>
            {
                template<typename T>
                auto requires_(T &t) -> decltype(
                    concepts::valid_expr(
                        concepts::model_of<RandomAccessIterator>(begin(t))
                    ));
            };

            struct ContiguousRange
              : refines<RandomAccessRange>
            {
                template<typename Rng>
                using data_reference_t = decltype(*data(std::declval<Rng&>()));

                template<typename Rng>
                using datum_t = meta::_t<std::remove_reference<data_reference_t<Rng>>>;

                template<typename Rng>
                auto requires_() -> decltype(
                    concepts::valid_expr(
                        concepts::model_of<Same, InputRange::value_t<Rng>,
                            meta::_t<std::remove_cv<datum_t<Rng>>>>(),
                        concepts::model_of<Same, data_reference_t<Rng>,
                            concepts::InputRange::reference_t<Rng>>()
                    ));
            };

            struct BoundedRange
              : refines<Range>
            {
                template<typename T>
                auto requires_(T &t) -> decltype(
                    concepts::valid_expr(
                        concepts::same_type(begin(t), end(t))
                    ));
            };
	}

        /// \addtogroup group-concepts
        // Specialize this if the default is wrong.
        template<typename T>
        struct disable_sized_range : std::false_type {};

        /// \cond
        namespace _size_
        {
            template<typename T>
            void size(T const &) = delete;

            struct fn : iter_size_fn
            {
            private:
                template<typename R, typename N>
                using size_type =
                    meta::_t<meta::if_<
                        concepts::models<concepts::Range, R>,
                        meta::defer<concepts::Range::difference_t, R>,
                        std::make_signed<decltype(+std::declval<N>())>>>;

                template<class To, class From,
                    CONCEPT_REQUIRES_(ranges::Integral<To>() && ranges::Integral<From>())>
                static RANGES_CXX14_CONSTEXPR To narrow_cast(From from) noexcept
                {
                    CONCEPT_ASSERT(std::is_signed<To>());
                    RANGES_EXPECT(!std::is_signed<From>() || from >= From{});
                    auto const to = static_cast<To>(from);
                    RANGES_EXPECT(to >= To{});
                    using C = common_type_t<To, From>;
                    RANGES_EXPECT(static_cast<C>(to) == static_cast<C>(from));
                    return to;
                }

                template<typename R, std::ptrdiff_t N>
                static constexpr std::ptrdiff_t impl_(R const (&)[N], int) noexcept
                {
                    return N;
                }

                // Prefer member if it returns Integral.
                template<typename R,
                    typename = meta::if_c<!disable_sized_range<R>()>,
                    typename N = decltype(aux::copy(std::declval<R const &>().size())),
                    CONCEPT_REQUIRES_(Integral<N>())>
                static constexpr auto impl_(R const &r, int)
                RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
                (
                    fn::narrow_cast<size_type<R, N>>(r.size())
                )

                // Use ADL if it returns Integral.
                template<typename R,
                    typename = meta::if_c<!disable_sized_range<R>()>,
                    typename N = decltype(aux::copy(size(std::declval<R const &>()))),
                    CONCEPT_REQUIRES_(Integral<N>())>
                static constexpr auto impl_(R const &r, long)
                RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
                (
                    fn::narrow_cast<size_type<R, N>>(size(r))
                )

                template<typename R,
                    CONCEPT_REQUIRES_(concepts::models<concepts::ForwardRange, R>())>
                static constexpr auto impl_(R const &r, ...)
                RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
                (
                    ranges::iter_size(ranges::cbegin(r), ranges::cend(r))
                )

            public:
                using iter_size_fn::operator();

                template<typename R>
                constexpr auto operator()(R const &r) const
                RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
                (
                    fn::impl_(r, 42)
                )

                template<typename T, typename Fn = fn>
                constexpr auto operator()(std::reference_wrapper<T> ref) const
                RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
                (
                    Fn()(ref.get())
                )

                template<typename T, bool RValue, typename Fn = fn>
                constexpr auto operator()(ranges::reference_wrapper<T, RValue> ref) const
                RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
                (
                    Fn()(ref.get())
                )
            };
        }
        /// \endcond

        /// \ingroup group-core
        /// \return The result of an unqualified call to `size`
        RANGES_INLINE_VARIABLE(_size_::fn, size)

        namespace concepts
        {
            struct SizedRange
              : refines<Range>
            {
                template<typename T>
                using size_t = decltype(size(std::declval<T&>()));

                template<typename T>
                auto requires_(T &t) -> decltype(
                    concepts::valid_expr(
                        concepts::is_false(disable_sized_range<uncvref_t<T>>()),
                        concepts::has_type<Range::difference_t<T>>(size(t))
                    ));
            };

            ///
            /// View concepts below
            ///

            struct View
              : refines<Range>
            {
                template<typename T>
                auto requires_() -> decltype(
                    concepts::valid_expr(
                        concepts::model_of<SemiRegular, T>(),
                        concepts::is_true(detail::view_predicate_<T>())
                    ));
            };

            struct OutputView
              : refines<View(_1), OutputRange>
            {};

            struct InputView
              : refines<View, InputRange>
            {};

            struct ForwardView
              : refines<InputView, ForwardRange>
            {};

            struct BidirectionalView
              : refines<ForwardView, BidirectionalRange>
            {};

            struct RandomAccessView
              : refines<BidirectionalView, RandomAccessRange>
            {};

            // Additional concepts for checking additional orthogonal properties
            struct BoundedView
              : refines<View, BoundedRange>
            {};

            struct SizedView
              : refines<View, SizedRange>
            {};
        }

        template<typename T>
        using Range = concepts::models<concepts::Range, T>;

        template<typename T, typename V>
        using OutputRange = concepts::models<concepts::OutputRange, T, V>;

        template<typename T>
        using InputRange = concepts::models<concepts::InputRange, T>;

        template<typename T>
        using ForwardRange = concepts::models<concepts::ForwardRange, T>;

        template<typename T>
        using BidirectionalRange = concepts::models<concepts::BidirectionalRange, T>;

        template<typename T>
        using RandomAccessRange = concepts::models<concepts::RandomAccessRange, T>;

        template<typename Rng>
        using ContiguousRange = concepts::models<concepts::ContiguousRange, Rng>;

        template<typename T>
        using BoundedRange = concepts::models<concepts::BoundedRange, T>;

        template<typename T>
        using SizedRange = concepts::models<concepts::SizedRange, T>;

        template<typename T>
        using View = concepts::models<concepts::View, T>;

        template<typename T, typename V>
        using OutputView = concepts::models<concepts::OutputView, T, V>;

        template<typename T>
        using InputView = concepts::models<concepts::InputView, T>;

        template<typename T>
        using ForwardView = concepts::models<concepts::ForwardView, T>;

        template<typename T>
        using BidirectionalView = concepts::models<concepts::BidirectionalView, T>;

        template<typename T>
        using RandomAccessView = concepts::models<concepts::RandomAccessView, T>;

        // Extra concepts:
        template<typename T>
        using BoundedView = concepts::models<concepts::BoundedView, T>;

        template<typename T>
        using SizedView = concepts::models<concepts::SizedView, T>;

        ////////////////////////////////////////////////////////////////////////////////////////////
        // range_concept
        template<typename T>
        using range_concept =
            concepts::most_refined<
                meta::list<
                    concepts::RandomAccessRange,
                    concepts::BidirectionalRange,
                    concepts::ForwardRange,
                    concepts::InputRange>, T>;

        template<typename T>
        using range_concept_t =
            meta::_t<range_concept<T>>;

        ////////////////////////////////////////////////////////////////////////////////////////////
        // bounded_range_concept
        template<typename T>
        using bounded_range_concept =
            concepts::most_refined<
                meta::list<
                    concepts::BoundedRange,
                    concepts::Range>, T>;

        template<typename T>
        using bounded_range_concept_t =
            meta::_t<bounded_range_concept<T>>;

        ////////////////////////////////////////////////////////////////////////////////////////////
        // sized_range_concept
        template<typename T>
        using sized_range_concept =
            concepts::most_refined<
                meta::list<
                    concepts::SizedRange,
                    concepts::Range>, T>;

        template<typename T>
        using sized_range_concept_t =
            meta::_t<sized_range_concept<T>>;

        ////////////////////////////////////////////////////////////////////////////////////////////
        // bounded_view_concept
        template<typename T>
        using bounded_view_concept =
            concepts::most_refined<
                meta::list<
                    concepts::BoundedView,
                    concepts::View>, T>;

        template<typename T>
        using bounded_view_concept_t = meta::_t<bounded_view_concept<T>>;

        ////////////////////////////////////////////////////////////////////////////////////////////
        // sized_view_concept
        template<typename T>
        using sized_view_concept =
            concepts::most_refined<
                meta::list<
                    concepts::SizedView,
                    concepts::View>, T>;

        template<typename T>
        using sized_view_concept_t = meta::_t<sized_view_concept<T>>;

        ////////////////////////////////////////////////////////////////////////////////////////////
        // view_concept
        template<typename T>
        using view_concept =
            concepts::most_refined<
                meta::list<
                    concepts::View,
                    concepts::Range>, T>;

        template<typename T>
        using view_concept_t = meta::_t<view_concept<T>>;

        /// @}

        /// \cond
        namespace detail
        {
            template<typename T>
            std::is_same<reference_t<concepts::Range::iterator_t<T>>, reference_t<concepts::Range::iterator_t<T const>>>
            view_like_(int);

            template<typename T>
            std::false_type
            view_like_(long);

            template<typename T>
            using view_like = decltype(detail::view_like_<T>(42));

            // Something is a view if it's a Range and either:
            //  - It doesn't look like a container, or
            //  - It's derived from view_base
            template<typename T>
            struct view_predicate_
              : meta::_t<meta::if_<
                    meta::is_trait<enable_view<T>>,
                    enable_view<T>,
                    meta::bool_<view_like<T>() || DerivedFrom<T, view_base>()>>>
            {};

            template<typename T>
            struct view_predicate_<std::initializer_list<T>>
              : std::false_type
            {};

            template<class Key, class Compare, class Alloc>
            struct view_predicate_<std::set<Key, Compare, Alloc>>
              : std::false_type
            {};

            template<class Key, class Compare, class Alloc>
            struct view_predicate_<std::multiset<Key, Compare, Alloc>>
              : std::false_type
            {};

            template<class Key, class Hash, class Pred, class Alloc>
            struct view_predicate_<std::unordered_set<Key, Hash, Pred, Alloc>>
              : std::false_type
            {};

            template<class Key, class Hash, class Pred, class Alloc>
            struct view_predicate_<std::unordered_multiset<Key, Hash, Pred, Alloc>>
              : std::false_type
            {};

            template<typename T, std::size_t N>
            struct view_predicate_<T[N]>
              : std::false_type
            {};
        }
        /// \endcond

        /// \addtogroup group-concepts
        /// @{

        // Specialize this if the default is wrong.
        template<typename T>
        using is_view
            RANGES_DEPRECATED("If you need to override the logic of the View concept, please use ranges::enable_view."
                              "Otherwise, please use the View concept directly.") =
                View<T>;

        /// @}
    }
}

#endif
