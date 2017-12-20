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

#ifndef RANGES_V3_VIEW_LOW_PASS_HPP
#define RANGES_V3_VIEW_LOW_PASS_HPP

#include <functional>
#include <utility>
#include <vector>
#include <meta/meta.hpp>
#include <range/v3/begin_end.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/range_concepts.hpp>
#include <range/v3/range_traits.hpp>
#include <range/v3/view_adaptor.hpp>
#include <range/v3/view_facade.hpp>
#include <range/v3/utility/optional.hpp>
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
            using LowPassConstraint2 =
                SemiRegular<range_value_type_t<Rng>>;
                // TODO: complete this constraint.
                // can-add-and-subtract-range_value_type_t<Rng> &&
                // can-divide-range_value_type_t<Rng>-by-range_difference_type_t<Rng>
            template<typename Rng>
            using LowPassConstraint = meta::and_<
                InputRange<Rng>,
                meta::defer<LowPassConstraint2, Rng>>;
        } // namespace detail

        template<typename Rng, typename = void>
        struct low_pass_view
          : view_adaptor<low_pass_view<Rng>, Rng,
                (range_cardinality<Rng>::value > 0)
                    ? finite
                    : range_cardinality<Rng>::value>
        {
            CONCEPT_ASSERT(detail::LowPassConstraint<Rng>() && ForwardView<Rng>());

            low_pass_view() = default;
            RANGES_CXX14_CONSTEXPR
            explicit low_pass_view(Rng rng, range_difference_type_t<Rng> n)
              : low_pass_view::view_adaptor{std::move(rng)}
              , data_{begin_cache_t{}, begin_sum_t{}, end_cache_t{},
                    end_sum_t{}, n}
            {
                RANGES_EXPECT(n > 0);
            }

            CONCEPT_REQUIRES(SizedRange<Rng>())
            range_size_type_t<Rng> size()
            {
                return size_(ranges::distance(this->base()));
            }
            CONCEPT_REQUIRES(SizedRange<Rng const>())
            range_size_type_t<Rng> size() const
            {
                return size_(ranges::distance(this->base()));
            }

        private:
            friend range_access;

            static constexpr bool is_symmetric =
                BidirectionalRange<Rng>() && BoundedRange<Rng>();

            struct adaptor
              : adaptor_base
              , private meta::if_<
                    RandomAccessRange<Rng>,
                    meta::nil_, box<iterator_t<Rng>, low_pass_view<Rng>>>
            {
                adaptor() = default;
                explicit adaptor(low_pass_view &rng) noexcept
                  : count_{rng.count()}
#ifndef NDEBUG
                  , rng_{&rng}
#endif
                {}
                iterator_t<Rng> begin(low_pass_view &rng)
                {
                    return begin_(rng, iterator_concept<iterator_t<Rng>>{});
                }
                iterator_t<Rng> end(low_pass_view &rng)
                {
                    return end_(rng, iterator_concept<iterator_t<Rng>>{});
                }
                range_value_type_t<Rng> read(iterator_t<Rng> const &it) const
                {
                    RANGES_ASSERT(it != ranges::end(rng_->base()));
                    return (sum_ + *it) / count_;
                }
                void next(iterator_t<Rng> &it)
                {
                    RANGES_ASSERT(it != ranges::end(rng_->base()));
                    next_trailing(it, iterator_concept<iterator_t<Rng>>{});
                    sum_ += *it;
                    ++it;
                }
                CONCEPT_REQUIRES(BidirectionalRange<Rng>())
                void prev(iterator_t<Rng> &it)
                {
                    prev_trailing(it, iterator_concept<iterator_t<Rng>>{});
                    sum_ -= *--it;
                }
                void advance() = delete; // This adaptor never provides random access
            private:
                iterator_t<Rng> begin_(low_pass_view &rng, concepts::RandomAccessIterator*)
                {
                    auto first = ranges::begin(rng.base());
                    if (rng.begin_sum())
                    {
                        sum_ = *rng.begin_sum();
                        return first + (rng.count() - 1);
                    }

                    auto const last = ranges::end(rng.base());
                    for (auto i = rng.count(); --i > 0 && first != last; ++first)
                        sum_ += *first;

                    rng.begin_sum().emplace(sum_);
                    return first;
                }
                iterator_t<Rng> begin_(low_pass_view &rng, concepts::ForwardIterator*)
                {
                    auto first = ranges::begin(rng.base());
                    trailing() = first;
                    if (rng.begin_cache())
                    {
                        sum_ = rng.begin_sum();
                        return *rng.begin_cache();
                    }

                    auto const last = ranges::end(rng.base());
                    for (auto i = rng.count(); --i > 0 && first != last; ++first)
                        sum_ += *first;

                    rng.begin_sum() = sum_;
                    rng.begin_cache().emplace(first);
                    return first;
                }

                iterator_t<Rng> end_(low_pass_view &rng, concepts::RandomAccessIterator*)
                {
                    auto /* const */ last = ranges::end(rng.base());
                    if (rng.end_sum())
                    {
                        sum_ = *rng.end_sum();
                    return last;
                    }

                        auto const first = ranges::begin(rng.base());
                        auto pos = last;
                        for (auto i = rng.count(); --i > 0 && first != pos;)
                        sum_ += *--pos;
                    rng.end_sum().emplace(sum_);
                    return last;
                }
                iterator_t<Rng> end_(low_pass_view &rng, concepts::BidirectionalIterator*)
                {
                    auto /* const */ last = ranges::end(rng.base());
                    if (rng.end_cache())
                    {
                        trailing() = *rng.end_cache();
                        sum_ = rng.end_sum();
                        return last;
                    }

                    auto const first = ranges::begin(rng.base());
                    auto pos = last;
                    for (auto i = rng.count(); --i > 0 && first != pos;)
                        sum_ += *--pos;
                    rng.end_sum() = sum_;
                    rng.end_cache().emplace(pos);
                    trailing() = pos;
                    return last;
                }

                void next_trailing(iterator_t<Rng> const &, concepts::ForwardIterator*)
                {
                    auto &trail = trailing();
                    sum_ -= *trail;
                    ++trail;
                }
                void next_trailing(iterator_t<Rng> const &it, concepts::RandomAccessIterator*)
                {
                    auto trail = it - (count_ - 1);
                    sum_ -= *trail;
                }

                void prev_trailing(iterator_t<Rng> const &, concepts::BidirectionalIterator*)
                {
                    auto &trail = trailing();
                    RANGES_ASSERT(trail != ranges::begin(rng_->base()));
                    sum_ += *--trail;
                }
                void prev_trailing(iterator_t<Rng> const &it, concepts::RandomAccessIterator*)
                {
                    auto trail = it - (count_ - 1);
                    RANGES_ASSERT(trail != ranges::begin(rng_->base()));
                    sum_ += *--trail;
                }

                iterator_t<Rng> &trailing() noexcept { return adaptor::box::get(); }

                range_difference_type_t<Rng> count_ = 0;
                range_value_type_t<Rng> sum_{}; // sum of the previous (count - 1) elements
#ifndef NDEBUG
                low_pass_view *rng_ = nullptr;
#endif
            };

            adaptor begin_adaptor() { return adaptor{*this}; }
            CONCEPT_REQUIRES(is_symmetric)
            adaptor end_adaptor() { return adaptor{*this}; }
            CONCEPT_REQUIRES(!is_symmetric)
            adaptor_base end_adaptor() { return {}; }

            range_size_type_t<Rng> size_(range_difference_type_t<Rng> const n) const
            {
                auto result = n - count() + 1;
                return static_cast<range_size_type_t<Rng>>(result < 0 ? 0 : result);
            }

            using begin_cache_t = detail::non_propagating_cache<iterator_t<Rng>,
                meta::list<low_pass_view, meta::size_t<0>>,
                !RandomAccessRange<Rng>()>;
            using begin_sum_t = meta::if_c<(bool) RandomAccessRange<Rng>(),
                optional<range_value_type_t<Rng>>,
                range_value_type_t<Rng>>;
            using end_cache_t = detail::non_propagating_cache<iterator_t<Rng>,
                meta::list<low_pass_view, meta::size_t<1>>,
                is_symmetric && !RandomAccessRange<Rng>()>;
            using end_sum_t = meta::if_c<is_symmetric,
                meta::if_c<(bool) RandomAccessRange<Rng>(),
                    optional<range_value_type_t<Rng>>,
                    range_value_type_t<Rng>>,
                meta::nil_>;

            compressed_tuple<
                begin_cache_t,               // begin + (count() - 1)
                begin_sum_t,                 // sum of the first (count() - 1) elements
                end_cache_t,                 // end - (count() - 1)
                end_sum_t,                   // sum of the last (count() - 1) elements
                range_difference_type_t<Rng> // count()
            > data_;

            begin_cache_t &begin_cache() noexcept { return get<0>(data_); }
            begin_sum_t &begin_sum() noexcept { return get<1>(data_); }

            end_cache_t &end_cache() noexcept { return get<2>(data_); }
            end_sum_t &end_sum() noexcept { return get<3>(data_); }

            range_difference_type_t<Rng> count() const noexcept { return get<4>(data_); }
        };

        template<typename Rng>
        struct low_pass_view<Rng, meta::if_c<!ForwardRange<Rng>()>>
            : view_facade<low_pass_view<Rng>,
                (range_cardinality<Rng>::value > 0)
                    ? finite
                    : range_cardinality<Rng>::value>
        {
            CONCEPT_ASSERT(detail::LowPassConstraint<Rng>() && InputView<Rng>());

            low_pass_view() = default;
            explicit RANGES_CXX14_CONSTEXPR low_pass_view(Rng rng, range_difference_type_t<Rng> n)
              : data_{std::move(rng), iterator_t<Rng>{}, std::vector<range_value_type_t<Rng>>{},
                    0, n, range_value_type_t<Rng>{}}
            {
                RANGES_EXPECT(n > 0);
            }

            CONCEPT_REQUIRES(SizedRange<Rng>())
            range_size_type_t<Rng> size()
            {
                return size_(ranges::distance(rng()));
            }
            CONCEPT_REQUIRES(SizedRange<Rng const>())
            range_size_type_t<Rng> size() const
            {
                return size_(ranges::distance(rng()));
            }

            Rng &base() noexcept { return rng(); }
            Rng const &base() const noexcept { return rng(); }
        private:
            friend range_access;

            struct cursor
            {
                cursor() = default;
                explicit cursor(low_pass_view &rng) noexcept
                  : rng_{&rng}
                {}
                range_value_type_t<Rng> read() const
                {
                    auto const &pos = rng_->pos();
                    RANGES_ASSERT(pos != ranges::end(rng_->rng()));
                    return (rng_->sum() + *pos) / rng_->count();
                }
                void next()
                {
                    auto &pos = rng_->pos();
                    RANGES_ASSERT(pos != ranges::end(rng_->rng()));
                    auto &index = rng_->index();
                    auto &element = rng_->stored_data()[index];
                    if (++index >= rng_->count() - 1)
                        index = 0;
                    auto &sum = rng_->sum();
                    sum -= element;
                    element = *pos;
                    ++pos;
                    sum += element;
                }
                bool equal(default_sentinel) const
                {
                    return rng_->pos() == ranges::end(rng_->rng());
                }
                CONCEPT_REQUIRES(SizedSentinel<sentinel_t<Rng>, iterator_t<Rng>>())
                range_difference_type_t<Rng> distance_to(default_sentinel) const
                {
                    return ranges::end(rng_->rng()) - rng_->pos();
                }
            private:
                low_pass_view *rng_ = nullptr;
            };

            cursor begin_cursor()
            {
                auto &vec = get<2>(data_);
                auto const n = count();
                vec.reserve(static_cast<std::size_t>(n) - 1);
                auto &first = pos();
                first = ranges::begin(rng());
                auto const last = ranges::end(rng());
                for (auto i = 0; i < n - 1 && first != last; ++i, (void)++first)
                {
                    vec.emplace_back(*first);
                    sum() += vec.back();
                }
                pos() = first;
                return cursor{*this};
            }

            range_size_type_t<Rng> size_(range_difference_type_t<Rng> const n) const
            {
                auto result = n - count() + 1;
                return static_cast<range_size_type_t<Rng>>(result < 0 ? 0 : result);
            }

            compressed_tuple<
                Rng,                                  // Base range
                iterator_t<Rng>,                      // position in range
                std::vector<range_value_type_t<Rng>>, // Most recent (count() - 1) elements
                range_difference_type_t<Rng>,         // current index into the above vector
                range_difference_type_t<Rng>,         // count()
                range_value_type_t<Rng>               // sum of the most recent (count() - 1) elements
            > data_{};

            Rng &rng() noexcept { return get<0>(data_); }
            Rng const &rng() const noexcept { return get<0>(data_); }

            iterator_t<Rng> &pos() noexcept { return get<1>(data_); }

            range_value_type_t<Rng> *stored_data() noexcept { return get<2>(data_).data(); }

            range_difference_type_t<Rng> &index() noexcept { return get<3>(data_); }

            range_difference_type_t<Rng> count() const noexcept { return get<4>(data_); }

            range_value_type_t<Rng> &sum() noexcept { return get<5>(data_); }
            range_value_type_t<Rng> const &sum() const noexcept { return get<5>(data_); }
        };

        namespace view
        {
            struct low_pass_fn
            {
            private:
                friend view_access;
                template<typename Int,
                    CONCEPT_REQUIRES_(Integral<Int>())>
                static auto bind(low_pass_fn f, Int n)
                RANGES_DECLTYPE_AUTO_RETURN
                (
                    make_pipeable(std::bind(f, std::placeholders::_1, n))
                )
            public:
                template<typename Rng,
                    CONCEPT_REQUIRES_(detail::LowPassConstraint<Rng>())>
                RANGES_CXX14_CONSTEXPR low_pass_view<all_t<Rng>>
                operator()(Rng &&rng, range_difference_type_t<Rng> n) const
                {
                    return low_pass_view<all_t<Rng>>{all(static_cast<Rng &&>(rng)), n};
                }

                // For the sake of better error messages:
            #ifndef RANGES_DOXYGEN_INVOKED
            private:
                template<typename Int,
                    CONCEPT_REQUIRES_(!Integral<Int>())>
                static detail::null_pipe bind(low_pass_fn, Int)
                {
                    CONCEPT_ASSERT_MSG(Integral<Int>(),
                        "The object passed to view::low_pass must satisfy the Integral concept");
                    return {};
                }
            public:
                template<typename Rng, typename T,
                    CONCEPT_REQUIRES_(!(detail::LowPassConstraint<Rng>() && Integral<T>()))>
                void operator()(Rng &&, T) const
                {
                    CONCEPT_ASSERT_MSG(InputRange<Rng>(),
                        "The first argument to view::low_pass must satisfy the InputRange concept");
                    CONCEPT_ASSERT_MSG(SemiRegular<range_value_type_t<Rng>>(),
                        "The value type of the first argument to view::low_pass must satisfy the SemiRegular concept");
                    CONCEPT_ASSERT_MSG(Integral<T>(),
                        "The second argument to view::low_pass must satisfy the Integral concept");
                }
            #endif
            };

            /// \relates low_pass_fn
            /// \ingroup group-views
            RANGES_INLINE_VARIABLE(view<low_pass_fn>, low_pass)
        } // namespace view
        /// @}
    } // namespace v3
} // namespace ranges

#endif
