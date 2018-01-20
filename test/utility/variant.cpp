// Range v3 library
//
//  Copyright Eric Niebler 2015
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3

#include <vector>
#include <sstream>
#include <iostream>
#include <range/v3/utility/variant.hpp>
#include "../simple_test.hpp"
//#include "../test_utils.hpp"

RANGES_DIAGNOSTIC_IGNORE_UNDEFINED_FUNC_TEMPLATE

template<class...> class show_type; // FIXME: remove

namespace bad_access
{
    void test()
    {
        CONCEPT_ASSERT(std::is_base_of<std::exception, ranges::bad_variant_access>::value);
        static_assert(noexcept(ranges::bad_variant_access{}), "must be noexcept");
        static_assert(noexcept(ranges::bad_variant_access{}.what()), "must be noexcept");
        ranges::bad_variant_access ex;
        CHECK(ex.what() != nullptr);
    }
}

namespace npos
{
    CONCEPT_ASSERT(ranges::variant_npos == std::size_t(-1));
}

namespace alternative
{
    template <class V, size_t I, class E>
    void single_test()
    {
        CONCEPT_ASSERT(std::is_same<meta::_t<ranges::variant_alternative<I, V>>, E>::value);
        CONCEPT_ASSERT(std::is_same<meta::_t<ranges::variant_alternative<I, const V>>, const E>::value);
        CONCEPT_ASSERT(std::is_same<meta::_t<ranges::variant_alternative<I, volatile V>>,
            volatile E>::value);
        CONCEPT_ASSERT(std::is_same<meta::_t<ranges::variant_alternative<I, const volatile V>>,
            const volatile E>::value);
        CONCEPT_ASSERT(std::is_same<ranges::variant_alternative_t<I, V>, E>::value);
        CONCEPT_ASSERT(std::is_same<ranges::variant_alternative_t<I, const V>, const E>::value);
        CONCEPT_ASSERT(std::is_same<ranges::variant_alternative_t<I, volatile V>, volatile E>::value);
        CONCEPT_ASSERT(std::is_same<ranges::variant_alternative_t<I, const volatile V>,
            const volatile E>::value);
    }

    void test()
    {
        {
            using V = ranges::variant<int, void *, const void *, long double>;
            single_test<V, 0, int>();
            single_test<V, 1, void *>();
            single_test<V, 2, const void *>();
            single_test<V, 3, long double>();
        }
        {
            using V = ranges::variant<int, int &, const int &, int &&, long double>;
            single_test<V, 0, int>();
            single_test<V, 1, int &>();
            single_test<V, 2, const int &>();
            single_test<V, 3, int &&>();
            single_test<V, 4, long double>();
        }
    }
}

namespace size
{
    template <class V, size_t E>
    void single_test()
    {
        CONCEPT_ASSERT(ranges::variant_size<V>::value == E);
        CONCEPT_ASSERT(ranges::variant_size<const V>::value == E);
        CONCEPT_ASSERT(ranges::variant_size<volatile V>::value == E);
        CONCEPT_ASSERT(ranges::variant_size<const volatile V>::value == E);
        CONCEPT_ASSERT(std::is_base_of<
            std::integral_constant<std::size_t, E>, ranges::variant_size<V>>::value);
#if RANGES_CXX_VARIABLE_TEMPLATES >= RANGES_CXX_VARIABLE_TEMPLATES_14
        CONCEPT_ASSERT(ranges::variant_size_v<V> == E);
        CONCEPT_ASSERT(ranges::variant_size_v<const V> == E);
        CONCEPT_ASSERT(ranges::variant_size_v<volatile V> == E);
        CONCEPT_ASSERT(ranges::variant_size_v<const volatile V> == E);
#endif
    }

    void test()
    {
        single_test<ranges::variant<>, 0>();
        single_test<ranges::variant<void *>, 1>();
        single_test<ranges::variant<long, long, void *, double>, 4>();
    }
} // namespace size

namespace holds_alternative
{
    void test()
    {
        {
            using V = ranges::variant<int>;
            constexpr V v;
            CONCEPT_ASSERT(ranges::holds_alternative<int>(v));
        }
        {
            using V = ranges::variant<int, long>;
            constexpr V v;
            CONCEPT_ASSERT(ranges::holds_alternative<int>(v));
            CONCEPT_ASSERT(!ranges::holds_alternative<long>(v));
        }
        { // noexcept test
            using V = ranges::variant<int>;
            const V v;
            CONCEPT_ASSERT(noexcept(ranges::holds_alternative<int>(v)));
        }
    }
} // namespace holds_alternative

