// Range v3 library
//
//  Copyright Tobias Mayer 2016
//  Copyright Casey Carter 2016-2017
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3

#include <forward_list>
#include <list>
#include <vector>
#include <range/v3/core.hpp>
#include <range/v3/view/cycle.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/low_pass.hpp>
#include <range/v3/view/repeat.hpp>
#include <range/v3/view/repeat_n.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/take_exactly.hpp>
#include <range/v3/view/transform.hpp>

RANGES_DIAGNOSTIC_IGNORE_FLOAT_CONVERSION
RANGES_DIAGNOSTIC_IGNORE_FLOAT_EQUAL

#include "../simple_test.hpp"
#include "../test_utils.hpp"

using namespace ranges;


namespace
{
    constexpr auto N = 7;
    constexpr auto K = 3;
    CONCEPT_ASSERT(K < N);

    template<typename Adapted>
    void test_size(Adapted& a, std::true_type)
    {
        CONCEPT_ASSERT(SizedRange<Adapted>());
        CHECK(size(a), unsigned(N - K + 1));
    }
    template<typename Adapted>
    void test_size(Adapted&, std::false_type)
    {}

    template<typename Adapted>
    void test_prev(Adapted&, iterator_t<Adapted> const& it, std::true_type)
    {
        ::check_equal(*ranges::prev(it, 3), N - K - 1);
    }
    template<typename Adapted>
    void test_prev(Adapted&, sentinel_t<Adapted> const&, std::false_type)
    {}

    template<typename Rng,
        bool = RandomAccessRange<Rng>() || (BidirectionalRange<Rng>() && BoundedRange<Rng>())>
    struct size_compare
    {
        iterator_t<Rng> iter1_;
        iterator_t<Rng> iter2_;
        range_difference_type_t<Rng> dist_;
    };
    template<typename Rng>
    struct size_compare<Rng, true>
    {
        iterator_t<Rng> iter1_;
        range_difference_type_t<Rng> dist_;
    };

    template<typename Base>
    void test_finite()
    {
        Base v = view::indices(N);
        auto rng = view::low_pass(v, K);
        using Adapted = decltype(rng);
        test_size(rng, SizedRange<Base>());

        CONCEPT_ASSERT(!RandomAccessRange<Base>() || BidirectionalRange<Adapted>());
        CONCEPT_ASSERT(RandomAccessRange<Base>() ||
            Same<iterator_concept_t<iterator_t<Base>>,
                 iterator_concept_t<iterator_t<Adapted>>>());
        CONCEPT_ASSERT(!(BoundedRange<Base>() && BidirectionalRange<Base>()) ||
            BoundedRange<Adapted>());

        ::check_equal(rng, view::iota(1, N - K + 2));

        test_prev(rng, ranges::end(rng),
            meta::bool_<BoundedRange<Base>() && BidirectionalRange<Base>()>{});
    }

    template<int N, int K>
    void test_cyclic()
    {
        // An infinite, cyclic range with cycle length > K
        CONCEPT_ASSERT(N > K);
        auto rng = view::indices(N) | view::cycle | view::low_pass(K);
        //[0,1,2],[1,2,3],[2,3,4],[3,4,5],[4,5,6],[5,6,0],[6,0,1],...
        using R = decltype(rng);
        CONCEPT_ASSERT(BidirectionalView<R>());
        CONCEPT_ASSERT(!BoundedRange<R>());
        auto it = rng.begin();
        CONCEPT_ASSERT(BidirectionalIterator<decltype(it)>());
        for (int k = 0; k < 42; ++k) {
            for (int i = 0; i <= N - K; ++i, ++it) {
                CHECK(*it == i + (K - 1) / 2);
            }
            for (int i = 1; i < K; ++i, ++it) {
                int sum = 0;
                for (int j = 0; j < K - i; ++j)
                    sum += N - K + j + i;
                for (int j = 0; j < i; ++j)
                    sum += j;
                CHECK(*it == sum / K);
            }
        }
    }
}

int main()
{
    test_finite<std::forward_list<int>>();
    test_finite<std::list<int>>();
    test_finite<std::vector<int>>();
    test_finite<decltype(view::indices(N))>();

    {
        // An infinite, cyclic range with cycle length == 1
        auto rng = view::repeat(42) | view::low_pass(K);
        using R = decltype(rng);
        CONCEPT_ASSERT(BidirectionalView<R>());
        CONCEPT_ASSERT(!BoundedRange<R>());
        ::check_equal(rng | view::take_exactly(2 * N), view::repeat_n(42, 2 * N));
        auto it = next(begin(rng), 42);
        CHECK(it == begin(rng));
        CHECK(*it == 42);
    }

    {
        // An infinite, cyclic range with cycle length == K
        auto rng = view::indices(K) | view::cycle |
            view::transform(convert_to<double>{}) | view::low_pass(K);
        using R = decltype(rng);
        CONCEPT_ASSERT(BidirectionalView<R>());
        CONCEPT_ASSERT(!BoundedRange<R>());
        ::check_equal(rng | view::take_exactly(42),
            view::repeat_n((K - 1) / 2.0, 42));
    }

    test_cyclic<N, K>();
    test_cyclic<4, 3>();
    test_cyclic<11, 4>();
    test_cyclic<16, 8>();

    {
        double some_doubles[] = {1,1,1,1,2,2,2,2,3,3,3,3};
        auto rng = debug_input_view<double>{some_doubles} | view::low_pass(4);
        using R = decltype(rng);
        CONCEPT_ASSERT(InputView<R>());
        CONCEPT_ASSERT(!BoundedRange<R>());
        ::check_equal(rng, {1.0,1.25,1.5,1.75,2.0,2.25,2.5,2.75,3.0});
    }

    return ::test_result();
}
