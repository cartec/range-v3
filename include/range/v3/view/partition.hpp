/// \file
// Range v3 library
//
//  Copyright Casey Carter 2018
//  Copyright Eric Niebler 2018
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3
//
#ifndef RANGE_V3_VIEW_PARTITION_HPP
#define RANGE_V3_VIEW_PARTITION_HPP

#include <tuple>
#include <meta/meta.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/view_adaptor.hpp>
#include <range/v3/utility/iterator_concepts.hpp>
#include <range/v3/view/view.hpp>

namespace ranges
{
    inline namespace v3
    {
        namespace detail
        {
            template<typename Rng, typename Out, typename Pred, typename Proj>
            using PartitionConstraint = meta::and_<
                InputRange<Rng>, WeaklyIncrementable<Out>, IndirectlyCopyable<iterator_t<Rng>, Out>,
                IndirectPredicate<Pred &, projected<iterator_t<Rng>, Proj>>>;
        }

        template<typename Rng, typename Out, typename Pred, typename Proj = ident>
        struct partition_view
          : view_adaptor<
                partition_view<Rng, Out, Pred, Proj>, Rng,
                is_finite<Rng>::value ? finite : range_cardinality<Rng>::value>
        {
            friend range_access;

            CONCEPT_ASSERT(detail::PartitionConstraint<Rng, Out, Pred, Proj>());

            partition_view() = default;
            RANGES_CXX14_CONSTEXPR partition_view(Rng rng, Out out, Pred pred, Proj proj = Proj{})
              : partition_view::view_adaptor{std::move(rng)}
              , data_(std::move(out), std::move(pred), std::move(proj))
            {}

        private:
            struct adaptor : adaptor_base
            {
                using single_pass = std::true_type;

                adaptor() = default;
                adaptor(partition_view &pv)
                  : pv_{&pv}
                {}
                iterator_t<Rng> begin(partition_view &pv)
                {
                    auto it = ranges::begin(pv.base());
                    pv_->satisfy(it);
                    return it;
                }
                void next(iterator_t<Rng> &it)
                {
                    RANGES_EXPECT(it != ranges::end(pv_->base()));
                    pv_->satisfy(++it);
                }
                void prev() = delete;

            private:
                partition_view *pv_;
            };

            adaptor begin_adaptor()
            {
                return adaptor{*this};
            }

            void satisfy(iterator_t<Rng> &it)
            {
                auto &out = std::get<0>(data_);
                auto &pred = static_cast<Pred &>(std::get<1>(data_));
                auto &proj = static_cast<Proj &>(std::get<2>(data_));
                for (auto const last = ranges::end(this->base()); it != last; ++it)
                {
                    auto &&value = *it;
                    if (!invoke(pred, invoke(proj, value))) break;
                    out = static_cast<decltype(value) &&>(value);
                    ++out;
                }
            }

            std::tuple<Out, semiregular_t<Pred>, semiregular_t<Proj>> data_{};
        };

        namespace view
        {
            struct partition_fn
            {
            private:
                friend view_access;

                template<typename Out, typename Pred, typename Proj = ident,
                    CONCEPT_REQUIRES_(WeaklyIncrementable<Out>() &&
                        CopyConstructible<Pred>() && CopyConstructible<Proj>())>
                static RANGES_CXX14_CONSTEXPR auto
                bind(partition_fn fn, Out out, Pred pred, Proj proj = Proj{})
                RANGES_DECLTYPE_AUTO_RETURN
                (
                    make_pipeable(std::bind(fn, std::placeholders::_1, out, pred, proj))
                )
            public:
                template<typename Rng, typename Out, typename Pred, typename Proj = ident,
                    CONCEPT_REQUIRES_(detail::PartitionConstraint<Rng, Out, Pred, Proj>())>
                RANGES_CXX14_CONSTEXPR partition_view<all_t<Rng>, Out, Pred, Proj>
                operator()(Rng &&rng, Out out, Pred pred, Proj proj = Proj{}) const
                {
                    return {all(static_cast<Rng &&>(rng)),
                        std::move(out), std::move(pred), std::move(proj)};
                }

#ifndef RANGES_DOXYGEN_INVOKED
                template<typename Rng, typename Out, typename Pred, typename Proj = ident,
                    CONCEPT_REQUIRES_(!detail::PartitionConstraint<Rng, Out, Pred, Proj>())>
                void operator()(Rng &&, Out, Pred, Proj = Proj{}) const
                {
                    static_assert(InputRange<Rng>(),
                        "");
                    static_assert(detail::PartitionConstraint<Rng, Out, Pred, Proj>(),
                        "FIXME");
                }
#endif
            };

            /// \relates partition_fn
            /// \ingroup group-views
            RANGES_INLINE_VARIABLE(view<partition_fn>, partition)
        } // namespace view
    } // namespace v3
} // namespace ranges

#endif // RANGE_V3_VIEW_PARTITION_HPP