namespace default_ctor
{
    struct NonDefaultConstructible {
        NonDefaultConstructible(int) {}
    };

    struct NotNoexcept {
        NotNoexcept() noexcept(false) {}
    };

    struct DefaultCtorThrows {
        [[noreturn]] DefaultCtorThrows() { throw 42; }
    };

    void test_default_ctor_sfinae() {
        {
            using V = ranges::variant<ranges::monostate, int>;
            CONCEPT_ASSERT(std::is_default_constructible<V>::value);
        }
        {
            using V = ranges::variant<NonDefaultConstructible, int>;
            CONCEPT_ASSERT(!std::is_default_constructible<V>::value);
        }
        {
            using V = ranges::variant<int &, int>;
            CONCEPT_ASSERT(!std::is_default_constructible<V>::value);
        }
    }

    void test_default_ctor_noexcept() {
        {
            using V = ranges::variant<int>;
            CONCEPT_ASSERT(std::is_nothrow_default_constructible<V>::value);
        }
        {
            using V = ranges::variant<NotNoexcept>;
            CONCEPT_ASSERT(!std::is_nothrow_default_constructible<V>::value);
        }
    }

    void test_default_ctor_throws() {
        using V = ranges::variant<DefaultCtorThrows, int>;
        try {
            V v;
            CHECK(false);
        } catch (const int &ex) {
            CHECK(ex == 42);
        } catch (...) {
            CHECK(false);
        }
    }

    void test_default_ctor_basic()
    {
        {
            ranges::variant<int> v;
            CHECK(v.index() == 0u);
            CHECK(ranges::get<0>(v) == 0);
        }
        {
            ranges::variant<int, long> v;
            CHECK(v.index() == 0u);
            CHECK(ranges::get<0>(v) == 0);
        }
        {
            using V = ranges::variant<int, long>;
            constexpr V v;
            CONCEPT_ASSERT(v.index() == 0);
            CONCEPT_ASSERT(ranges::get<0>(v) == 0);
        }
        {
            using V = ranges::variant<int, long>;
            constexpr V v;
            CONCEPT_ASSERT(v.index() == 0);
            CONCEPT_ASSERT(ranges::get<0>(v) == 0);
        }
    }

    void test()
    {
        test_default_ctor_basic();
        test_default_ctor_sfinae();
        test_default_ctor_noexcept();
        test_default_ctor_throws();
    }
} // namespace default_ctor

namespace copy_ctor
{
    struct NonT
    {
        NonT(int v) : value(v) {}
        NonT(const NonT &o) : value(o.value) {}
        int value;
    };
    CONCEPT_ASSERT(!ranges::detail::is_trivially_copy_constructible<NonT>::value);

    struct NoCopy
    {
        NoCopy(const NoCopy &) = delete;
    };

    struct MoveOnly
    {
        MoveOnly(const MoveOnly &) = delete;
        MoveOnly(MoveOnly &&) = default;
    };

    struct MoveOnlyNT
    {
        MoveOnlyNT(const MoveOnlyNT &) = delete;
        MoveOnlyNT(MoveOnlyNT &&) {}
    };

    struct NTCopy
    {
        constexpr NTCopy(int v) : value(v) {}
        NTCopy(const NTCopy &that) : value(that.value) {}
        NTCopy(NTCopy &&) = delete;
        int value;
    };

    CONCEPT_ASSERT(!ranges::detail::is_trivially_copy_constructible<NTCopy>::value);
    CONCEPT_ASSERT(std::is_copy_constructible<NTCopy>::value);

    struct TCopy
    {
        constexpr TCopy(int v) : value(v) {}
        TCopy(TCopy const &) = default;
        TCopy(TCopy &&) = delete;
        int value;
    };

    CONCEPT_ASSERT(!RANGES_PROPER_TRIVIAL_TYPE_TRAITS ||
        ranges::detail::is_trivially_copy_constructible<TCopy>::value);

    struct TCopyNTMove
    {
        constexpr TCopyNTMove(int v) : value(v) {}
        TCopyNTMove(const TCopyNTMove&) = default;
        TCopyNTMove(TCopyNTMove&& that) : value(that.value) { that.value = -1; }
        int value;
    };

