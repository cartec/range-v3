// Range v3 library
//
//  Copyright Eric Niebler 2015
//  Copyright Casey Carter 2018
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3

// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <vector>
#include <sstream>
#include <iostream>
#include <range/v3/utility/variant.hpp>
#include "../simple_test.hpp"
//#include "../test_utils.hpp"

RANGES_DIAGNOSTIC_IGNORE_UNDEFINED_FUNC_TEMPLATE

template<class...> class show_type; // FIXME: remove

#if RANGES_CXX_STATIC_ASSERT >= RANGES_CXX_STATIC_ASSERT_17
#define STATIC_ASSERT(...) static_assert(__VA_ARGS__)
#else
#define STATIC_ASSERT(...) static_assert((__VA_ARGS__), #__VA_ARGS__)
#endif
#define ASSERT_NOEXCEPT(...) STATIC_ASSERT(noexcept(__VA_ARGS__))

namespace detail {
    template <class Tp> void eat_type(Tp);

    template <class Tp, class ...Args>
    constexpr auto test_convertible_imp(int)
        -> decltype(eat_type<Tp>({std::declval<Args>()...}), true)
    { return true; }

    template <class Tp, class ...Args>
    constexpr auto test_convertible_imp(long) -> bool { return false; }
}

template <class Tp, class ...Args>
constexpr bool test_convertible()
{ return detail::test_convertible_imp<Tp, Args...>(0); }


struct MakeEmptyT
{
    static int alive;
    MakeEmptyT() { ++alive; }
    MakeEmptyT(MakeEmptyT const&) {
        ++alive;
        // Don't throw from the copy constructor since variant's assignment
        // operator performs a copy before committing to the assignment.
    }
    [[noreturn]] MakeEmptyT(MakeEmptyT &&)
    {
        throw 42;
    }
    MakeEmptyT& operator=(MakeEmptyT const&)
    {
        throw 42;
    }
    MakeEmptyT& operator=(MakeEmptyT&&)
    {
        throw 42;
    }
    ~MakeEmptyT()
    {
        --alive;
    }
};
STATIC_ASSERT(ranges::is_swappable<MakeEmptyT>::value); // required for test

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

namespace bad_access
{
    void test()
    {
        STATIC_ASSERT(std::is_base_of<std::exception, ranges::bad_variant_access>::value);
        static_assert(noexcept(ranges::bad_variant_access{}), "must be noexcept");
        static_assert(noexcept(ranges::bad_variant_access{}.what()), "must be noexcept");
        ranges::bad_variant_access ex;
        CHECK(ex.what() != nullptr);
    }
}

namespace npos
{
    STATIC_ASSERT(ranges::variant_npos == std::size_t(-1));
}

namespace alternative
{
    template <class V, size_t I, class E>
    void single_test()
    {
        STATIC_ASSERT(std::is_same<meta::_t<ranges::variant_alternative<I, V>>, E>::value);
        STATIC_ASSERT(std::is_same<meta::_t<ranges::variant_alternative<I, const V>>, const E>::value);
        STATIC_ASSERT(std::is_same<meta::_t<ranges::variant_alternative<I, volatile V>>,
            volatile E>::value);
        STATIC_ASSERT(std::is_same<meta::_t<ranges::variant_alternative<I, const volatile V>>,
            const volatile E>::value);
        STATIC_ASSERT(std::is_same<ranges::variant_alternative_t<I, V>, E>::value);
        STATIC_ASSERT(std::is_same<ranges::variant_alternative_t<I, const V>, const E>::value);
        STATIC_ASSERT(std::is_same<ranges::variant_alternative_t<I, volatile V>, volatile E>::value);
        STATIC_ASSERT(std::is_same<ranges::variant_alternative_t<I, const volatile V>,
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
        STATIC_ASSERT(ranges::variant_size<V>::value == E);
        STATIC_ASSERT(ranges::variant_size<const V>::value == E);
        STATIC_ASSERT(ranges::variant_size<volatile V>::value == E);
        STATIC_ASSERT(ranges::variant_size<const volatile V>::value == E);
        STATIC_ASSERT(std::is_base_of<std::integral_constant<std::size_t, E>,
            ranges::variant_size<V>>::value);
#if RANGES_CXX_VARIABLE_TEMPLATES >= RANGES_CXX_VARIABLE_TEMPLATES_14
        STATIC_ASSERT(ranges::variant_size_v<V> == E);
        STATIC_ASSERT(ranges::variant_size_v<const V> == E);
        STATIC_ASSERT(ranges::variant_size_v<volatile V> == E);
        STATIC_ASSERT(ranges::variant_size_v<const volatile V> == E);
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
            STATIC_ASSERT(ranges::holds_alternative<int>(v));
        }
        {
            using V = ranges::variant<int, long>;
            constexpr V v;
            STATIC_ASSERT(ranges::holds_alternative<int>(v));
            STATIC_ASSERT(!ranges::holds_alternative<long>(v));
        }
        { // noexcept test
            using V = ranges::variant<int>;
            const V v;
            STATIC_ASSERT(noexcept(ranges::holds_alternative<int>(v)));
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
            STATIC_ASSERT(std::is_default_constructible<V>::value);
        }
        {
            using V = ranges::variant<NonDefaultConstructible, int>;
            STATIC_ASSERT(!std::is_default_constructible<V>::value);
        }
        {
            using V = ranges::variant<int &, int>;
            STATIC_ASSERT(!std::is_default_constructible<V>::value);
        }
    }

