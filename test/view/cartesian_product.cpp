#include <iostream>
#include <range/v3/utility/tuple_algorithm.hpp>
#include <range/v3/view/cartesian_product.hpp>
#include <range/v3/view/reverse.hpp>
#include "../simple_test.hpp"
#include "../test_utils.hpp"

using namespace ranges;

namespace ranges {
    template<typename... Ts>
    std::ostream &operator<<(std::ostream &os, common_tuple<Ts...> const& t)
    {
        os << '(';
        auto first = true;
        tuple_for_each(t, [&](auto const& e) {
            if (first) first = false;
            else os << ',';
            os << e;
        });
        os << ')';
        return os;
    }
}

int main() {
    const int some_ints[] = {0,1,2,3};
    const char * const some_strings[] = {"John", "Paul", "George", "Ringo"};
    auto rng = view::cartesian_product(some_ints, some_strings);

    ::models<concepts::ForwardView>(rng);
    ::models<concepts::BoundedRange>(rng);

    using CT = common_tuple<int, std::string>;
    auto control = std::initializer_list<CT>{
        CT{0, "John"}, CT{0, "Paul"}, CT{0, "George"}, CT{0, "Ringo"},
        CT{1, "John"}, CT{1, "Paul"}, CT{1, "George"}, CT{1, "Ringo"},
        CT{2, "John"}, CT{2, "Paul"}, CT{2, "George"}, CT{2, "Ringo"},
        CT{3, "John"}, CT{3, "Paul"}, CT{3, "George"}, CT{3, "Ringo"}};

    ::check_equal(rng, control);
    ::check_equal(view::reverse(rng), view::reverse(control));

    CHECK(size(rng) == 16u);

    auto const first = ranges::begin(rng);
    auto const last = ranges::end(rng);
    CHECK((last - first) == 16);
    for(auto i = 0; i <= distance(rng); ++i)
    {
        for(auto j = 0; j <= distance(rng); ++j)
        {
            CHECK((ranges::next(first, i) - ranges::next(first, j)) == i - j);
        }
    }

    return test_result();
}