    CONCEPT_ASSERT(!RANGES_PROPER_TRIVIAL_TYPE_TRAITS ||
        ranges::detail::is_trivially_copy_constructible<TCopyNTMove>::value);

    struct MakeEmptyT {
        static int alive;
        MakeEmptyT() { ++alive; }
        MakeEmptyT(const MakeEmptyT &) { ++alive; }
        [[noreturn]] MakeEmptyT(MakeEmptyT &&) { throw 42; }
        MakeEmptyT &operator=(const MakeEmptyT &) { throw 42; }
        MakeEmptyT &operator=(MakeEmptyT &&) { throw 42; }
        ~MakeEmptyT() { --alive; }
    };

    int MakeEmptyT::alive = 0;

    template <class Variant>
    void makeEmpty(Variant &v)
    {
        Variant v2(ranges::in_place_type<MakeEmptyT>);
        try
        {
            v = std::move(v2);
            CHECK(false);
        }
        catch (...)
        {
            CHECK(v.valueless_by_exception());
        }
    }

    void test_copy_ctor_sfinae()
    {
        {
            using V = ranges::variant<int, long>;
            CONCEPT_ASSERT(std::is_copy_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, NoCopy>;
            CONCEPT_ASSERT(!std::is_copy_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, MoveOnly>;
            CONCEPT_ASSERT(!std::is_copy_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, MoveOnlyNT>;
            CONCEPT_ASSERT(!std::is_copy_constructible<V>::value);
        }

        // The following tests are for not-yet-standardized behavior (P0602):
#if RANGES_PROPER_TRIVIAL_TYPE_TRAITS
        {
            using V = ranges::variant<int, long>;
            CONCEPT_ASSERT(ranges::detail::is_trivially_copy_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, NTCopy>;
            CONCEPT_ASSERT(!ranges::detail::is_trivially_copy_constructible<V>::value);
            CONCEPT_ASSERT(std::is_copy_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, TCopy>;
            CONCEPT_ASSERT(ranges::detail::is_trivially_copy_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, TCopyNTMove>;
            CONCEPT_ASSERT(ranges::detail::is_trivially_copy_constructible<V>::value);
        }
#endif
    }

    void test_copy_ctor_basic()
    {
        {
            ranges::variant<int> v(ranges::in_place_index<0>, 42);
            ranges::variant<int> v2 = v;
            CHECK(v2.index() == 0u);
            CHECK(ranges::get<0>(v2) == 42);
        }
        {
            ranges::variant<int, long> v(ranges::in_place_index<1>, 42);
            ranges::variant<int, long> v2 = v;
            CHECK(v2.index() == 1u);
            CHECK(ranges::get<1>(v2) == 42);
        }
        {
            ranges::variant<NonT> v(ranges::in_place_index<0>, 42);
            CHECK(v.index() == 0u);
            ranges::variant<NonT> v2(v);
            CHECK(v2.index() == 0u);
            CHECK(ranges::get<0>(v2).value == 42);
        }
        {
            ranges::variant<int, NonT> v(ranges::in_place_index<1>, 42);
            CHECK(v.index() == 1u);
            ranges::variant<int, NonT> v2(v);
            CHECK(v2.index() == 1u);
            CHECK(ranges::get<1>(v2).value == 42);
        }

        // The following tests are for not-yet-standardized behavior (P0602):
        {
            constexpr ranges::variant<int> v(ranges::in_place_index<0>, 42);
            CONCEPT_ASSERT(v.index() == 0);
            constexpr ranges::variant<int> v2 = v;
            CONCEPT_ASSERT(v2.index() == 0);
            CONCEPT_ASSERT(ranges::get<0>(v2) == 42);
        }
        {
            constexpr ranges::variant<int, long> v(ranges::in_place_index<1>, 42);
            CONCEPT_ASSERT(v.index() == 1);
            constexpr ranges::variant<int, long> v2 = v;
            CONCEPT_ASSERT(v2.index() == 1);
            CONCEPT_ASSERT(ranges::get<1>(v2) == 42);
        }
#if RANGES_PROPER_TRIVIAL_TYPE_TRAITS
        {
            constexpr ranges::variant<TCopy> v(ranges::in_place_index<0>, 42);
            CONCEPT_ASSERT(v.index() == 0);
            constexpr ranges::variant<TCopy> v2(v);
            CONCEPT_ASSERT(v2.index() == 0);
            CONCEPT_ASSERT(ranges::get<0>(v2).value == 42);
        }
        {
            constexpr ranges::variant<int, TCopy> v(ranges::in_place_index<1>, 42);
            CONCEPT_ASSERT(v.index() == 1);
            constexpr ranges::variant<int, TCopy> v2(v);
            CONCEPT_ASSERT(v2.index() == 1);
            CONCEPT_ASSERT(ranges::get<1>(v2).value == 42);
        }
        {
            constexpr ranges::variant<TCopyNTMove> v(ranges::in_place_index<0>, 42);
            CONCEPT_ASSERT(v.index() == 0);
            constexpr ranges::variant<TCopyNTMove> v2(v);
            CONCEPT_ASSERT(v2.index() == 0);
            CONCEPT_ASSERT(ranges::get<0>(v2).value == 42);
        }
        {
            constexpr ranges::variant<int, TCopyNTMove> v(ranges::in_place_index<1>, 42);
            CONCEPT_ASSERT(v.index() == 1);
            constexpr ranges::variant<int, TCopyNTMove> v2(v);
            CONCEPT_ASSERT(v2.index() == 1);
            CONCEPT_ASSERT(ranges::get<1>(v2).value == 42);
        }
#endif
    }

    void test_copy_ctor_valueless_by_exception()
    {
        using V = ranges::variant<int, MakeEmptyT>;
        V v1;
        makeEmpty(v1);
        const V &cv1 = v1;
        V v(cv1);
        CHECK(v.valueless_by_exception());
    }

    template <size_t Idx>
    constexpr bool test_constexpr_copy_ctor_extension_imp2(
        ranges::variant<long, void*, const int> const& v1,
        ranges::variant<long, void*, const int> const v2)
    {
        return v2.index() == v1.index() &&
            v2.index() == Idx &&
            ranges::get<Idx>(v2) == ranges::get<Idx>(v1);
    }

    template <size_t Idx>
    constexpr bool test_constexpr_copy_ctor_extension_imp(
        ranges::variant<long, void*, const int> const& v)
    {
        return test_constexpr_copy_ctor_extension_imp2<Idx>(v, v);
    }

    void test_constexpr_copy_ctor_extension()
    {
        // NOTE: This test is for not yet standardized behavior. (P0602)
        using V = ranges::variant<long, void*, const int>;
        CONCEPT_ASSERT(!RANGES_PROPER_TRIVIAL_TYPE_TRAITS ||
            ranges::detail::is_trivially_copyable<V>::value);
        CONCEPT_ASSERT(test_constexpr_copy_ctor_extension_imp<0>(
            V(ranges::in_place_index<0>, 42l)));
        CONCEPT_ASSERT(test_constexpr_copy_ctor_extension_imp<1>(
            V(ranges::in_place_index<1>, nullptr)));
        CONCEPT_ASSERT(test_constexpr_copy_ctor_extension_imp<2>(
            V(ranges::in_place_index<2>, 101)));
    }

    void test()
    {
        test_copy_ctor_basic();
        test_copy_ctor_valueless_by_exception();
        test_copy_ctor_sfinae();
        test_constexpr_copy_ctor_extension();
#if RANGES_CXX_DEDUCTION_GUIDES >= RANGES_CXX_DEDUCTION_GUIDES_17
        { // This is the motivating example from P0739R0
            ranges::variant<int, double> v1(ranges::in_place_index<0>, 3);
            ranges::variant v2 = v1;
            (void) v2;
        }
#endif
    }
} // namespace copy_ctor

int main()
{
    bad_access::test();
    alternative::test();
    size::test();
    holds_alternative::test();
    default_ctor::test();
    copy_ctor::test();

#if 0
    // Simple variant and access.
    {
        variant<int, short> v;
        CHECK(v.index() == 0u);
        auto v2 = v;
        CHECK(v2.index() == 0u);
        v.emplace<1>((short)2);
        CHECK(v.index() == 1u);
        CHECK(get<1>(v) == (short)2);
        try
        {
            get<0>(v);
            CHECK(false);
        }
        catch(const bad_variant_access&)
        {}
        catch(...)
        {
            CHECK(!(bool)"unknown exception");
        }
        v = v2;
        CHECK(v.index() == 0u);
    }

    // variant of void
    {
        variant<void, void> v;
        CHECK(v.index() == 0u);
        v.emplace<0>();
        CHECK(v.index() == 0u);
        try
        {
            // Will only compile if get returns void
            v.index() == 0 ? void() : get<0>(v);
        }
        catch(...)
        {
            CHECK(false);
        }
        v.emplace<1>();
        CHECK(v.index() == 1u);
        try
        {
            get<0>(v);
            CHECK(false);
        }
        catch(const bad_variant_access&)
        {}
        catch(...)
        {
            CHECK(!(bool)"unknown exception");
        }
    }

    // variant of references
    {
        int i = 42;
        std::string s = "hello world";
        variant<int&, std::string&> v{emplaced_index<0>, i};
        CONCEPT_ASSERT(!DefaultConstructible<variant<int&, std::string&>>());
        CHECK(v.index() == 0u);
        CHECK(get<0>(v) == 42);
        CHECK(&get<0>(v) == &i);
        auto const & cv = v;
        get<0>(cv) = 24;
        CHECK(i == 24);
        v.emplace<1>(s);
        CHECK(v.index() == 1u);
        CHECK(get<1>(v) == "hello world");
        CHECK(&get<1>(v) == &s);
        get<1>(cv) = "goodbye";
        CHECK(s == "goodbye");
    }

    // Move test 1
    {
        variant<int, MoveOnlyString> v{emplaced_index<1>, "hello world"};
        CHECK(get<1>(v) == "hello world");
        MoveOnlyString s = get<1>(std::move(v));
        CHECK(s == "hello world");
        CHECK(get<1>(v) == "");
        v.emplace<1>("goodbye");
        CHECK(get<1>(v) == "goodbye");
        auto v2 = std::move(v);
        CHECK(get<1>(v2) == "goodbye");
        CHECK(get<1>(v) == "");
        v = std::move(v2);
        CHECK(get<1>(v) == "goodbye");
        CHECK(get<1>(v2) == "");
    }

    // Move test 2
    {
        MoveOnlyString s = "hello world";
        variant<MoveOnlyString&> v{emplaced_index<0>, s};
        CHECK(get<0>(v) == "hello world");
        MoveOnlyString &s2 = get<0>(std::move(v));
        CHECK(&s2 == &s);
    }

    // Apply test 1
    {
        std::stringstream sout;
        variant<int, std::string> v{emplaced_index<1>, "hello"};
        auto fun = overload(
            [&sout](int&) {sout << "int";},
            [&sout](std::string&)->int {sout << "string"; return 42;});
        variant<void, int> x = v.visit(fun);
        CHECK(sout.str() == "string");
        CHECK(x.index() == 1u);
        CHECK(get<1>(x) == 42);
    }

    // Apply test 2
    {
        std::stringstream sout;
        std::string s = "hello";
        variant<int, std::string&> const v{emplaced_index<1>, s};
        auto fun = overload(
            [&sout](int const&) {sout << "int";},
            [&sout](std::string&)->int {sout << "string"; return 42;});
        variant<void, int> x = v.visit(fun);
        CHECK(sout.str() == "string");
        CHECK(x.index() == 1u);
        CHECK(get<1>(x) == 42);
    }

    // constexpr variant
    {
        constexpr variant<int, short> v{emplaced_index<1>, (short)2};
        static_assert(v.index() == 1,"");
        static_assert(v.valid(),"");
    }

    // Variant and arrays
    {
        variant<int[5], std::vector<int>> v{emplaced_index<0>, {1,2,3,4,5}};
        int (&rgi)[5] = get<0>(v);
        check_equal(rgi, {1,2,3,4,5});

        variant<int[5], std::vector<int>> v2{emplaced_index<0>, {}};
        int (&rgi2)[5] = get<0>(v2);
        check_equal(rgi2, {0,0,0,0,0});

        v2 = v;
        check_equal(rgi2, {1,2,3,4,5});

        struct T
        {
            T() = delete;
            T(int) {}
            T(T const &) = default;
            T &operator=(T const &) = default;
        };

        // Should compile and not assert at runtime.
        variant<T[5]> vrgt{emplaced_index<0>, {T{42},T{42},T{42},T{42},T{42}}};
        (void) vrgt;
    }
#endif

    return ::test_result();
}
