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
                  , data_{ranges::begin(this->base()), std::move(comp), std::move(proj)}
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
                    iterator_t<Rng>,
                    semiregular_t<detail::flipped<Comp>>,
                    semiregular_t<Proj>> data_{};

                iterator_t<Rng> &bound() noexcept
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
                        auto &base = rng.base();
                        auto &bound = rng.bound();
                        auto first = ranges::begin(base);
                        auto const last = ranges::end(base);
                        if (first == bound && first != last)
                        {
                            auto rev = view::reverse(base);
                            auto &comp = rng.comp();
                            auto &proj = rng.proj();
                            // worst case O(N), "amortized" O(1)
                            ranges::make_heap(rev, comp, proj);
                            ranges::pop_heap(rev, comp, proj);
                            ranges::advance(bound, 1, last);
                        }
                        return first;
                    }
                    RANGES_CXX14_CONSTEXPR
                    void next(iterator_t<Rng> &it)
                    {
                        auto const end = ranges::end(rng_->base());
                        RANGES_EXPECT(it != end);
                        auto &bound = rng_->bound();
                        if (++it == bound && bound != end)
                        {
                            auto rev = make_iterator_range(bound, end) | view::reverse;
                            // *** O(lg N) *** but O(1) on repeat passes
                            ranges::pop_heap(rev, rng_->comp(), rng_->proj());
                            ++bound;
                        }
                    }
                    RANGES_CXX14_CONSTEXPR
                    void advance(iterator_t<Rng> &it, range_difference_type_t<Rng> n)
                    {
                        auto const end = ranges::end(rng_->base());
                        RANGES_EXPECT(n <= end - it);

                        auto &bound = rng_->bound();
                        if (bound == end || n < bound - it)
                        {
                            // O(1) on repeat passes
                            it += n;
                            return;
                        }

                        if (n == end - it)
                        {
                            // *** O(n lg N) ***
                            bound = ranges::sort(bound, end, rng_->comp().base(), rng_->proj());
                            it = bound;
                            return;
                        }

                        auto d = bound - it;
                        it += d - 1;
                        n -= d - 1;

                        // *** O(n lg N) ***
                        for (; n > 0; --n)
                            this->next(it);
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