    void test_default_ctor_noexcept() {
        {
            using V = ranges::variant<int>;
            STATIC_ASSERT(std::is_nothrow_default_constructible<V>::value);
        }
        {
            using V = ranges::variant<NotNoexcept>;
            STATIC_ASSERT(!std::is_nothrow_default_constructible<V>::value);
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
            STATIC_ASSERT(v.index() == 0);
            STATIC_ASSERT(ranges::get<0>(v) == 0);
        }
        {
            using V = ranges::variant<int, long>;
            constexpr V v;
            STATIC_ASSERT(v.index() == 0);
            STATIC_ASSERT(ranges::get<0>(v) == 0);
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
    STATIC_ASSERT(!ranges::detail::is_trivially_copy_constructible<NonT>::value);

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

    STATIC_ASSERT(!ranges::detail::is_trivially_copy_constructible<NTCopy>::value);
    STATIC_ASSERT(std::is_copy_constructible<NTCopy>::value);

    struct TCopy
    {
        constexpr TCopy(int v) : value(v) {}
        TCopy(TCopy const &) = default;
        TCopy(TCopy &&) = delete;
        int value;
    };

    STATIC_ASSERT(!RANGES_PROPER_TRIVIAL_TRAITS ||
        ranges::detail::is_trivially_copy_constructible<TCopy>::value);

    struct TCopyNTMove
    {
        constexpr TCopyNTMove(int v) : value(v) {}
        TCopyNTMove(const TCopyNTMove&) = default;
        TCopyNTMove(TCopyNTMove&& that) : value(that.value) { that.value = -1; }
        int value;
    };

    STATIC_ASSERT(!RANGES_PROPER_TRIVIAL_TRAITS ||
        ranges::detail::is_trivially_copy_constructible<TCopyNTMove>::value);

    void test_copy_ctor_sfinae()
    {
        {
            using V = ranges::variant<int, long>;
            STATIC_ASSERT(std::is_copy_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, NoCopy>;
            STATIC_ASSERT(!std::is_copy_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, MoveOnly>;
            STATIC_ASSERT(!std::is_copy_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, MoveOnlyNT>;
            STATIC_ASSERT(!std::is_copy_constructible<V>::value);
        }

        // The following tests are for not-yet-standardized behavior (P0602):
#if RANGES_PROPER_TRIVIAL_TRAITS
        {
            using V = ranges::variant<int, long>;
            STATIC_ASSERT(ranges::detail::is_trivially_copy_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, NTCopy>;
            STATIC_ASSERT(!ranges::detail::is_trivially_copy_constructible<V>::value);
            STATIC_ASSERT(std::is_copy_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, TCopy>;
            STATIC_ASSERT(ranges::detail::is_trivially_copy_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, TCopyNTMove>;
            STATIC_ASSERT(ranges::detail::is_trivially_copy_constructible<V>::value);
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
            STATIC_ASSERT(v.index() == 0);
            constexpr ranges::variant<int> v2 = v;
            STATIC_ASSERT(v2.index() == 0);
            STATIC_ASSERT(ranges::get<0>(v2) == 42);
        }
        {
            constexpr ranges::variant<int, long> v(ranges::in_place_index<1>, 42);
            STATIC_ASSERT(v.index() == 1);
            constexpr ranges::variant<int, long> v2 = v;
            STATIC_ASSERT(v2.index() == 1);
            STATIC_ASSERT(ranges::get<1>(v2) == 42);
        }
#if RANGES_PROPER_TRIVIAL_TRAITS
        {
            constexpr ranges::variant<TCopy> v(ranges::in_place_index<0>, 42);
            STATIC_ASSERT(v.index() == 0);
            constexpr ranges::variant<TCopy> v2(v);
            STATIC_ASSERT(v2.index() == 0);
            STATIC_ASSERT(ranges::get<0>(v2).value == 42);
        }
        {
            constexpr ranges::variant<int, TCopy> v(ranges::in_place_index<1>, 42);
            STATIC_ASSERT(v.index() == 1);
            constexpr ranges::variant<int, TCopy> v2(v);
            STATIC_ASSERT(v2.index() == 1);
            STATIC_ASSERT(ranges::get<1>(v2).value == 42);
        }
        {
            constexpr ranges::variant<TCopyNTMove> v(ranges::in_place_index<0>, 42);
            STATIC_ASSERT(v.index() == 0);
            constexpr ranges::variant<TCopyNTMove> v2(v);
            STATIC_ASSERT(v2.index() == 0);
            STATIC_ASSERT(ranges::get<0>(v2).value == 42);
        }
        {
            constexpr ranges::variant<int, TCopyNTMove> v(ranges::in_place_index<1>, 42);
            STATIC_ASSERT(v.index() == 1);
            constexpr ranges::variant<int, TCopyNTMove> v2(v);
            STATIC_ASSERT(v2.index() == 1);
            STATIC_ASSERT(ranges::get<1>(v2).value == 42);
        }
#endif // RANGES_PROPER_TRIVIAL_TRAITS
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
        STATIC_ASSERT(!RANGES_PROPER_TRIVIAL_TRAITS ||
            ranges::detail::is_trivially_copyable<V>::value);
        STATIC_ASSERT(test_constexpr_copy_ctor_extension_imp<0>(
            V(ranges::in_place_index<0>, 42l)));
        STATIC_ASSERT(test_constexpr_copy_ctor_extension_imp<1>(
            V(ranges::in_place_index<1>, nullptr)));
        STATIC_ASSERT(test_constexpr_copy_ctor_extension_imp<2>(
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

namespace move_ctor
{
    struct ThrowsMove
    {
        ThrowsMove(ThrowsMove &&) noexcept(false) {}
    };

    struct NoCopy
    {
        NoCopy(const NoCopy &) = delete;
    };

    struct MoveOnly
    {
        int value;
        MoveOnly(int v) : value(v) {}
        MoveOnly(const MoveOnly &) = delete;
        MoveOnly(MoveOnly &&) = default;
    };

    struct MoveOnlyNT
    {
        int value;
        MoveOnlyNT(int v) : value(v) {}
        MoveOnlyNT(const MoveOnlyNT &) = delete;
        MoveOnlyNT(MoveOnlyNT &&other) : value(other.value) { other.value = -1; }
    };

    struct NTMove
    {
        constexpr NTMove(int v) : value(v) {}
        NTMove(const NTMove &) = delete;
        NTMove(NTMove &&that) : value(that.value) { that.value = -1; }
        int value;
    };

    STATIC_ASSERT(!ranges::detail::is_trivially_move_constructible<NTMove>::value);
    STATIC_ASSERT(std::is_move_constructible<NTMove>::value);

    struct TMove
    {
        constexpr TMove(int v) : value(v) {}
        TMove(const TMove &) = delete;
        TMove(TMove &&) = default;
        int value;
    };

    STATIC_ASSERT(!RANGES_PROPER_TRIVIAL_TRAITS ||
        ranges::detail::is_trivially_move_constructible<TMove>::value);

    struct TMoveNTCopy
    {
        constexpr TMoveNTCopy(int v) : value(v) {}
        TMoveNTCopy(const TMoveNTCopy& that) : value(that.value) {}
        TMoveNTCopy(TMoveNTCopy&&) = default;
        int value;
    };

    STATIC_ASSERT(!RANGES_PROPER_TRIVIAL_TRAITS ||
        ranges::detail::is_trivially_move_constructible<TMoveNTCopy>::value);

    void test_move_noexcept()
    {
        {
            using V = ranges::variant<int, long>;
            STATIC_ASSERT(std::is_nothrow_move_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, MoveOnly>;
            STATIC_ASSERT(std::is_nothrow_move_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, MoveOnlyNT>;
            STATIC_ASSERT(!std::is_nothrow_move_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, ThrowsMove>;
            STATIC_ASSERT(!std::is_nothrow_move_constructible<V>::value);
        }
    }

    void test_move_ctor_sfinae()
    {
        {
            using V = ranges::variant<int, long>;
            STATIC_ASSERT(std::is_move_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, MoveOnly>;
            STATIC_ASSERT(std::is_move_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, MoveOnlyNT>;
            STATIC_ASSERT(std::is_move_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, NoCopy>;
            STATIC_ASSERT(!std::is_move_constructible<V>::value);
        }

#if RANGES_PROPER_TRIVIAL_TRAITS
        // The following tests are for not-yet-standardized behavior (P0602):
        {
            using V = ranges::variant<int, long>;
            STATIC_ASSERT(ranges::detail::is_trivially_move_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, NTMove>;
            STATIC_ASSERT(!ranges::detail::is_trivially_move_constructible<V>::value);
            STATIC_ASSERT(std::is_move_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, TMove>;
            STATIC_ASSERT(ranges::detail::is_trivially_move_constructible<V>::value);
        }
        {
            using V = ranges::variant<int, TMoveNTCopy>;
            STATIC_ASSERT(ranges::detail::is_trivially_move_constructible<V>::value);
        }
#endif // RANGES_PROPER_TRIVIAL_TRAITS
    }

    template <typename T>
    struct Result { size_t index; T value; };

    void test_move_ctor_basic()
    {
        {
            ranges::variant<int> v(ranges::in_place_index<0>, 42);
            ranges::variant<int> v2 = std::move(v);
            CHECK(v2.index() == 0u);
            CHECK(ranges::get<0>(v2) == 42);
        }
        {
            ranges::variant<int, long> v(ranges::in_place_index<1>, 42);
            ranges::variant<int, long> v2 = std::move(v);
            CHECK(v2.index() == 1u);
            CHECK(ranges::get<1>(v2) == 42);
        }
        {
            ranges::variant<MoveOnly> v(ranges::in_place_index<0>, 42);
            CHECK(v.index() == 0u);
            ranges::variant<MoveOnly> v2(std::move(v));
            CHECK(v2.index() == 0u);
            CHECK(ranges::get<0>(v2).value == 42);
        }
        {
            ranges::variant<int, MoveOnly> v(ranges::in_place_index<1>, 42);
            CHECK(v.index() == 1u);
            ranges::variant<int, MoveOnly> v2(std::move(v));
            CHECK(v2.index() == 1u);
            CHECK(ranges::get<1>(v2).value == 42);
        }
        {
            ranges::variant<MoveOnlyNT> v(ranges::in_place_index<0>, 42);
            CHECK(v.index() == 0u);
            ranges::variant<MoveOnlyNT> v2(std::move(v));
            CHECK(v2.index() == 0u);
            CHECK(ranges::get<0>(v).value == -1);
            CHECK(ranges::get<0>(v2).value == 42);
        }
        {
            ranges::variant<int, MoveOnlyNT> v(ranges::in_place_index<1>, 42);
            CHECK(v.index() == 1u);
            ranges::variant<int, MoveOnlyNT> v2(std::move(v));
            CHECK(v2.index() == 1u);
            CHECK(ranges::get<1>(v).value == -1);
            CHECK(ranges::get<1>(v2).value == 42);
        }

        // The following tests are for not-yet-standardized behavior (P0602):
        {
            struct
            {
                static constexpr Result<int> impl2(ranges::variant<int> v2) {
                    return {v2.index(), ranges::get<0>(ranges::detail::move(v2))};
                }
                static constexpr Result<int> impl1(ranges::variant<int> v) {
                    return impl2(ranges::detail::move(v));
                }
                constexpr Result<int> operator()() const {
                    return impl1(ranges::variant<int>(ranges::in_place_index<0>, 42));
                }
            } test;
            constexpr auto result = test();
            STATIC_ASSERT(result.index == 0);
            STATIC_ASSERT(result.value == 42);
        }
        {
            struct
            {
                static constexpr Result<long> impl2(ranges::variant<int, long> v2) {
                    return {v2.index(), ranges::get<1>(ranges::detail::move(v2))};
                }
                static constexpr Result<long> impl1(ranges::variant<int, long> v) {
                    return impl2(ranges::detail::move(v));
                }
                constexpr Result<long> operator()() const {
                    return impl1(ranges::variant<int, long>(ranges::in_place_index<1>, 42));
                }
            } test;
            constexpr auto result = test();
            STATIC_ASSERT(result.index == 1);
            STATIC_ASSERT(result.value == 42);
        }
#if RANGES_PROPER_TRIVIAL_TRAITS
        {
            using V = ranges::variant<TMove>;
            constexpr std::size_t I = 0;
            struct
            {
                using R = Result<ranges::variant_alternative_t<I, V>>;
                static constexpr R impl2(V v)
                {
                    return {v.index(), ranges::get<I>(ranges::detail::move(v))};
                }
                static constexpr R impl1(V v)
                {
                    return impl2(V{ranges::detail::move(v)});
                }
                constexpr R operator()() const
                {
                    return impl1(V{ranges::in_place_index<I>, 42});
                }
            } test;
            constexpr auto result = test();
            STATIC_ASSERT(result.index == I);
            STATIC_ASSERT(result.value.value == 42);
        }
        {
            using V = ranges::variant<int, TMove>;
            constexpr std::size_t I = 1;
            struct
            {
                using R = Result<ranges::variant_alternative_t<I, V>>;
                static constexpr R impl2(V v)
                {
                    return {v.index(), ranges::get<I>(ranges::detail::move(v))};
                }
                static constexpr R impl1(V v)
                {
                    return impl2(V{ranges::detail::move(v)});
                }
                constexpr R operator()() const
                {
                    return impl1(V{ranges::in_place_index<I>, 42});
                }
            } test;
            constexpr auto result = test();
            STATIC_ASSERT(result.index == I);
            STATIC_ASSERT(result.value.value == 42);
        }
        {
            using V = ranges::variant<TMoveNTCopy>;
            constexpr std::size_t I = 0;
            struct
            {
                using R = Result<ranges::variant_alternative_t<I, V>>;
                static constexpr R impl2(V v)
                {
                    return {v.index(), ranges::get<I>(ranges::detail::move(v))};
                }
                static constexpr R impl1(V v)
                {
                    return impl2(V{ranges::detail::move(v)});
                }
                constexpr R operator()() const
                {
                    return impl1(V{ranges::in_place_index<I>, 42});
                }
            } test;
            constexpr auto result = test();
            STATIC_ASSERT(result.index == I);
            STATIC_ASSERT(result.value.value == 42);
        }
        {
            using V = ranges::variant<int, TMoveNTCopy>;
            constexpr std::size_t I = 1;
            struct
            {
                using R = Result<ranges::variant_alternative_t<I, V>>;
                static constexpr R impl2(V v)
                {
                    return {v.index(), ranges::get<I>(ranges::detail::move(v))};
                }
                static constexpr R impl1(V v)
                {
                    return impl2(V{ranges::detail::move(v)});
                }
                constexpr R operator()() const
                {
                    return impl1(V{ranges::in_place_index<I>, 42});
                }
            } test;
            constexpr auto result = test();
            STATIC_ASSERT(result.index == I);
            STATIC_ASSERT(result.value.value == 42);
        }
#endif // RANGES_PROPER_TRIVIAL_TRAITS
    }

    void test_move_ctor_valueless_by_exception()
    {
        using V = ranges::variant<int, MakeEmptyT>;
        V v1;
        makeEmpty(v1);
        V v(std::move(v1));
        CHECK(v.valueless_by_exception());
    }

    template <size_t Idx>
    constexpr bool test_constexpr_ctor_extension_imp3(
            ranges::variant<long, void*, const int> const& v1,
            ranges::variant<long, void*, const int> v2)
    {
        return v2.index() == v1.index() && v2.index() == Idx &&
            ranges::get<Idx>(v2) == ranges::get<Idx>(v1);
    }
    template <size_t Idx>
    constexpr bool test_constexpr_ctor_extension_imp2(
            ranges::variant<long, void*, const int> const& v1,
            ranges::variant<long, void*, const int> v2)
    {
        return test_constexpr_ctor_extension_imp3<Idx>(v1, ranges::detail::move(v2));
    }
    template <size_t Idx>
    constexpr bool test_constexpr_ctor_extension_imp(
            ranges::variant<long, void*, const int> const& v)
    {
        return test_constexpr_ctor_extension_imp2<Idx>(v, v);
    }

    void test_constexpr_move_ctor_extension()
    {
        // NOTE: This test is for not yet standardized behavior. (P0602)
        using V = ranges::variant<long, void*, const int>;
        STATIC_ASSERT(!RANGES_PROPER_TRIVIAL_TRAITS ||
            ranges::detail::is_trivially_copyable<V>::value);
        STATIC_ASSERT(!RANGES_PROPER_TRIVIAL_TRAITS ||
            ranges::detail::is_trivially_move_constructible<V>::value);
        STATIC_ASSERT(test_constexpr_ctor_extension_imp<0>(V(ranges::in_place_index<0>, 42l)));
        STATIC_ASSERT(test_constexpr_ctor_extension_imp<1>(V(ranges::in_place_index<1>, nullptr)));
        STATIC_ASSERT(test_constexpr_ctor_extension_imp<2>(V(ranges::in_place_index<2>, 101)));
    }

    void test()
    {
        test_move_ctor_basic();
        test_move_ctor_valueless_by_exception();
        test_move_noexcept();
        test_move_ctor_sfinae();
        test_constexpr_move_ctor_extension();
    }
} // namespace move_ctor

namespace in_place_index_ctor
{
    void test_ctor_sfinae()
    {
        {
            using V = ranges::variant<int>;
            STATIC_ASSERT(
                std::is_constructible<V, ranges::in_place_index_t<0>, int>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_index_t<0>, int>());
        }
        {
            using V = ranges::variant<int, long, long long>;
            STATIC_ASSERT(
                std::is_constructible<V, ranges::in_place_index_t<1>, int>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_index_t<1>, int>());
        }
        {
            using V = ranges::variant<int, long, int *>;
            STATIC_ASSERT(
                    std::is_constructible<V, ranges::in_place_index_t<2>, int *>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_index_t<2>, int *>());
        }
        { // args not convertible to type
            using V = ranges::variant<int, long, int *>;
            STATIC_ASSERT(
                    !std::is_constructible<V, ranges::in_place_index_t<0>, int *>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_index_t<0>, int *>());
        }
        { // index not in variant
            using V = ranges::variant<int, long, int *>;
            STATIC_ASSERT(
                    !std::is_constructible<V, ranges::in_place_index_t<3>, int>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_index_t<3>, int>());
        }
    }

    void test_ctor_basic()
    {
        {
            constexpr ranges::variant<int> v(ranges::in_place_index<0>, 42);
            STATIC_ASSERT(v.index() == 0);
            STATIC_ASSERT(ranges::get<0>(v) == 42);
        }
        {
            constexpr ranges::variant<int, long, long> v(ranges::in_place_index<1>, 42);
            STATIC_ASSERT(v.index() == 1);
            STATIC_ASSERT(ranges::get<1>(v) == 42);
        }
        {
            constexpr ranges::variant<int, const int, long> v(ranges::in_place_index<1>, 42);
            STATIC_ASSERT(v.index() == 1);
            STATIC_ASSERT(ranges::get<1>(v) == 42);
        }
        {
            using V = ranges::variant<const int, volatile int, int>;
            int x = 42;
            V v(ranges::in_place_index<0>, x);
            CHECK(v.index() == 0u);
            CHECK(ranges::get<0>(v) == x);
        }
        {
            using V = ranges::variant<const int, volatile int, int>;
            int x = 42;
            V v(ranges::in_place_index<1>, x);
            CHECK(v.index() == 1u);
            CHECK(ranges::get<1>(v) == x);
        }
        {
            using V = ranges::variant<const int, volatile int, int>;
            int x = 42;
            V v(ranges::in_place_index<2>, x);
            CHECK(v.index() == 2u);
            CHECK(ranges::get<2>(v) == x);
        }
    }

    void test()
    {
        test_ctor_basic();
        test_ctor_sfinae();
    }
} // namespace in_place_index_ctor

namespace in_place_index_il_ctor
{
    struct InitList
    {
        std::size_t size;
        constexpr InitList(std::initializer_list<int> il)
          : size(il.size())
        {}
    };

    struct InitListArg
    {
        std::size_t size;
        int value;
        constexpr InitListArg(std::initializer_list<int> il, int v)
          : size(il.size()), value(v)
        {}
    };

    void test_ctor_sfinae()
    {
        using IL = std::initializer_list<int>;
        { // just init list
            using V = ranges::variant<InitList, InitListArg, int>;
            STATIC_ASSERT(std::is_constructible<V, ranges::in_place_index_t<0>, IL>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_index_t<0>, IL>());
        }
        { // too many arguments
            using V = ranges::variant<InitList, InitListArg, int>;
            STATIC_ASSERT(!std::is_constructible<V, ranges::in_place_index_t<0>, IL, int>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_index_t<0>, IL, int>());
        }
        { // too few arguments
            using V = ranges::variant<InitList, InitListArg, int>;
            STATIC_ASSERT(!std::is_constructible<V, ranges::in_place_index_t<1>, IL>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_index_t<1>, IL>());
        }
        { // init list and arguments
            using V = ranges::variant<InitList, InitListArg, int>;
            STATIC_ASSERT(std::is_constructible<V, ranges::in_place_index_t<1>, IL, int>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_index_t<1>, IL, int>());
        }
        { // not constructible from arguments
            using V = ranges::variant<InitList, InitListArg, int>;
            STATIC_ASSERT(!std::is_constructible<V, ranges::in_place_index_t<2>, IL>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_index_t<2>, IL>());
        }
    }

    void test_ctor_basic()
    {
        {
            constexpr ranges::variant<InitList, InitListArg, InitList> v(
                    ranges::in_place_index<0>, {1, 2, 3});
            STATIC_ASSERT(v.index() == 0);
            STATIC_ASSERT(ranges::get<0>(v).size == 3);
        }
        {
            constexpr ranges::variant<InitList, InitListArg, InitList> v(
                    ranges::in_place_index<2>, {1, 2, 3});
            STATIC_ASSERT(v.index() == 2);
            STATIC_ASSERT(ranges::get<2>(v).size == 3);
        }
        {
            constexpr ranges::variant<InitList, InitListArg, InitListArg> v(
                    ranges::in_place_index<1>, {1, 2, 3, 4}, 42);
            STATIC_ASSERT(v.index() == 1);
            STATIC_ASSERT(ranges::get<1>(v).size == 4);
            STATIC_ASSERT(ranges::get<1>(v).value == 42);
        }
    }

    void test()
    {
        test_ctor_basic();
        test_ctor_sfinae();
    }
} // namespace in_place_index_il_ctor

namespace in_place_type_ctor
{
    void test_ctor_sfinae()
    {
        {
            using V = ranges::variant<int>;
            STATIC_ASSERT(std::is_constructible<V, ranges::in_place_type_t<int>, int>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_type_t<int>, int>());
        }
        {
            using V = ranges::variant<int, long, long long>;
            STATIC_ASSERT(std::is_constructible<V, ranges::in_place_type_t<long>, int>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_type_t<long>, int>());
        }
        {
            using V = ranges::variant<int, long, int *>;
            STATIC_ASSERT(std::is_constructible<V, ranges::in_place_type_t<int *>, int *>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_type_t<int *>, int *>());
        }
        { // duplicate type
            using V = ranges::variant<int, long, int>;
            STATIC_ASSERT(!std::is_constructible<V, ranges::in_place_type_t<int>, int>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_type_t<int>, int>());
        }
        { // args not convertible to type
            using V = ranges::variant<int, long, int *>;
            STATIC_ASSERT(!std::is_constructible<V, ranges::in_place_type_t<int>, int *>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_type_t<int>, int *>());
        }
        { // type not in variant
            using V = ranges::variant<int, long, int *>;
            STATIC_ASSERT(!std::is_constructible<V, ranges::in_place_type_t<long long>, int>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_type_t<long long>, int>());
        }
    }

    void test_ctor_basic()
    {
        {
            constexpr ranges::variant<int> v(ranges::in_place_type<int>, 42);
            STATIC_ASSERT(v.index() == 0);
            STATIC_ASSERT(ranges::get<0>(v) == 42);
        }
        {
            constexpr ranges::variant<int, long> v(ranges::in_place_type<long>, 42);
            STATIC_ASSERT(v.index() == 1);
            STATIC_ASSERT(ranges::get<1>(v) == 42);
        }
        {
            constexpr ranges::variant<int, const int, long> v(
                    ranges::in_place_type<const int>, 42);
            STATIC_ASSERT(v.index() == 1);
            STATIC_ASSERT(ranges::get<1>(v) == 42);
        }
        {
            using V = ranges::variant<const int, volatile int, int>;
            int x = 42;
            V v(ranges::in_place_type<const int>, x);
            CHECK(v.index() == 0u);
            CHECK(ranges::get<0>(v) == x);
        }
        {
            using V = ranges::variant<const int, volatile int, int>;
            int x = 42;
            V v(ranges::in_place_type<volatile int>, x);
            CHECK(v.index() == 1u);
            CHECK(ranges::get<1>(v) == x);
        }
        {
            using V = ranges::variant<const int, volatile int, int>;
            int x = 42;
            V v(ranges::in_place_type<int>, x);
            CHECK(v.index() == 2u);
            CHECK(ranges::get<2>(v) == x);
        }
    }

    void test()
    {
        test_ctor_basic();
        test_ctor_sfinae();
    }
} // namespace in_place_type_ctor

namespace in_place_type_il_ctor
{
    struct InitList
    {
        std::size_t size;
        constexpr InitList(std::initializer_list<int> il)
          : size(il.size())
        {}
    };

    struct InitListArg
    {
        std::size_t size;
        int value;
        constexpr InitListArg(std::initializer_list<int> il, int v)
          : size(il.size()), value(v)
        {}
    };

    void test_ctor_sfinae()
    {
        using IL = std::initializer_list<int>;
        { // just init list
            using V = ranges::variant<InitList, InitListArg, int>;
            STATIC_ASSERT(std::is_constructible<V, ranges::in_place_type_t<InitList>, IL>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_type_t<InitList>, IL>());
        }
        { // too many arguments
            using V = ranges::variant<InitList, InitListArg, int>;
            STATIC_ASSERT(!std::is_constructible<V, ranges::in_place_type_t<InitList>, IL, int>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_type_t<InitList>, IL, int>());
        }
        { // too few arguments
            using V = ranges::variant<InitList, InitListArg, int>;
            STATIC_ASSERT(!std::is_constructible<V, ranges::in_place_type_t<InitListArg>, IL>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_type_t<InitListArg>, IL>());
        }
        { // init list and arguments
            using V = ranges::variant<InitList, InitListArg, int>;
            STATIC_ASSERT(std::is_constructible<V, ranges::in_place_type_t<InitListArg>, IL, int>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_type_t<InitListArg>, IL, int>());
        }
        { // not constructible from arguments
            using V = ranges::variant<InitList, InitListArg, int>;
            STATIC_ASSERT(!std::is_constructible<V, ranges::in_place_type_t<int>, IL>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_type_t<int>, IL>());
        }
        { // duplicate types in variant
            using V = ranges::variant<InitListArg, InitListArg, int>;
            STATIC_ASSERT(!std::is_constructible<V, ranges::in_place_type_t<InitListArg>, IL, int>::value);
            STATIC_ASSERT(!test_convertible<V, ranges::in_place_type_t<InitListArg>, IL, int>());
        }
    }

    void test_ctor_basic()
    {
        {
            constexpr ranges::variant<InitList, InitListArg> v(
                ranges::in_place_type<InitList>, {1, 2, 3});
            STATIC_ASSERT(v.index() == 0);
            STATIC_ASSERT(ranges::get<0>(v).size == 3);
        }
        {
            constexpr ranges::variant<InitList, InitListArg> v(
                ranges::in_place_type<InitListArg>, {1, 2, 3, 4}, 42);
            STATIC_ASSERT(v.index() == 1);
            STATIC_ASSERT(ranges::get<1>(v).size == 4);
            STATIC_ASSERT(ranges::get<1>(v).value == 42);
        }
    }

    void test()
    {
        test_ctor_basic();
        test_ctor_sfinae();
    }
} // namespace in_place_type_il_ctor

namespace converting_ctor {
    struct Dummy { Dummy() = default; };

    struct ThrowsT { ThrowsT(int) noexcept(false) {} };

    struct NoThrowT { NoThrowT(int) noexcept(true) {} };

    struct AnyConstructible { template <typename T> AnyConstructible(T &&) {} };
    struct NoConstructible { NoConstructible() = delete; };

    void test_T_ctor_noexcept()
    {
        {
            using V = ranges::variant<Dummy, NoThrowT>;
            STATIC_ASSERT(std::is_nothrow_constructible<V, int>::value);
        }
        {
            using V = ranges::variant<Dummy, ThrowsT>;
            STATIC_ASSERT(!std::is_nothrow_constructible<V, int>::value);
        }
    }

    void test_T_ctor_sfinae() {
        {
            using V = ranges::variant<long, unsigned>;
            static_assert(!std::is_constructible<V, int>::value, "ambiguous");
        }
        {
            using V = ranges::variant<std::string, std::string>;
            static_assert(!std::is_constructible<V, const char *>::value, "ambiguous");
        }
        {
            using V = ranges::variant<std::string, void *>;
            static_assert(!std::is_constructible<V, int>::value, "no matching constructor");
        }
        {
            using V = ranges::variant<AnyConstructible, NoConstructible>;
            static_assert(
                !std::is_constructible<V, ranges::in_place_type_t<NoConstructible>>::value,
                "no matching constructor");
            static_assert(!std::is_constructible<V, ranges::in_place_index_t<1>>::value,
                "no matching constructor");
        }

        {
            using V = ranges::variant<int, int &&>;
            static_assert(!std::is_constructible<V, int>::value, "ambiguous");
        }
        {
            using V = ranges::variant<int, const int &>;
            static_assert(!std::is_constructible<V, int>::value, "ambiguous");
        }
    }

    void test_T_ctor_basic() {
        {
            constexpr ranges::variant<int> v(42);
            STATIC_ASSERT(v.index() == 0);
            STATIC_ASSERT(ranges::get<0>(v) == 42);
        }
        {
            constexpr ranges::variant<int, long> v(42l);
            STATIC_ASSERT(v.index() == 1);
            STATIC_ASSERT(ranges::get<1>(v) == 42);
        }
        {
            using V = ranges::variant<const int &, int &&, long>;
            static_assert(std::is_convertible<int &, V>::value, "must be implicit");
            int x = 42;
            V v(x);
            CHECK(v.index() == 0u);
            CHECK(&ranges::get<0>(v) == &x);
        }
        {
            using V = ranges::variant<const int &, int &&, long>;
            static_assert(std::is_convertible<int, V>::value, "must be implicit");
            int x = 42;
            V v(std::move(x));
            CHECK(v.index() == 1u);
            CHECK(&ranges::get<1>(v) == &x);
        }
    }

    void test()
    {
        test_T_ctor_basic();
        test_T_ctor_noexcept();
        test_T_ctor_sfinae();
    }
} // namespace converting_ctor

namespace dtor
{
    struct NonTDtor
    {
        static int count;
        NonTDtor() = default;
        ~NonTDtor() { ++count; }
    };
    int NonTDtor::count = 0;
    STATIC_ASSERT(!std::is_trivially_destructible<NonTDtor>::value);

    struct NonTDtor1
    {
        static int count;
        NonTDtor1() = default;
        ~NonTDtor1() { ++count; }
    };
    int NonTDtor1::count = 0;
    STATIC_ASSERT(!std::is_trivially_destructible<NonTDtor1>::value);

    struct TDtor
    {
        TDtor(const TDtor &) {} // non-trivial copy
        ~TDtor() = default;
    };
    STATIC_ASSERT(!ranges::detail::is_trivially_copy_constructible<TDtor>::value);
    STATIC_ASSERT(std::is_trivially_destructible<TDtor>::value);

    void test()
    {
        {
            using V = ranges::variant<int, long, TDtor>;
            STATIC_ASSERT(std::is_trivially_destructible<V>::value);
        }
        {
            using V = ranges::variant<NonTDtor, int, NonTDtor1>;
            STATIC_ASSERT(!std::is_trivially_destructible<V>::value);
            {
                V v(ranges::in_place_index<0>);
                CHECK(NonTDtor::count == 0);
                CHECK(NonTDtor1::count == 0);
            }
            CHECK(NonTDtor::count == 1);
            CHECK(NonTDtor1::count == 0);
            NonTDtor::count = 0;
            { V v(ranges::in_place_index<1>); }
            CHECK(NonTDtor::count == 0);
            CHECK(NonTDtor1::count == 0);
            {
                V v(ranges::in_place_index<2>);
                CHECK(NonTDtor::count == 0);
                CHECK(NonTDtor1::count == 0);
            }
            CHECK(NonTDtor::count == 0);
            CHECK(NonTDtor1::count == 1);
        }
    }
} // namespace dtor

namespace copy_assign
{
    struct NoCopy
    {
        NoCopy(const NoCopy &) = delete;
        NoCopy &operator=(const NoCopy &) = default;
    };

    struct CopyOnly
    {
        CopyOnly(const CopyOnly &) = default;
        CopyOnly(CopyOnly &&) = delete;
        CopyOnly &operator=(const CopyOnly &) = default;
        CopyOnly &operator=(CopyOnly &&) = delete;
    };

    struct MoveOnly
    {
        MoveOnly(const MoveOnly &) = delete;
        MoveOnly(MoveOnly &&) = default;
        MoveOnly &operator=(const MoveOnly &) = default;
    };

    struct MoveOnlyNT
    {
        MoveOnlyNT(const MoveOnlyNT &) = delete;
        MoveOnlyNT(MoveOnlyNT &&) {}
        MoveOnlyNT &operator=(const MoveOnlyNT &) = default;
    };

    struct CopyAssign
    {
        static int alive;
        static int copy_construct;
        static int copy_assign;
        static int move_construct;
        static int move_assign;
        static void reset() {
            copy_construct = copy_assign = move_construct = move_assign = alive = 0;
        }
        CopyAssign(int v) : value(v) { ++alive; }
        CopyAssign(const CopyAssign &o) : value(o.value) {
            ++alive;
            ++copy_construct;
        }
        CopyAssign(CopyAssign &&o) noexcept : value(o.value) {
            o.value = -1;
            ++alive;
            ++move_construct;
        }
        CopyAssign &operator=(const CopyAssign &o) {
            value = o.value;
            ++copy_assign;
            return *this;
        }
        CopyAssign &operator=(CopyAssign &&o) noexcept {
            value = o.value;
            o.value = -1;
            ++move_assign;
            return *this;
        }
        ~CopyAssign() { --alive; }
        int value;
    };

    int CopyAssign::alive = 0;
    int CopyAssign::copy_construct = 0;
    int CopyAssign::copy_assign = 0;
    int CopyAssign::move_construct = 0;
    int CopyAssign::move_assign = 0;

    struct CopyMaybeThrows
    {
        CopyMaybeThrows(const CopyMaybeThrows &);
        CopyMaybeThrows &operator=(const CopyMaybeThrows &);
    };
    struct CopyDoesThrow
    {
        CopyDoesThrow(const CopyDoesThrow &) noexcept(false);
        CopyDoesThrow &operator=(const CopyDoesThrow &) noexcept(false);
    };


    struct NTCopyAssign
    {
        constexpr NTCopyAssign(int v) : value(v) {}
        NTCopyAssign(const NTCopyAssign &) = default;
        NTCopyAssign(NTCopyAssign &&) = default;
        NTCopyAssign &operator=(const NTCopyAssign &that) {
            value = that.value;
            return *this;
        }
        NTCopyAssign &operator=(NTCopyAssign &&) = delete;
        int value;
    };

    STATIC_ASSERT(!ranges::detail::is_trivially_copy_assignable<NTCopyAssign>::value);
    STATIC_ASSERT(std::is_copy_assignable<NTCopyAssign>::value);

    struct TCopyAssign
    {
        constexpr TCopyAssign(int v) : value(v) {}
        TCopyAssign(const TCopyAssign &) = default;
        TCopyAssign(TCopyAssign &&) = default;
        TCopyAssign &operator=(const TCopyAssign &) = default;
        TCopyAssign &operator=(TCopyAssign &&) = delete;
        int value;
    };

    STATIC_ASSERT(!RANGES_PROPER_TRIVIAL_TRAITS ||
        ranges::detail::is_trivially_copy_assignable<TCopyAssign>::value);

    struct TCopyAssignNTMoveAssign
    {
        constexpr TCopyAssignNTMoveAssign(int v) : value(v) {}
        TCopyAssignNTMoveAssign(const TCopyAssignNTMoveAssign &) = default;
        TCopyAssignNTMoveAssign(TCopyAssignNTMoveAssign &&) = default;
        TCopyAssignNTMoveAssign &operator=(const TCopyAssignNTMoveAssign &) = default;
        TCopyAssignNTMoveAssign &operator=(TCopyAssignNTMoveAssign &&that) {
            value = that.value;
            that.value = -1;
            return *this;
        }
        int value;
    };

    STATIC_ASSERT(!RANGES_PROPER_TRIVIAL_TRAITS ||
        ranges::detail::is_trivially_copy_assignable<TCopyAssignNTMoveAssign>::value);

    struct CopyThrows
    {
        CopyThrows() = default;
        [[noreturn]] CopyThrows(const CopyThrows &) { throw 42; }
        CopyThrows &operator=(const CopyThrows &) { throw 42; }
    };

    struct CopyCannotThrow
    {
        static int alive;
        CopyCannotThrow() { ++alive; }
        CopyCannotThrow(const CopyCannotThrow &) noexcept { ++alive; }
        CopyCannotThrow(CopyCannotThrow &&) noexcept { CHECK(false); }
        CopyCannotThrow &operator=(const CopyCannotThrow &) noexcept = default;
        CopyCannotThrow &operator=(CopyCannotThrow &&) noexcept { CHECK(false); return *this; }
    };

    int CopyCannotThrow::alive = 0;

    struct MoveThrows
    {
        static int alive;
        MoveThrows() { ++alive; }
        MoveThrows(const MoveThrows &) { ++alive; }
        [[noreturn]] MoveThrows(MoveThrows &&) { throw 42; }
        MoveThrows &operator=(const MoveThrows &) { return *this; }
        MoveThrows &operator=(MoveThrows &&) { throw 42; }
        ~MoveThrows() { --alive; }
    };

    int MoveThrows::alive = 0;

    void test_copy_assignment_not_noexcept()
    {
        {
            using V = ranges::variant<CopyMaybeThrows>;
            STATIC_ASSERT(!std::is_nothrow_copy_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, CopyDoesThrow>;
            STATIC_ASSERT(!std::is_nothrow_copy_assignable<V>::value);
        }
    }

    void test_copy_assignment_sfinae()
    {
        {
            using V = ranges::variant<int, long>;
            STATIC_ASSERT(std::is_copy_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, CopyOnly>;
            STATIC_ASSERT(std::is_copy_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, NoCopy>;
            STATIC_ASSERT(!std::is_copy_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, MoveOnly>;
            STATIC_ASSERT(!std::is_copy_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, MoveOnlyNT>;
            STATIC_ASSERT(!std::is_copy_assignable<V>::value);
        }

#if RANGES_PROPER_TRIVIAL_TRAITS
        // The following tests are for not-yet-standardized behavior (P0602):
        {
            using V = ranges::variant<int, long>;
            STATIC_ASSERT(ranges::detail::is_trivially_copy_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, NTCopyAssign>;
            STATIC_ASSERT(!ranges::detail::is_trivially_copy_assignable<V>::value);
            STATIC_ASSERT(std::is_copy_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, TCopyAssign>;
            STATIC_ASSERT(ranges::detail::is_trivially_copy_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, TCopyAssignNTMoveAssign>;
            STATIC_ASSERT(ranges::detail::is_trivially_copy_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, CopyOnly>;
            STATIC_ASSERT(ranges::detail::is_trivially_copy_assignable<V>::value);
        }
#endif // RANGES_PROPER_TRIVIAL_TRAITS
    }

    void test_copy_assignment_empty_empty()
    {
        using MET = MakeEmptyT;
        {
            using V = ranges::variant<int, long, MET>;
            V v1(ranges::in_place_index<0>);
            makeEmpty(v1);
            V v2(ranges::in_place_index<0>);
            makeEmpty(v2);
            V &vref = (v1 = v2);
            CHECK(&vref == &v1);
            CHECK(v1.valueless_by_exception());
            CHECK(v1.index() == ranges::variant_npos);
        }
    }

    void test_copy_assignment_non_empty_empty()
    {
        using MET = MakeEmptyT;
        {
            using V = ranges::variant<int, MET>;
            V v1(ranges::in_place_index<0>, 42);
            V v2(ranges::in_place_index<0>);
            makeEmpty(v2);
            V &vref = (v1 = v2);
            CHECK(&vref == &v1);
            CHECK(v1.valueless_by_exception());
            CHECK(v1.index() == ranges::variant_npos);
        }
        {
            using V = ranges::variant<int, MET, std::string>;
            V v1(ranges::in_place_index<2>, "hello");
            V v2(ranges::in_place_index<0>);
            makeEmpty(v2);
            V &vref = (v1 = v2);
            CHECK(&vref == &v1);
            CHECK(v1.valueless_by_exception());
            CHECK(v1.index() == ranges::variant_npos);
        }
    }

    void test_copy_assignment_empty_non_empty()
    {
        using MET = MakeEmptyT;
        {
            using V = ranges::variant<int, MET>;
            V v1(ranges::in_place_index<0>);
            makeEmpty(v1);
            V v2(ranges::in_place_index<0>, 42);
            V &vref = (v1 = v2);
            CHECK(&vref == &v1);
            CHECK(v1.index() == 0u);
            CHECK(ranges::get<0>(v1) == 42);
        }
        {
            using V = ranges::variant<int, MET, std::string>;
            V v1(ranges::in_place_index<0>);
            makeEmpty(v1);
            V v2(ranges::in_place_type<std::string>, "hello");
            V &vref = (v1 = v2);
            CHECK(&vref == &v1);
            CHECK(v1.index() == 2u);
            CHECK(ranges::get<2>(v1) == "hello");
        }
    }

    template <typename T> struct Result { size_t index; T value; };

    void test_copy_assignment_same_index() {
        {
            using V = ranges::variant<int>;
            V v1(43);
            V v2(42);
            V &vref = (v1 = v2);
            CHECK(&vref == &v1);
            CHECK(v1.index() == 0u);
            CHECK(ranges::get<0>(v1) == 42);
        }
        {
            using V = ranges::variant<int, long, unsigned>;
            V v1(43l);
            V v2(42l);
            V &vref = (v1 = v2);
            CHECK(&vref == &v1);
            CHECK(v1.index() == 1u);
            CHECK(ranges::get<1>(v1) == 42);
        }
        {
            using V = ranges::variant<int, CopyAssign, unsigned>;
            V v1(ranges::in_place_type<CopyAssign>, 43);
            V v2(ranges::in_place_type<CopyAssign>, 42);
            CopyAssign::reset();
            V &vref = (v1 = v2);
            CHECK(&vref == &v1);
            CHECK(v1.index() == 1u);
            CHECK(ranges::get<1>(v1).value == 42);
            CHECK(CopyAssign::copy_construct == 0);
            CHECK(CopyAssign::move_construct == 0);
            CHECK(CopyAssign::copy_assign == 1);
        }
        using MET = MakeEmptyT;
        {
            using V = ranges::variant<int, MET, std::string>;
            V v1(ranges::in_place_type<MET>);
            MET &mref = ranges::get<1>(v1);
            V v2(ranges::in_place_type<MET>);
            try {
                v1 = v2;
                CHECK(false);
            } catch (...) {
            }
            CHECK(v1.index() == 1u);
            CHECK(&ranges::get<1>(v1) == &mref);
        }

#if RANGES_CXX_CONSTEXPR >= RANGES_CXX_CONSTEXPR_14
        // The following tests are for not-yet-standardized behavior (P0602):
        {
            struct {
                constexpr Result<int> operator()() const {
                    using V = ranges::variant<int>;
                    V v(43);
                    V v2(42);
                    v = v2;
                    return {v.index(), ranges::get<0>(v)};
                }
            } test;
            constexpr auto result = test();
            STATIC_ASSERT(result.index == 0);
            STATIC_ASSERT(result.value == 42);
        }
        {
            struct {
                constexpr Result<long> operator()() const {
                    using V = ranges::variant<int, long, unsigned>;
                    V v(43l);
                    V v2(42l);
                    v = v2;
                    return {v.index(), ranges::get<1>(v)};
                }
            } test;
            constexpr auto result = test();
            STATIC_ASSERT(result.index == 1);
            STATIC_ASSERT(result.value == 42l);
        }
        {
            struct {
                constexpr Result<int> operator()() const {
                    using V = ranges::variant<int, TCopyAssign, unsigned>;
                    V v(ranges::in_place_type<TCopyAssign>, 43);
                    V v2(ranges::in_place_type<TCopyAssign>, 42);
                    v = v2;
                    return {v.index(), ranges::get<1>(v).value};
                }
            } test;
            constexpr auto result = test();
            STATIC_ASSERT(result.index == 1);
            STATIC_ASSERT(result.value == 42);
        }
        {
            struct {
                constexpr Result<int> operator()() const {
                    using V = ranges::variant<int, TCopyAssignNTMoveAssign, unsigned>;
                    V v(ranges::in_place_type<TCopyAssignNTMoveAssign>, 43);
                    V v2(ranges::in_place_type<TCopyAssignNTMoveAssign>, 42);
                    v = v2;
                    return {v.index(), ranges::get<1>(v).value};
                }
            } test;
            constexpr auto result = test();
            STATIC_ASSERT(result.index == 1);
            STATIC_ASSERT(result.value == 42);
        }
#endif
    }

    void test_copy_assignment_different_index() {
        {
            using V = ranges::variant<int, long, unsigned>;
            V v1(43);
            V v2(42l);
            V &vref = (v1 = v2);
            CHECK(&vref == &v1);
            CHECK(v1.index() == 1u);
            CHECK(ranges::get<1>(v1) == 42);
        }
        {
            using V = ranges::variant<int, CopyAssign, unsigned>;
            CopyAssign::reset();
            V v1(ranges::in_place_type<unsigned>, 43u);
            V v2(ranges::in_place_type<CopyAssign>, 42);
            CHECK(CopyAssign::copy_construct == 0);
            CHECK(CopyAssign::move_construct == 0);
            CHECK(CopyAssign::alive == 1);
            V &vref = (v1 = v2);
            CHECK(&vref == &v1);
            CHECK(v1.index() == 1u);
            CHECK(ranges::get<1>(v1).value == 42);
            CHECK(CopyAssign::alive == 2);
            CHECK(CopyAssign::copy_construct == 1);
            CHECK(CopyAssign::move_construct == 1);
            CHECK(CopyAssign::copy_assign == 0);
        }
    #ifndef TEST_HAS_NO_EXCEPTIONS
        {
            using V = ranges::variant<int, CopyThrows, std::string>;
            V v1(ranges::in_place_type<std::string>, "hello");
            V v2(ranges::in_place_type<CopyThrows>);
            try {
                v1 = v2;
                CHECK(false);
            } catch (...) { /* ... */
            }
            // Test that copy construction is used directly if move construction may throw,
            // resulting in a valueless variant if copy throws.
            CHECK(v1.valueless_by_exception());
        }
        {
            using V = ranges::variant<int, MoveThrows, std::string>;
            V v1(ranges::in_place_type<std::string>, "hello");
            V v2(ranges::in_place_type<MoveThrows>);
            CHECK(MoveThrows::alive == 1);
            // Test that copy construction is used directly if move construction may throw.
            v1 = v2;
            CHECK(v1.index() == 1u);
            CHECK(v2.index() == 1u);
            CHECK(MoveThrows::alive == 2);
        }
        {
            // Test that direct copy construction is preferred when it cannot throw.
            using V = ranges::variant<int, CopyCannotThrow, std::string>;
            V v1(ranges::in_place_type<std::string>, "hello");
            V v2(ranges::in_place_type<CopyCannotThrow>);
            CHECK(CopyCannotThrow::alive == 1);
            v1 = v2;
            CHECK(v1.index() == 1u);
            CHECK(v2.index() == 1u);
            CHECK(CopyCannotThrow::alive == 2);
        }
        {
            using V = ranges::variant<int, CopyThrows, std::string>;
            V v1(ranges::in_place_type<CopyThrows>);
            V v2(ranges::in_place_type<std::string>, "hello");
            V &vref = (v1 = v2);
            CHECK(&vref == &v1);
            CHECK(v1.index() == 2u);
            CHECK(ranges::get<2>(v1) == "hello");
            CHECK(v2.index() == 2u);
            CHECK(ranges::get<2>(v2) == "hello");
        }
        {
            using V = ranges::variant<int, MoveThrows, std::string>;
            V v1(ranges::in_place_type<MoveThrows>);
            V v2(ranges::in_place_type<std::string>, "hello");
            V &vref = (v1 = v2);
            CHECK(&vref == &v1);
            CHECK(v1.index() == 2u);
            CHECK(ranges::get<2>(v1) == "hello");
            CHECK(v2.index() == 2u);
            CHECK(ranges::get<2>(v2) == "hello");
        }
    #endif // TEST_HAS_NO_EXCEPTIONS

#if RANGES_CXX_CONSTEXPR >= RANGES_CXX_CONSTEXPR_14
        // The following tests are for not-yet-standardized behavior (P0602):
        {
            struct {
                constexpr Result<long> operator()() const {
                    using V = ranges::variant<int, long, unsigned>;
                    V v(43);
                    V v2(42l);
                    v = v2;
                    return {v.index(), ranges::get<1>(v)};
                }
            } test;
            constexpr auto result = test();
            STATIC_ASSERT(result.index == 1);
            STATIC_ASSERT(result.value == 42l);
        }
        {
            struct {
                constexpr Result<int> operator()() const {
                    using V = ranges::variant<int, TCopyAssign, unsigned>;
                    V v(ranges::in_place_type<unsigned>, 43u);
                    V v2(ranges::in_place_type<TCopyAssign>, 42);
                    v = v2;
                    return {v.index(), ranges::get<1>(v).value};
                }
            } test;
            constexpr auto result = test();
            STATIC_ASSERT(result.index == 1);
            STATIC_ASSERT(result.value == 42);
        }
#endif
    }

#if RANGES_CXX_CONSTEXPR >= RANGES_CXX_CONSTEXPR_14
    template <size_t NewIdx, class ValueType>
    constexpr bool test_constexpr_assign_extension_imp(
            ranges::variant<long, void*, int>&& v, ValueType&& new_value)
    {
        const ranges::variant<long, void*, int> cp(
                std::forward<ValueType>(new_value));
        v = cp;
        return v.index() == NewIdx && ranges::get<NewIdx>(v) == ranges::get<NewIdx>(cp);
    }
#endif // RANGES_CXX_CONSTEXPR

    void test_constexpr_copy_assignment_extension()
    {
        // The following tests are for not-yet-standardized behavior (P0602):
        using V = ranges::variant<long, void*, int>;
        STATIC_ASSERT(!RANGES_PROPER_TRIVIAL_TRAITS ||
            ranges::detail::is_trivially_copyable<V>::value);
        STATIC_ASSERT(!RANGES_PROPER_TRIVIAL_TRAITS ||
            ranges::detail::is_trivially_copy_assignable<V>::value);
#if RANGES_CXX_CONSTEXPR >= RANGES_CXX_CONSTEXPR_14
        STATIC_ASSERT(test_constexpr_assign_extension_imp<0>(V(42l), 101l));
        STATIC_ASSERT(test_constexpr_assign_extension_imp<0>(V(nullptr), 101l));
        STATIC_ASSERT(test_constexpr_assign_extension_imp<1>(V(42l), nullptr));
        STATIC_ASSERT(test_constexpr_assign_extension_imp<2>(V(42l), 101));
#endif
    }

    void test()
    {
        test_copy_assignment_empty_empty();
        test_copy_assignment_non_empty_empty();
        test_copy_assignment_empty_non_empty();
        test_copy_assignment_same_index();
        test_copy_assignment_different_index();
        test_copy_assignment_sfinae();
        test_copy_assignment_not_noexcept();
        test_constexpr_copy_assignment_extension();
    }
} // namespace copy_assign

namespace move_assign
{
    struct NoCopy
    {
        NoCopy(const NoCopy &) = delete;
        NoCopy &operator=(const NoCopy &) = default;
    };

    struct CopyOnly
    {
        CopyOnly(const CopyOnly &) = default;
        CopyOnly(CopyOnly &&) = delete;
        CopyOnly &operator=(const CopyOnly &) = default;
        CopyOnly &operator=(CopyOnly &&) = delete;
    };

    struct MoveOnly
    {
        MoveOnly(const MoveOnly &) = delete;
        MoveOnly(MoveOnly &&) = default;
        MoveOnly &operator=(const MoveOnly &) = delete;
        MoveOnly &operator=(MoveOnly &&) = default;
    };

    struct MoveOnlyNT
    {
        MoveOnlyNT(const MoveOnlyNT &) = delete;
        MoveOnlyNT(MoveOnlyNT &&) {}
        MoveOnlyNT &operator=(const MoveOnlyNT &) = delete;
        MoveOnlyNT &operator=(MoveOnlyNT &&) = default;
    };

    struct MoveOnlyOddNothrow
    {
        MoveOnlyOddNothrow(MoveOnlyOddNothrow &&) noexcept(false) {}
        MoveOnlyOddNothrow(const MoveOnlyOddNothrow &) = delete;
        MoveOnlyOddNothrow &operator=(MoveOnlyOddNothrow &&) noexcept = default;
        MoveOnlyOddNothrow &operator=(const MoveOnlyOddNothrow &) = delete;
    };

    struct MoveAssignOnly
    {
        MoveAssignOnly(MoveAssignOnly &&) = delete;
        MoveAssignOnly &operator=(MoveAssignOnly &&) = default;
    };

    struct MoveAssign
    {
        static int move_construct;
        static int move_assign;
        static void reset() { move_construct = move_assign = 0; }
        MoveAssign(int v) : value(v) {}
        MoveAssign(MoveAssign &&o) : value(o.value) {
            ++move_construct;
            o.value = -1;
        }
        MoveAssign &operator=(MoveAssign &&o) {
            value = o.value;
            ++move_assign;
            o.value = -1;
            return *this;
        }
        int value;
    };

    int MoveAssign::move_construct = 0;
    int MoveAssign::move_assign = 0;

    struct NTMoveAssign
    {
        constexpr NTMoveAssign(int v) : value(v) {}
        NTMoveAssign(const NTMoveAssign &) = default;
        NTMoveAssign(NTMoveAssign &&) = default;
        NTMoveAssign &operator=(const NTMoveAssign &that) = default;
        NTMoveAssign &operator=(NTMoveAssign &&that) {
            value = that.value;
            that.value = -1;
            return *this;
        }
        int value;
    };

    STATIC_ASSERT(!ranges::detail::is_trivially_move_assignable<NTMoveAssign>::value);
    STATIC_ASSERT(std::is_move_assignable<NTMoveAssign>::value);

    struct TMoveAssign
    {
        constexpr TMoveAssign(int v) : value(v) {}
        TMoveAssign(const TMoveAssign &) = delete;
        TMoveAssign(TMoveAssign &&) = default;
        TMoveAssign &operator=(const TMoveAssign &) = delete;
        TMoveAssign &operator=(TMoveAssign &&) = default;
        int value;
    };

    STATIC_ASSERT(!RANGES_PROPER_TRIVIAL_TRAITS ||
        ranges::detail::is_trivially_move_assignable<TMoveAssign>::value);

    struct TMoveAssignNTCopyAssign
    {
        constexpr TMoveAssignNTCopyAssign(int v) : value(v) {}
        TMoveAssignNTCopyAssign(const TMoveAssignNTCopyAssign &) = default;
        TMoveAssignNTCopyAssign(TMoveAssignNTCopyAssign &&) = default;
        TMoveAssignNTCopyAssign &operator=(const TMoveAssignNTCopyAssign &that) {
            value = that.value;
            return *this;
        }
        TMoveAssignNTCopyAssign &operator=(TMoveAssignNTCopyAssign &&) = default;
        int value;
    };

    STATIC_ASSERT(~RANGES_PROPER_TRIVIAL_TRAITS ||
        ranges::detail::is_trivially_move_assignable<TMoveAssignNTCopyAssign>::value);

    struct TrivialCopyNontrivialMove
    {
        TrivialCopyNontrivialMove(TrivialCopyNontrivialMove const&) = default;
        TrivialCopyNontrivialMove(TrivialCopyNontrivialMove&&) noexcept {}
        TrivialCopyNontrivialMove& operator=(TrivialCopyNontrivialMove const&) = default;
        TrivialCopyNontrivialMove& operator=(TrivialCopyNontrivialMove&&) noexcept {
            return *this;
        }
    };

    STATIC_ASSERT(!RANGES_PROPER_TRIVIAL_TRAITS ||
        ranges::detail::is_trivially_copy_assignable<TrivialCopyNontrivialMove>::value);
    STATIC_ASSERT(!ranges::detail::is_trivially_move_assignable<TrivialCopyNontrivialMove>::value);

    void test_move_assignment_noexcept()
    {
        {
            using V = ranges::variant<int>;
            STATIC_ASSERT(std::is_nothrow_move_assignable<V>::value);
        }
        {
            using V = ranges::variant<MoveOnly>;
            STATIC_ASSERT(std::is_nothrow_move_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, long>;
            STATIC_ASSERT(std::is_nothrow_move_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, MoveOnly>;
            STATIC_ASSERT(std::is_nothrow_move_assignable<V>::value);
        }
        {
            using V = ranges::variant<MoveOnlyNT>;
            STATIC_ASSERT(!std::is_nothrow_move_assignable<V>::value);
        }
        {
            using V = ranges::variant<MoveOnlyOddNothrow>;
            STATIC_ASSERT(!std::is_nothrow_move_assignable<V>::value);
        }
    }

    void test_move_assignment_sfinae()
    {
        {
            using V = ranges::variant<int, long>;
            STATIC_ASSERT(std::is_move_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, CopyOnly>;
            STATIC_ASSERT(std::is_move_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, NoCopy>;
            STATIC_ASSERT(!std::is_move_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, MoveOnly>;
            STATIC_ASSERT(std::is_move_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, MoveOnlyNT>;
            STATIC_ASSERT(std::is_move_assignable<V>::value);
        }
        {
            // variant only provides move assignment when the types also provide
            // a move constructor.
            using V = ranges::variant<int, MoveAssignOnly>;
            STATIC_ASSERT(!std::is_move_assignable<V>::value);
        }

#if RANGES_PROPER_TRIVIAL_TRAITS
        // The following tests are for not-yet-standardized behavior (P0602):
        {
            using V = ranges::variant<int, long>;
            STATIC_ASSERT(ranges::detail::is_trivially_move_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, NTMoveAssign>;
            STATIC_ASSERT(!ranges::detail::is_trivially_move_assignable<V>::value);
            STATIC_ASSERT(std::is_move_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, TMoveAssign>;
            STATIC_ASSERT(ranges::detail::is_trivially_move_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, TMoveAssignNTCopyAssign>;
            STATIC_ASSERT(ranges::detail::is_trivially_move_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, TrivialCopyNontrivialMove>;
            STATIC_ASSERT(!ranges::detail::is_trivially_move_assignable<V>::value);
        }
        {
            using V = ranges::variant<int, CopyOnly>;
            STATIC_ASSERT(ranges::detail::is_trivially_move_assignable<V>::value);
        }
#endif // RANGES_PROPER_TRIVIAL_TRAITS
    }

    void test_move_assignment_empty_empty()
    {
        using MET = MakeEmptyT;
        {
            using V = ranges::variant<int, long, MET>;
            V v1(ranges::in_place_index<0>);
            makeEmpty(v1);
            V v2(ranges::in_place_index<0>);
            makeEmpty(v2);
            V &vref = (v1 = std::move(v2));
            CHECK(&vref == &v1);
            CHECK(v1.valueless_by_exception());
            CHECK(v1.index() == ranges::variant_npos);
        }
    }

    void test_move_assignment_non_empty_empty()
    {
        using MET = MakeEmptyT;
        {
            using V = ranges::variant<int, MET>;
            V v1(ranges::in_place_index<0>, 42);
            V v2(ranges::in_place_index<0>);
            makeEmpty(v2);
            V &vref = (v1 = std::move(v2));
            CHECK(&vref == &v1);
            CHECK(v1.valueless_by_exception());
            CHECK(v1.index() == ranges::variant_npos);
        }
        {
            using V = ranges::variant<int, MET, std::string>;
            V v1(ranges::in_place_index<2>, "hello");
            V v2(ranges::in_place_index<0>);
            makeEmpty(v2);
            V &vref = (v1 = std::move(v2));
            CHECK(&vref == &v1);
            CHECK(v1.valueless_by_exception());
            CHECK(v1.index() == ranges::variant_npos);
        }
    }

    void test_move_assignment_empty_non_empty()
    {
        using MET = MakeEmptyT;
        {
            using V = ranges::variant<int, MET>;
            V v1(ranges::in_place_index<0>);
            makeEmpty(v1);
            V v2(ranges::in_place_index<0>, 42);
            V &vref = (v1 = std::move(v2));
            CHECK(&vref == &v1);
            CHECK(v1.index() == 0u);
            CHECK(ranges::get<0>(v1) == 42);
        }
        {
            using V = ranges::variant<int, MET, std::string>;
            V v1(ranges::in_place_index<0>);
            makeEmpty(v1);
            V v2(ranges::in_place_type<std::string>, "hello");
            V &vref = (v1 = std::move(v2));
            CHECK(&vref == &v1);
            CHECK(v1.index() == 2u);
            CHECK(ranges::get<2>(v1) == "hello");
        }
    }

    template <typename T> struct Result { size_t index; T value; };

    void test_move_assignment_same_index()
    {
        {
            using V = ranges::variant<int>;
            V v1(43);
            V v2(42);
            V &vref = (v1 = std::move(v2));
            CHECK(&vref == &v1);
            CHECK(v1.index() == 0u);
            CHECK(ranges::get<0>(v1) == 42);
        }
        {
            using V = ranges::variant<int, long, unsigned>;
            V v1(43l);
            V v2(42l);
            V &vref = (v1 = std::move(v2));
            CHECK(&vref == &v1);
            CHECK(v1.index() == 1u);
            CHECK(ranges::get<1>(v1) == 42);
        }
        {
            using V = ranges::variant<int, MoveAssign, unsigned>;
            V v1(ranges::in_place_type<MoveAssign>, 43);
            V v2(ranges::in_place_type<MoveAssign>, 42);
            MoveAssign::reset();
            V &vref = (v1 = std::move(v2));
            CHECK(&vref == &v1);
            CHECK(v1.index() == 1u);
            CHECK(ranges::get<1>(v1).value == 42);
            CHECK(MoveAssign::move_construct == 0);
            CHECK(MoveAssign::move_assign == 1);
        }

        using MET = MakeEmptyT;
        {
            using V = ranges::variant<int, MET, std::string>;
            V v1(ranges::in_place_type<MET>);
            MET &mref = ranges::get<1>(v1);
            V v2(ranges::in_place_type<MET>);
            try {
                v1 = std::move(v2);
                CHECK(false);
            } catch (...) {
            }
            CHECK(v1.index() == 1u);
            CHECK(&ranges::get<1>(v1) == &mref);
        }

#if RANGES_CXX_CONSTEXPR >= RANGES_CXX_CONSTEXPR_14
        // The following tests are for not-yet-standardized behavior (P0602):
        {
            struct {
                constexpr Result<int> operator()() const {
                    using V = ranges::variant<int>;
                    V v(43);
                    V v2(42);
                    v = std::move(v2);
                    return {v.index(), ranges::get<0>(v)};
                }
            } test;
            constexpr auto result = test();
            STATIC_ASSERT(result.index == 0);
            STATIC_ASSERT(result.value == 42);
        }
        {
            struct {
                constexpr Result<long> operator()() const {
                    using V = ranges::variant<int, long, unsigned>;
                    V v(43l);
                    V v2(42l);
                    v = std::move(v2);
                    return {v.index(), ranges::get<1>(v)};
                }
            } test;
            constexpr auto result = test();
            STATIC_ASSERT(result.index == 1);
            STATIC_ASSERT(result.value == 42l);
        }
        {
            struct {
                constexpr Result<int> operator()() const {
                    using V = ranges::variant<int, TMoveAssign, unsigned>;
                    V v(ranges::in_place_type<TMoveAssign>, 43);
                    V v2(ranges::in_place_type<TMoveAssign>, 42);
                    v = std::move(v2);
                    return {v.index(), ranges::get<1>(v).value};
                }
            } test;
            constexpr auto result = test();
            STATIC_ASSERT(result.index == 1);
            STATIC_ASSERT(result.value == 42);
        }
#endif // RANGES_CXX_CONSTEXPR
    }

    void test_move_assignment_different_index()
    {
        {
            using V = ranges::variant<int, long, unsigned>;
            V v1(43);
            V v2(42l);
            V &vref = (v1 = std::move(v2));
            CHECK(&vref == &v1);
            CHECK(v1.index() == 1u);
            CHECK(ranges::get<1>(v1) == 42);
        }
        {
            using V = ranges::variant<int, MoveAssign, unsigned>;
            V v1(ranges::in_place_type<unsigned>, 43u);
            V v2(ranges::in_place_type<MoveAssign>, 42);
            MoveAssign::reset();
            V &vref = (v1 = std::move(v2));
            CHECK(&vref == &v1);
            CHECK(v1.index() == 1u);
            CHECK(ranges::get<1>(v1).value == 42);
            CHECK(MoveAssign::move_construct == 1);
            CHECK(MoveAssign::move_assign == 0);
        }

        using MET = MakeEmptyT;
        {
            using V = ranges::variant<int, MET, std::string>;
            V v1(ranges::in_place_type<int>);
            V v2(ranges::in_place_type<MET>);
            try {
                v1 = std::move(v2);
                CHECK(false);
            } catch (...) {
            }
            CHECK(v1.valueless_by_exception());
            CHECK(v1.index() == ranges::variant_npos);
        }
        {
            using V = ranges::variant<int, MET, std::string>;
            V v1(ranges::in_place_type<MET>);
            V v2(ranges::in_place_type<std::string>, "hello");
            V &vref = (v1 = std::move(v2));
            CHECK(&vref == &v1);
            CHECK(v1.index() == 2u);
            CHECK(ranges::get<2>(v1) == "hello");
        }

#if RANGES_CXX_CONSTEXPR >= RANGES_CXX_CONSTEXPR_14
        // The following tests are for not-yet-standardized behavior (P0602):
        {
            struct {
                constexpr Result<long> operator()() const {
                    using V = ranges::variant<int, long, unsigned>;
                    V v(43);
                    V v2(42l);
                    v = std::move(v2);
                    return {v.index(), ranges::get<1>(v)};
                }
            } test;
            constexpr auto result = test();
            STATIC_ASSERT(result.index == 1);
            STATIC_ASSERT(result.value == 42l);
        }
        {
            struct {
                constexpr Result<long> operator()() const {
                    using V = ranges::variant<int, TMoveAssign, unsigned>;
                    V v(ranges::in_place_type<unsigned>, 43u);
                    V v2(ranges::in_place_type<TMoveAssign>, 42);
                    v = std::move(v2);
                    return {v.index(), ranges::get<1>(v).value};
                }
            } test;
            constexpr auto result = test();
            STATIC_ASSERT(result.index == 1);
            STATIC_ASSERT(result.value == 42);
        }
#endif // RANGES_CXX_CONSTEXPR
    }

#if RANGES_CXX_CONSTEXPR >= RANGES_CXX_CONSTEXPR_14
    template <size_t NewIdx, class ValueType>
    constexpr bool test_constexpr_assign_extension_imp(
            ranges::variant<long, void*, int>&& v, ValueType&& new_value)
    {
        ranges::variant<long, void*, int> v2(std::forward<ValueType>(new_value));
        const auto cp = v2;
        v = std::move(v2);
        return v.index() == NewIdx && ranges::get<NewIdx>(v) == ranges::get<NewIdx>(cp);
    }
#endif // RANGES_CXX_CONSTEXPR

    void test_constexpr_move_assignment_extension()
    {
        // The following tests are for not-yet-standardized behavior (P0602):
        using V = ranges::variant<long, void*, int>;
        STATIC_ASSERT(!RANGES_PROPER_TRIVIAL_TRAITS ||
            ranges::detail::is_trivially_copyable<V>::value);
        STATIC_ASSERT(!RANGES_PROPER_TRIVIAL_TRAITS ||
            ranges::detail::is_trivially_move_assignable<V>::value);
#if RANGES_CXX_CONSTEXPR >= RANGES_CXX_CONSTEXPR_14
        STATIC_ASSERT(test_constexpr_assign_extension_imp<0>(V(42l), 101l));
        STATIC_ASSERT(test_constexpr_assign_extension_imp<0>(V(nullptr), 101l));
        STATIC_ASSERT(test_constexpr_assign_extension_imp<1>(V(42l), nullptr));
        STATIC_ASSERT(test_constexpr_assign_extension_imp<2>(V(42l), 101));
#endif
    }

    void test()
    {
        test_move_assignment_empty_empty();
        test_move_assignment_non_empty_empty();
        test_move_assignment_empty_non_empty();
        test_move_assignment_same_index();
        test_move_assignment_different_index();
        test_move_assignment_sfinae();
        test_move_assignment_noexcept();
        test_constexpr_move_assignment_extension();
    }
} // namespace move_assign

namespace T_assign
{
    namespace MetaHelpers {
        struct Dummy
        {
            Dummy() = default;
        };

        struct ThrowsCtorT
        {
            ThrowsCtorT(int) noexcept(false) {}
            ThrowsCtorT &operator=(int) noexcept { return *this; }
        };

        struct ThrowsAssignT
        {
            ThrowsAssignT(int) noexcept {}
            ThrowsAssignT &operator=(int) noexcept(false) { return *this; }
        };

        struct NoThrowT
        {
            NoThrowT(int) noexcept {}
            NoThrowT &operator=(int) noexcept { return *this; }
        };
    } // namespace MetaHelpers

    namespace RuntimeHelpers {
        struct ThrowsCtorT
        {
            int value;
            ThrowsCtorT() : value(0) {}
            [[noreturn]] ThrowsCtorT(int) noexcept(false) { throw 42; }
            ThrowsCtorT &operator=(int v) noexcept {
                value = v;
                return *this;
            }
        };

        struct MoveCrashes
        {
            int value;
            MoveCrashes(int v = 0) noexcept : value{v} {}
            MoveCrashes(MoveCrashes &&) noexcept { CHECK(false); }
            MoveCrashes &operator=(MoveCrashes &&) noexcept { CHECK(false); return *this; }
            MoveCrashes &operator=(int v) noexcept {
                value = v;
                return *this;
            }
        };

        struct ThrowsCtorTandMove
        {
            int value;
            ThrowsCtorTandMove() : value(0) {}
            [[noreturn]] ThrowsCtorTandMove(int) noexcept(false) { throw 42; }
            ThrowsCtorTandMove(ThrowsCtorTandMove &&) noexcept(false) { CHECK(false); }
            ThrowsCtorTandMove &operator=(int v) noexcept {
                value = v;
                return *this;
            }
        };

        struct ThrowsAssignT
        {
            int value;
            ThrowsAssignT() : value(0) {}
            ThrowsAssignT(int v) noexcept : value(v) {}
            ThrowsAssignT &operator=(int) noexcept(false) { throw 42; }
        };

        struct NoThrowT
        {
            int value;
            NoThrowT() : value(0) {}
            NoThrowT(int v) noexcept : value(v) {}
            NoThrowT &operator=(int v) noexcept {
                value = v;
                return *this;
            }
        };
    } // namespace RuntimeHelpers

    void test_T_assignment_noexcept()
    {
        using namespace MetaHelpers;
        {
            using V = ranges::variant<Dummy, NoThrowT>;
            STATIC_ASSERT(std::is_nothrow_assignable<V, int>::value);
        }
        {
            using V = ranges::variant<Dummy, ThrowsCtorT>;
            STATIC_ASSERT(!std::is_nothrow_assignable<V, int>::value);
        }
        {
            using V = ranges::variant<Dummy, ThrowsAssignT>;
            STATIC_ASSERT(!std::is_nothrow_assignable<V, int>::value);
        }
    }

    void test_T_assignment_sfinae()
    {
        {
            using V = ranges::variant<long, unsigned>;
            static_assert(!std::is_assignable<V, int>::value, "ambiguous");
        }
        {
            using V = ranges::variant<std::string, std::string>;
            static_assert(!std::is_assignable<V, const char *>::value, "ambiguous");
        }
        {
            using V = ranges::variant<std::string, void *>;
            static_assert(!std::is_assignable<V, int>::value, "no matching operator=");
        }
        {
            using V = ranges::variant<int, int &&>;
            static_assert(!std::is_assignable<V, int>::value, "ambiguous");
        }
        {
            using V = ranges::variant<int, const int &>;
            static_assert(!std::is_assignable<V, int>::value, "ambiguous");
        }
    }

    void test_T_assignment_basic()
    {
        {
            ranges::variant<int> v(43);
            v = 42;
            CHECK(v.index() == 0u);
            CHECK(ranges::get<0>(v) == 42);
        }
        {
            ranges::variant<int, long> v(43l);
            v = 42;
            CHECK(v.index() == 0u);
            CHECK(ranges::get<0>(v) == 42);
            v = 43l;
            CHECK(v.index() == 1u);
            CHECK(ranges::get<1>(v) == 43);
        }
        {
            using V = ranges::variant<int &, int &&, long>;
            int x = 42;
            V v(43l);
            v = x;
            CHECK(v.index() == 0u);
            CHECK(&ranges::get<0>(v) == &x);
            v = std::move(x);
            CHECK(v.index() == 1u);
            CHECK(&ranges::get<1>(v) == &x);
            // 'long' is selected by FUN(const int &) since 'const int &' cannot bind
            // to 'int&'.
            const int &cx = x;
            v = cx;
            CHECK(v.index() == 2u);
            CHECK(ranges::get<2>(v) == 42);
        }
    }

    void test_T_assignment_performs_construction()
    {
        using namespace RuntimeHelpers;
        {
            using V = ranges::variant<std::string, ThrowsCtorT>;
            V v(ranges::in_place_type<std::string>, "hello");
            try {
                v = 42;
                CHECK(false);
            } catch (...) { /* ... */
            }
            CHECK(v.index() == 0u);
            CHECK(ranges::get<0>(v) == "hello");
        }
        {
            using V = ranges::variant<ThrowsAssignT, std::string>;
            V v(ranges::in_place_type<std::string>, "hello");
            v = 42;
            CHECK(v.index() == 0u);
            CHECK(ranges::get<0>(v).value == 42);
        }
    }

    void test_T_assignment_performs_assignment()
    {
        using namespace RuntimeHelpers;
        {
            using V = ranges::variant<ThrowsCtorT>;
            V v;
            v = 42;
            CHECK(v.index() == 0u);
            CHECK(ranges::get<0>(v).value == 42);
        }
        {
            using V = ranges::variant<ThrowsCtorT, std::string>;
            V v;
            v = 42;
            CHECK(v.index() == 0u);
            CHECK(ranges::get<0>(v).value == 42);
        }
        {
            using V = ranges::variant<ThrowsAssignT>;
            V v(100);
            try {
                v = 42;
                CHECK(false);
            } catch (...) { /* ... */
            }
            CHECK(v.index() == 0u);
            CHECK(ranges::get<0>(v).value == 100);
        }
        {
            using V = ranges::variant<std::string, ThrowsAssignT>;
            V v(100);
            try {
                v = 42;
                CHECK(false);
            } catch (...) { /* ... */
            }
            CHECK(v.index() == 1u);
            CHECK(ranges::get<1>(v).value == 100);
        }
    }

    void test()
    {
        test_T_assignment_basic();
        test_T_assignment_performs_construction();
        test_T_assignment_performs_assignment();
        test_T_assignment_noexcept();
        test_T_assignment_sfinae();
    }
} // namespace T_assign

namespace monostate
{
    void test()
    {
        using M = ranges::monostate;
        STATIC_ASSERT(ranges::detail::is_trivially_default_constructible<M>::value);
        STATIC_ASSERT(ranges::detail::is_trivially_copy_constructible<M>::value);
        STATIC_ASSERT(ranges::detail::is_trivially_copy_assignable<M>::value);
        STATIC_ASSERT(std::is_trivially_destructible<M>::value);
        constexpr M m{};
        (void)m;
    }
} // namespace monostate

namespace monostate_relops
{
    void test()
    {
        using M = ranges::monostate;
        constexpr M m1{};
        constexpr M m2{};
        {
            STATIC_ASSERT((m1 < m2) == false);
            ASSERT_NOEXCEPT(m1 < m2);
        }
        {
            STATIC_ASSERT((m1 > m2) == false);
            ASSERT_NOEXCEPT(m1 > m2);
        }
        {
            STATIC_ASSERT((m1 <= m2) == true);
            ASSERT_NOEXCEPT(m1 <= m2);
        }
        {
            STATIC_ASSERT((m1 >= m2) == true);
            ASSERT_NOEXCEPT(m1 >= m2);
        }
        {
            STATIC_ASSERT((m1 == m2) == true);
            ASSERT_NOEXCEPT(m1 == m2);
        }
        {
            STATIC_ASSERT((m1 != m2) == false);
            ASSERT_NOEXCEPT(m1 != m2);
        }
    }
} // namespace monostate_relops

int main()
{
    bad_access::test();
    alternative::test();
    size::test();
    holds_alternative::test();
    default_ctor::test();
    copy_ctor::test();
    move_ctor::test();
    in_place_index_ctor::test();
    in_place_index_il_ctor::test();
    in_place_type_ctor::test();
    in_place_type_il_ctor::test();
    converting_ctor::test();
    dtor::test();
    copy_assign::test();
    move_assign::test();
    T_assign::test();
    monostate::test();
    monostate_relops::test();

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
        STATIC_ASSERT(v.index() == 1);
        STATIC_ASSERT(v.valid());
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

        // Should compile and not CHECK at runtime.
        variant<T[5]> vrgt{emplaced_index<0>, {T{42},T{42},T{42},T{42},T{42}}};
        (void) vrgt;
    }
#endif

    return ::test_result();
}
