// Range v3 library
//
//  Copyright Eric Niebler 2014-present
//  Copyright Casey Carter 2015
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3

#include <memory>
#include <range/v3/utility/functional.hpp>
#include <range/v3/view/filter.hpp>
#include "../simple_test.hpp"
#include "../test_utils.hpp"

CONCEPT_ASSERT(ranges::Constructible<ranges::reference_wrapper<int>, int&>());
CONCEPT_ASSERT(!ranges::Constructible<ranges::reference_wrapper<int>, int&&>());
CONCEPT_ASSERT(!ranges::Constructible<ranges::reference_wrapper<int &&>, int&>());
CONCEPT_ASSERT(ranges::Constructible<ranges::reference_wrapper<int &&>, int&&>());

namespace
{
    struct Integer
    {
        int i;
        operator int() const { return i; }
        bool odd() const { return (i % 2) != 0; }
    };

    enum class kind { lvalue, const_lvalue, rvalue, const_rvalue };

    std::ostream &operator<<(std::ostream &os, kind k)
    {
        const char* message = nullptr;
        switch (k) {
            case kind::lvalue:
                message = "lvalue";
                break;
            case kind::const_lvalue:
                message = "const_lvalue";
                break;
            case kind::rvalue:
                message = "rvalue";
                break;
            case kind::const_rvalue:
                message = "const_rvalue";
                break;
        }
        return os << message;
    }

    kind last_call;

    template<kind DisableKind>
    struct fn
    {
        bool operator()() &
        {
            last_call = kind::lvalue;
            return DisableKind != kind::lvalue;
        }
        bool operator()() const &
        {
            last_call = kind::const_lvalue;
            return DisableKind != kind::const_lvalue;
        }
        bool operator()() &&
        {
            last_call = kind::rvalue;
            return DisableKind != kind::rvalue;
        }
        bool operator()() const &&
        {
            last_call = kind::const_rvalue;
            return DisableKind != kind::const_rvalue;
        }
    };

    constexpr struct {
        template<typename T>
        constexpr T&& operator()(T&& arg) const noexcept {
            return (T&&)arg;
        }
    } h = {};

    struct A {
        int i = 13;
        constexpr int f() const noexcept { return 42; }
        RANGES_CXX14_CONSTEXPR int g(int i) { return 2 * i; }
    };

    constexpr int f() noexcept { return 13; }
    constexpr int g(int i) { return 2 * i + 1; }

#define ASSERT_NOEXCEPT(...) static_assert(noexcept(__VA_ARGS__), "")
#if RANGES_CXX_NOEXCEPT_FUNCTION_TYPE >= RANGES_CXX_NOEXCEPT_FUNCTION_TYPE_17
#define ASSERT_NOEXCEPT_17(...) ASSERT_NOEXCEPT(__VA_ARGS__)
#else
#define ASSERT_NOEXCEPT_17(...)
#endif

