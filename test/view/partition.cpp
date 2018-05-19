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
#include <vector>
#include <range/v3/utility/iterator.hpp>
#include <range/v3/view/partition.hpp>
#include "../simple_test.hpp"
#include "../test_utils.hpp"

int main()
{
    {
        int some_ints[] = {0,1,2,3,4,5,6,7};
        std::vector<int> vec1, vec2;
        auto is_even = [](int i) { return i % 2 == 0; };
        auto is_nearly_pow2 = [](int i) { return (i & (i + 1)) == 0; };
        auto rng = some_ints |
            ranges::view::partition(ranges::back_inserter(vec1), is_even) |
            ranges::view::partition(ranges::back_inserter(vec2), is_nearly_pow2);
        check_equal(rng,  {5});
        check_equal(vec1, {0,2,4,6});
        check_equal(vec2, {1,3,7});
    }

    return test_result();
}
