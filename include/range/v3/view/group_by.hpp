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

#ifndef RANGES_V3_VIEW_GROUP_BY_HPP
#define RANGES_V3_VIEW_GROUP_BY_HPP

#include <utility>
#include <type_traits>
#include <meta/meta.hpp>
#include <range/v3/detail/satisfy_boost_range.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/begin_end.hpp>
#include <range/v3/iterator_range.hpp>
#include <range/v3/range_traits.hpp>
#include <range/v3/range_concepts.hpp>
#include <range/v3/view_facade.hpp>
#include <range/v3/utility/functional.hpp>
#include <range/v3/utility/semiregular.hpp>
#include <range/v3/utility/static_const.hpp>
#include <range/v3/algorithm/adjacent_find.hpp>
#include <range/v3/view/view.hpp>
#include <range/v3/view/take_while.hpp>

namespace ranges
{
    inline namespace v3
    {
        // TODO group_by could support Input ranges by keeping mutable state in
        // the range itself. The group_by view would then be mutable-only and
        // Input.

        /// \addtogroup group-views
        /// @{
        template<typename Rng, typename Fun, bool = (bool) ForwardRange<Rng>()>
        struct group_by_view
          : view_facade<
                group_by_view<Rng, Fun>,
                is_finite<Rng>::value ? finite : range_cardinality<Rng>::value>
        {
        private:
            friend range_access;
            Rng rng_;
            semiregular_t<Fun> fun_;

            template<bool IsConst>
            struct cursor
            {
            private:
                friend range_access; friend group_by_view;
                iterator_t<Rng> cur_;
                sentinel_t<Rng> last_;
                semiregular_ref_or_val_t<Fun, IsConst> fun_;

                struct take_while_pred
                {
                    iterator_t<Rng> first_;
                    semiregular_ref_or_val_t<Fun, IsConst> fun_;
                    bool operator()(range_reference_t<Rng> ref) const
                    {
                        return invoke(fun_, *first_, ref);
                    }
                };
                take_while_view<
                    iterator_range<iterator_t<Rng>, sentinel_t<Rng>>,
                    take_while_pred>
                read() const
                {
                    return {{cur_, last_}, {cur_, fun_}};
                }
                void next()
                {
                    cur_ =
                        ranges::next(adjacent_find(cur_, last_, not_fn(std::ref(fun_))), 1, last_);
                }
                bool equal(default_sentinel) const
                {
                    return cur_ == last_;
                }
                bool equal(cursor const &that) const
                {
                    return cur_ == that.cur_;
                }
                cursor(semiregular_ref_or_val_t<Fun, IsConst> fun, iterator_t<Rng> first,
                    sentinel_t<Rng> last)
                  : cur_(first), last_(last), fun_(fun)
                {}
            public:
                cursor() = default;
            };
            cursor<false> begin_cursor()
            {
                return {fun_, ranges::begin(rng_), ranges::end(rng_)};
            }
            CONCEPT_REQUIRES(Invocable<Fun const&, range_common_reference_t<Rng>,
                range_common_reference_t<Rng>>() && Range<Rng const>())
            cursor<true> begin_cursor() const
            {
                return {fun_, ranges::begin(rng_), ranges::end(rng_)};
            }
        public:
            group_by_view() = default;
            group_by_view(Rng rng, Fun fun)
              : rng_(std::move(rng))
              , fun_(std::move(fun))
            {}
        };

        template<typename Rng, typename Fun>
        struct group_by_view<Rng, Fun, false>
          : view_facade<
                group_by_view<Rng, Fun>,
                is_finite<Rng>::value ? finite : range_cardinality<Rng>::value>
        {
        private:
            friend range_access;

            using Key = detail::decay_t<result_of_t<Fun&(range_reference_t<Rng>)>>;
            mutable compressed_tuple<
                Rng,                                            // base
                semiregular_t<Fun>,                             // projection
                semiregular_t<Key>,                             // key
                detail::non_propagating_cache<iterator_t<Rng>>, // it
                bool                                            // new_extent
            > data_{};

            RANGES_CXX14_CONSTEXPR
            Rng &base() noexcept { return ranges::get<0>(data_); }
            RANGES_CXX14_CONSTEXPR
            Rng const &base() const noexcept { return ranges::get<0>(data_); }
            RANGES_CXX14_CONSTEXPR
            Fun &projection() noexcept { return ranges::get<1>(data_); }
            RANGES_CXX14_CONSTEXPR
            Fun const &projection() const noexcept { return ranges::get<1>(data_); }
            RANGES_CXX14_CONSTEXPR
            Key &key() noexcept { return ranges::get<2>(data_); }
            RANGES_CXX14_CONSTEXPR
            Key const &key() const noexcept { return ranges::get<2>(data_); }
            RANGES_CXX14_CONSTEXPR
            detail::non_propagating_cache<iterator_t<Rng>> &cached_it() const { ranges::get<3>(data_); }
            RANGES_CXX14_CONSTEXPR
            iterator_t<Rng> &it() noexcept
            {
                auto &cache = cached_it();
                if(!cache)
                {
                    cache = ranges::begin(base());
                    key() = projection(**cache);
                    new_extent() = true;
                }
                return *cache;
            }
            RANGES_CXX14_CONSTEXPR
            iterator_t<Rng> const &it() const noexcept
            {
                return const_cast<group_by_view&>(*this).it();
            }
            RANGES_CXX14_CONSTEXPR
            bool &new_extent() noexcept { return ranges::get<4>(data_); }
            RANGES_CXX14_CONSTEXPR
            bool const &new_extent() const noexcept { return ranges::get<4>(data_); }

            RANGES_CXX14_CONSTEXPR
            bool done() const noexcept
            {
                return it() == ranges::end(base());
            }
            RANGES_CXX14_CONSTEXPR
            void satisfy()
            {
                RANGES_EXPECT(!done());
                ++it();
                if (done())
                {
                    new_extent() = true;
                    return;
                }
                Key new_key = projection(*it());
                new_extent() = new_key != key();
                key() = detail::move(new_key);
            }

            struct outer_cursor
            {
            private:
                group_by_view *rng_ = nullptr;

                struct inner_view
                  : view_facade<inner_view, is_finite<Rng>::value ? finite : unknown>
                {
                private:
                    friend range_access;
                    group_by_view *rng_ = nullptr;

                    RANGES_CXX14_CONSTEXPR
                    bool done() const noexcept
                    {
                        RANGES_EXPECT(rng_);
                        RANGES_ASSERT(!rng_->done() || rng_->new_extent());
                        return rng_->new_extent();
                    }
                    RANGES_CXX14_CONSTEXPR
                    range_reference_t<Rng> read() const
                    {
                        RANGES_EXPECT(!done());
                        return *rng_->it();
                    }
                    RANGES_CXX14_CONSTEXPR
                    void next()
                    {
                        RANGES_EXPECT(!done());
                        rng_->satisfy();
                    }
                    RANGES_CXX14_CONSTEXPR
                    bool equal(default_sentinel) const
                    {
                        return done();
                    }

                public:
                    inner_view() = default;
                    constexpr explicit inner_view(group_by_view &view) noexcept
                      : rng_(&view)
                    {}
                };
            public:
                outer_cursor() = default;
                constexpr explicit outer_cursor(group_by_view &view) noexcept
                  : rng_(&view)
                {}
                RANGES_CXX14_CONSTEXPR
                bool done() const noexcept
                {
                    RANGES_EXPECT(rng_);
                    return rng_->done();
                }
                RANGES_CXX14_CONSTEXPR
                inner_view read() const
                {
                    RANGES_EXPECT(!done());
                    rng_->new_extent() = false;
                    return inner_view{*rng_};
                }
                RANGES_CXX14_CONSTEXPR
                void next()
                {
                    RANGES_EXPECT(rng_);
                    while(!rng_->new_extent()) {
                        rng_->satisfy();
                    }
                }
                RANGES_CXX14_CONSTEXPR
                bool equal(default_sentinel) const
                {
                    return done();
                }
            };
            RANGES_CXX14_CONSTEXPR
            outer_cursor begin_cursor()
            {
                return outer_cursor{*this};
            }
        public:
            group_by_view() = default;
            constexpr group_by_view(Rng rng, Fun fun)
              : data_{std::move(rng), std::move(fun), semiregular_t<Key>{}, nullopt, true}
            {}
        };

        namespace view
        {
            struct group_by_fn
            {
            private:
                friend view_access;
                template<typename Fun>
                static auto bind(group_by_fn group_by, Fun fun)
                RANGES_DECLTYPE_AUTO_RETURN
                (
                    make_pipeable(std::bind(group_by, std::placeholders::_1, std::move(fun)))
                )
            public:
                template<typename Rng, typename Fun>
                using Constraint = meta::or_<
                    meta::and_<ForwardRange<Rng>, IndirectRelation<Fun, iterator_t<Rng>>>,
                    meta::and_<InputRange<Rng>, IndirectRelation<Fun, iterator_t<Rng>>>>; // FIXME

                template<typename Rng, typename Fun,
                    CONCEPT_REQUIRES_(Constraint<Rng, Fun>())>
                group_by_view<all_t<Rng>, Fun> operator()(Rng && rng, Fun fun) const
                {
                    return {all(static_cast<Rng&&>(rng)), std::move(fun)};
                }

            #ifndef RANGES_DOXYGEN_INVOKED
                template<typename Rng, typename Fun,
                    CONCEPT_REQUIRES_(!Constraint<Rng, Fun>())>
                void operator()(Rng &&, Fun) const
                { // FIXME
                    CONCEPT_ASSERT_MSG(ForwardRange<Rng>(),
                        "The object on which view::group_by operates must be a model of the "
                        "ForwardRange concept.");
                    CONCEPT_ASSERT_MSG(IndirectRelation<Fun, iterator_t<Rng>>(),
                        "The function passed to view::group_by must be callable with two arguments "
                        "of the range's common reference type, and its return type must be "
                        "convertible to bool.");
                }
            #endif
            };

            /// \relates group_by_fn
            /// \ingroup group-views
            RANGES_INLINE_VARIABLE(view<group_by_fn>, group_by)
        }
        /// @}
    }
}

RANGES_SATISFY_BOOST_RANGE(::ranges::v3::group_by_view)

#endif