    void test_invoke()
    {
        CHECK(ranges::invoke(f) == 13);
        ASSERT_NOEXCEPT_17(ranges::invoke(f) == 13);
        CHECK(ranges::invoke(g, 2) == 5);
        CHECK(ranges::invoke(h, 42) == 42);
        ASSERT_NOEXCEPT(ranges::invoke(h, 42) == 42);
        {
            int i = 13;
            CHECK(&ranges::invoke(h, i) == &i);
            ASSERT_NOEXCEPT(&ranges::invoke(h, i) == &i);
        }

        CHECK(ranges::invoke(&A::f, A{}) == 42);
        ASSERT_NOEXCEPT_17(ranges::invoke(&A::f, A{}) == 42);
        CHECK(ranges::invoke(&A::g, A{}, 2) == 4);
        {
            A a;
            const auto& ca = a;
            CHECK(ranges::invoke(&A::f, a) == 42);
            ASSERT_NOEXCEPT_17(ranges::invoke(&A::f, a) == 42);
            CHECK(ranges::invoke(&A::f, ca) == 42);
            ASSERT_NOEXCEPT_17(ranges::invoke(&A::f, ca) == 42);
            CHECK(ranges::invoke(&A::g, a, 2) == 4);
        }

        {
            A a;
            const auto& ca = a;
            CHECK(ranges::invoke(&A::f, &a) == 42);
            ASSERT_NOEXCEPT_17(ranges::invoke(&A::f, &a) == 42);
            CHECK(ranges::invoke(&A::f, &ca) == 42);
            ASSERT_NOEXCEPT_17(ranges::invoke(&A::f, &ca) == 42);
            CHECK(ranges::invoke(&A::g, &a, 2) == 4);
        }
        {
            std::unique_ptr<A> up(new A);
            CHECK(ranges::invoke(&A::f, up) == 42);
            CHECK(ranges::invoke(&A::g, up, 2) == 4);
        }
        {
            auto sp = std::make_shared<A>();
            CHECK(ranges::invoke(&A::f, sp) == 42);
            ASSERT_NOEXCEPT_17(ranges::invoke(&A::f, sp) == 42);
            CHECK(ranges::invoke(&A::g, sp, 2) == 4);
        }

        CHECK(ranges::invoke(&A::i, A{}) == 13);
        ASSERT_NOEXCEPT_17(ranges::invoke(&A::i, A{}) == 13);
        { int&& tmp = ranges::invoke(&A::i, A{}); (void)tmp; }

        {
            A a;
            const auto& ca = a;
            CHECK(ranges::invoke(&A::i, a) == 13);
            ASSERT_NOEXCEPT(ranges::invoke(&A::i, a) == 13);
            CHECK(ranges::invoke(&A::i, ca) == 13);
            ASSERT_NOEXCEPT(ranges::invoke(&A::i, ca) == 13);
            CHECK(ranges::invoke(&A::i, &a) == 13);
            ASSERT_NOEXCEPT(ranges::invoke(&A::i, &a) == 13);
            CHECK(ranges::invoke(&A::i, &ca) == 13);
            ASSERT_NOEXCEPT(ranges::invoke(&A::i, &ca) == 13);

            ranges::invoke(&A::i, a) = 0;
            CHECK(a.i == 0);
            ranges::invoke(&A::i, &a) = 1;
            CHECK(a.i == 1);
            CONCEPT_ASSERT(ranges::Same<decltype(ranges::invoke(&A::i, ca)), const int&>());
            CONCEPT_ASSERT(ranges::Same<decltype(ranges::invoke(&A::i, &ca)), const int&>());
        }

        {
            std::unique_ptr<A> up(new A);
            CHECK(ranges::invoke(&A::i, up) == 13);
            ranges::invoke(&A::i, up) = 0;
            CHECK(up->i == 0);
        }

        {
            auto sp = std::make_shared<A>();
            CHECK(ranges::invoke(&A::i, sp) == 13);
            ranges::invoke(&A::i, sp) = 0;
            CHECK(sp->i == 0);
        }

        {
            struct B { int i = 42; constexpr int f() const { return i; } };
            static constexpr B b{};

            static_assert(b.*&B::i == 42, "");
            static_assert((b.*&B::f)() == 42, "");
            static_assert(ranges::invoke(&B::i, b) == 42, "");
            static_assert(ranges::invoke(&B::f, b) == 42, "");

            static_assert((&b)->*&B::i == 42, "");
            static_assert(((&b)->*&B::f)() == 42, "");
            static_assert(ranges::invoke(&B::i, &b) == 42, "");
            static_assert(ranges::invoke(&B::f, &b) == 42, "");

            static_assert((B{}.*&B::f)() == 42, "");
#if !defined(__GNUC__) || defined(__clang__) || __GNUC__ > 4
            // GCC 4.9 doesn't grok these as constant expressions:
            static_assert(B{}.*&B::i == 42, "");
            static_assert(ranges::invoke(&B::i, B{}) == 42, "");
            static_assert(ranges::invoke(&B::f, B{}) == 42, "");
#endif
        }
    }
} // unnamed namespace



int main()
{
    {
        // Check that not_fn works with callables
        Integer some_ints[] = {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}};
        ::check_equal(some_ints | ranges::view::filter(ranges::not_fn(&Integer::odd)),
                      {0,2,4,6});
    }

    // Check that not_fn forwards value category
    {
        constexpr auto k = kind::lvalue;
        using F = fn<k>;
        auto f = ranges::not_fn(F{});
        CHECK(f() == true);
        CHECK(last_call == k);
    }
    {
        constexpr auto k = kind::const_lvalue;
        using F = fn<k>;
        auto const f = ranges::not_fn(F{});
        CHECK(f() == true);
        CHECK(last_call == k);
    }
    {
        constexpr auto k = kind::rvalue;
        using F = fn<k>;
        auto f = ranges::not_fn(F{});
        CHECK(std::move(f)() == true); // xvalue
        CHECK(last_call == k);

        CHECK(decltype(f){}() == true); // prvalue
        CHECK(last_call == k);
    }

    {
        constexpr auto k = kind::const_rvalue;
        using F = fn<k>;
        auto const f = ranges::not_fn(F{});
        CHECK(std::move(f)() == true); // xvalue
        CHECK(last_call == k);

        CHECK(decltype(f){}() == true); // prvalue
        CHECK(last_call == k);
    }

#ifdef _WIN32
    {
        // Ensure that Invocable accepts pointers to functions with non-default calling conventions.
        CONCEPT_ASSERT(ranges::Invocable<void(__cdecl*)()>());
        CONCEPT_ASSERT(ranges::Invocable<void(__stdcall*)()>());
        CONCEPT_ASSERT(ranges::Invocable<void(__fastcall*)()>());
        CONCEPT_ASSERT(ranges::Invocable<void(__thiscall*)()>());
#ifndef __MINGW32__
        CONCEPT_ASSERT(ranges::Invocable<void(__vectorcall*)()>());
#endif
    }
#endif // _WIN32

    test_invoke();

    return ::test_result();
}
