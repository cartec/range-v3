/// \file
// Range v3 library
//
//  Copyright Casey Carter 2017
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3
//

#ifndef RANGES_V3_EXPERIMENTAL_VIEW_SORTED_HPP
#define RANGES_V3_EXPERIMENTAL_VIEW_SORTED_HPP

#include <range/v3/range_fwd.hpp>
#include <range/v3/view_adaptor.hpp>
#include <range/v3/algorithm/heap_algorithm.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/utility/compressed_pair.hpp>
#include <range/v3/utility/semiregular.hpp>
#include <range/v3/view/all.hpp>
#include <range/v3/view/reverse.hpp>

namespace ranges
{
    inline namespace v3
    {
        namespace detail
        {
            template<typename Comp>
            struct flipped
              : private box<Comp, flipped<Comp>>
            {
                flipped() = default;
                constexpr flipped(Comp comp)
                    noexcept(std::is_nothrow_move_constructible<Comp>::value)
                  : box<Comp, flipped<Comp>>{std::move(comp)}
                {}

                Comp const &base() const noexcept
                {
                    return this->box<Comp, flipped<Comp>>::get();
                }

                template<typename T, typename U>
                auto operator()(T &&t, U &&u) & ->
                    decltype(std::declval<Comp &>()(std::declval<U>(), std::declval<T>()))
                {
                    return this->box<Comp, flipped<Comp>>::get()(
                        static_cast<U &&>(u), static_cast<T &&>(t));
                }
                template<typename T, typename U>
                auto operator()(T &&t, U &&u) const& ->
                    decltype(std::declval<Comp const&>()(std::declval<U>(), std::declval<T>()))
                {
                    return this->box<Comp, flipped<Comp>>::get()(
                        static_cast<U &&>(u), static_cast<T &&>(t));
                }
                template<typename T, typename U>
                auto operator()(T &&t, U &&u) && ->
                    decltype(std::declval<Comp &&>()(std::declval<U>(), std::declval<T>()))
                {
                    return std::move(this->box<Comp, flipped<Comp>>::get())(
                        static_cast<U &&>(u), static_cast<T &&>(t));
                }
                template<typename T, typename U>
                void operator()(T &&, U &&) const&& = delete;
            };
        }

        namespace experimental
        {
            template<typename Rng, typename Comp, typename Proj>
            struct sorted_view
              : view_adaptor<sorted_view<Rng, Comp, Proj>, Rng, range_cardinality<Rng>::value>
            {
                CONCEPT_ASSERT(RandomAccessView<Rng>() && BoundedRange<Rng>() &&
                    Sortable<iterator_t<Rng>, Comp, Proj>());

                sorted_view() = default;
                RANGES_CXX14_CONSTEXPR sorted_view(Rng rng, Comp comp, Proj proj)
                  : sorted_view::view_adaptor{std::move(rng)}
                  , data_{-1, std::move(comp), std::move(proj)}
                {}
                RANGES_CXX14_CONSTEXPR range_size_type_t<Rng> size()
                    noexcept(noexcept(ranges::size(std::declval<Rng &>())))
                {
                    return ranges::size(this->base());
                }
                template<typename R = Rng const,
                    CONCEPT_REQUIRES_(SizedRange<R>())>
                constexpr range_size_type_t<R> size() const
                    noexcept(noexcept(ranges::size(std::declval<R &>())))
                {
                    return ranges::size(this->base());
                }

            private:
                friend range_access;

                compressed_tuple<
                    difference_type_t<iterator_t<Rng>>,
                    semiregular_t<detail::flipped<Comp>>,
                    semiregular_t<Proj>> data_{};

                difference_type_t<iterator_t<Rng>> &len() noexcept
                {
                    return get<0>(data_);
                }
                detail::flipped<Comp> &comp() noexcept
                {
                    return get<1>(data_);
                }
                Proj &proj() noexcept
                {
                    return get<2>(data_);
                }

                struct adaptor
                  : adaptor_base
                {
                    adaptor() = default;
                    constexpr adaptor(sorted_view &rng) noexcept
                      : rng_{&rng}
                    {}
                    RANGES_CXX14_CONSTEXPR
                    iterator_t<Rng> begin(sorted_view &rng)
                    {
                        RANGES_EXPECT(&rng == rng_);
                        auto first = ranges::begin(rng.base());
                        if (rng.len() < 0)
                        {
                            rng.len() = first != ranges::end(rng.base());
                            if (rng.len())
                            {
                                // worst case O(N), "amortized" O(1)
                                auto rev = view::reverse(rng.base());
                                // establish invariant: view::reverse([begin + rng.len(), end)) is a heap.
                                ranges::make_heap(rev, rng.comp(), rng.proj());
                                // establish invariant: [begin, rng.len()) is sorted.
                                ranges::pop_heap(rev, rng.comp(), rng.proj());
                            }
                        }
                        return first;
                    }
                    RANGES_CXX14_CONSTEXPR
                    void next(iterator_t<Rng> &it)
                    {
                        auto &base = rng_->base();
                        auto const first = ranges::begin(base);
                        auto &len = rng_->len();
                        auto const bound = first + len;
                        RANGES_EXPECT(it < bound);
                        if (++it == bound)
                        {
                            auto const last = ranges::end(base);
                            auto rev = make_iterator_range(bound, last) | view::reverse;
                            // *** O(lg N) *** but O(1) on repeat passes
                            ranges::pop_heap(rev, rng_->comp(), rng_->proj());
                            auto const size = last - first;
                            if (++len + 1 >= size)
                            {
                                // Everything is sorted; move len *past* the end of
                                // the range so valid iterators never again reach it.
                                len = size + 1;
                            }
                        }
                    }
                    RANGES_CXX14_CONSTEXPR
                    void advance(iterator_t<Rng> &it, range_difference_type_t<Rng> n)
                    {
                        // |** sorted elements **|** unsorted elements **|
                        // ^-first    it-^       ^- first + len     last-^
                        // Advancing it past first + len requires us to sort the
                        // intervening elements.

                        auto &base = rng_->base();
                        RANGES_ASSERT(n <= ranges::end(base) - it);
                        auto const first = ranges::begin(base);
                        auto const i = it - first;
                        RANGES_EXPECT(n >= -i);

                        auto const target = i + n;
                        auto &len = rng_->len();
                        if (len <= target)
                        {
                            auto &comp = rng_->comp();
                            auto &proj = rng_->proj();
                            auto const size = last - first;
                            while (len <= target) // *** O(n lg N) ***
                            {
                                auto rev = make_iterator_range(first + len, last) | view::reverse;
                                ranges::pop_heap(rev, comp, proj);

                                ++len;
                            }
                            if (len >= size)
                            {
                                // Everything is sorted; move len *past* the end of
                                // the range so valid iterators never again reach it.
                                len = size + 1;
                                break;
                            }
                        }

                        it += n; // O(1)
                    }
                private:
                    sorted_view *rng_ = nullptr;
                };

                RANGES_CXX14_CONSTEXPR
                adaptor begin_adaptor() noexcept
                {
                    return adaptor{*this};
                }
            };

            namespace view
            {
                struct sorted_fn
                {
                    template<typename Rng, typename C = ordered_less, typename P = ident,
                        CONCEPT_REQUIRES_(RandomAccessRange<Rng>() && BoundedRange<Rng>() &&
                            Sortable<iterator_t<Rng>, C, P>())>
                    RANGES_CXX14_CONSTEXPR sorted_view<ranges::view::all_t<Rng>, C, P>
                    operator()(Rng &&rng, C comp = C{}, P proj = P{}) const
                    RANGES_AUTO_RETURN_NOEXCEPT
                    (
                        sorted_view<ranges::view::all_t<Rng>, C, P>{
                            ranges::view::all(static_cast<Rng &&>(rng)),
                            std::move(comp), std::move(proj)}
                    )
                };

                /// \relates sorted_fn
                /// \ingroup group-views
                RANGES_INLINE_VARIABLE(ranges::view::view<sorted_fn>, sorted)
            } // namespace view
        } // namespace experimental
    } // namespace v3
} // namespace ranges

#endif
