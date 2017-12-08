#include <range/v3/experimental/view/sorted.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/unique.hpp>
#include "../../test_utils.hpp"

int main() {
    {
        int some_ints[] = {1,2,3,9,8,7,6,4,0,5};
        auto rng = ranges::experimental::view::sorted(some_ints);
        using R = decltype(rng);
        CONCEPT_ASSERT(ranges::RandomAccessRange<R>());
        CONCEPT_ASSERT(ranges::SizedRange<R>());
        CONCEPT_ASSERT(!ranges::BoundedRange<R>());
        ::check_equal(rng, {0,1,2,3,4,5,6,7,8,9});
    }

    {
        std::pair<int, int> some_pairs[] = {
            {1,0},{2,1},{3,2},{9,3},{8,4},{7,5},{6,6},{4,7},{0,8},{5,9}};
        auto rng = ranges::experimental::view::sorted(
            some_pairs, ranges::ordered_less{}, &std::pair<int, int>::first);
        using R = decltype(rng);
        CONCEPT_ASSERT(ranges::RandomAccessRange<R>());
        CONCEPT_ASSERT(ranges::SizedRange<R>());
        CONCEPT_ASSERT(!ranges::BoundedRange<R>());
        ::check_equal(rng,
            {std::pair<int,int>{0,8},{1,0},{2,1},{3,2},{4,7},{5,9},{6,6},{7,5},{8,4},{9,3}});
    }

    {
        int some_ints[] = {42,6,3,1,3,2,3,2,2,0,0,0,1,4,5,6,7,7,7,42};
        auto rng = ranges::experimental::view::sorted(some_ints)
            | ranges::view::unique | ranges::view::take(4);
        ::check_equal(rng, {0,1,2,3});
        ::check_equal(some_ints, {0,0,0,1,1,2,2,2,3,3,3,4,7,42,42,6,7,7,6,5});
    }

    return ::test_result();
}
