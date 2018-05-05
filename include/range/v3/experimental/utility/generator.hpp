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
#ifndef RANGES_V3_EXPERIMENTAL_UTILITY_GENERATOR_HPP
#define RANGES_V3_EXPERIMENTAL_UTILITY_GENERATOR_HPP

#include <range/v3/detail/config.hpp>
#if RANGES_CXX_COROUTINES >= RANGES_CXX_COROUTINES_TS1
#include <cstddef>
#include <exception>
#include <utility>
#include <experimental/coroutine>
#include <meta/meta.hpp>
#include <range/v3/range_traits.hpp>
#include <range/v3/view_facade.hpp>
#include <range/v3/utility/box.hpp>
#include <range/v3/utility/concepts.hpp>
#include <range/v3/utility/semiregular.hpp>
#include <range/v3/utility/swap.hpp>
#include <range/v3/view/all.hpp>

namespace ranges
{
    inline namespace v3
    {
        /// \addtogroup group-utility
        /// @{
        namespace experimental
        {
            // The type of size() for a sized_generator
            using generator_size_t = std::ptrdiff_t;

            // Type upon which to co_await to set the size of a sized_generator
            enum struct generator_size : generator_size_t {};

            template<typename Promise = void>
            struct coroutine_owner;
        } // namespace experimental

        /// \cond
        namespace detail
        {
            inline void resume(std::experimental::coroutine_handle<> coro)
            {
                // Pre: coro refers to a suspended coroutine.
                RANGES_EXPECT(coro);
                RANGES_EXPECT(!coro.done());
                coro.resume();
            }

            using dealloc_ptr_t = void(*)(void*, std::size_t) noexcept;

            template<typename T>
            using SuperEmpty = meta::bool_<
                std::is_empty<T>::value && !is_final<T>::value &&
                std::is_standard_layout<T>::value &&
                alignof(T) <= alignof(dealloc_ptr_t)>;

            template<class A>
            struct dealloc_with_alloc
              : A
            {
                dealloc_ptr_t dealloc_;

                dealloc_with_alloc(A&& a) noexcept
                  : A{std::move(a)}
                {}
            };

            template<class I, CONCEPT_REQUIRES_(UnsignedIntegral<I>())>
            constexpr I round_up(I value, I align) noexcept
            {
                // align must be a power of 2
                RANGES_EXPECT(align != 0 && (align & (align - 1)) == 0);
                return (value + (align - 1)) & ~(align - 1);
            }
        } // namespace detail
        /// \endcond

        namespace experimental
        {
            // An owning coroutine_handle
            template<typename Promise>
            struct coroutine_owner
              : std::experimental::coroutine_handle<Promise>
            {
                using base_t = std::experimental::coroutine_handle<Promise>;

                using base_t::operator bool;
                using base_t::done;
                using base_t::promise;

                constexpr coroutine_owner() noexcept = default;
                explicit constexpr coroutine_owner(base_t coro) noexcept
                  : base_t(coro)
                {}
                RANGES_CXX14_CONSTEXPR coroutine_owner(coroutine_owner &&that) noexcept
                  : base_t(ranges::exchange(that.base(), {}))
                {}

                ~coroutine_owner() { if (*this) base().destroy(); }

                coroutine_owner &operator=(coroutine_owner &&that) noexcept
                {
                    coroutine_owner{ranges::exchange(base(), ranges::exchange(that.base(), {}))};
                    return *this;
                }

                friend void swap(coroutine_owner &x, coroutine_owner &y) noexcept
                {
                    ranges::swap(x.base(), y.base());
                }

                void resume() { detail::resume(base()); }
                void operator()() { detail::resume(base()); }

                base_t &base() noexcept { return *this; }
                base_t const &base() const noexcept { return *this; }

                base_t release() noexcept
                {
                    return ranges::exchange(base(), {});
                }
            };
        } // namespace experimental

        /// \cond
        namespace detail
        {
            template<typename Reference>
            struct generator_promise
            {
                std::exception_ptr except_ = nullptr;

                CONCEPT_ASSERT(meta::or_<std::is_reference<Reference>,
                    CopyConstructible<Reference>>());

                generator_promise *get_return_object() noexcept
                {
                    return this;
                }
                std::experimental::suspend_always initial_suspend() const noexcept
                {
                    return {};
                }
                std::experimental::suspend_always final_suspend() const noexcept
                {
                    return {};
                }
                void return_void() const noexcept
                {}
                void unhandled_exception() noexcept
                {
                    except_ = std::current_exception();
                }
                template<typename Arg,
                    CONCEPT_REQUIRES_(ConvertibleTo<Arg, Reference>() &&
                        std::is_assignable<semiregular_t<Reference> &, Arg>::value)>
                std::experimental::suspend_always yield_value(Arg &&arg)
                    noexcept(std::is_nothrow_assignable<semiregular_t<Reference> &, Arg>::value)
                {
                    ref_ = std::forward<Arg>(arg);
                    return {};
                }
                std::experimental::suspend_never
                await_transform(experimental::generator_size) const noexcept
                {
                    RANGES_ENSURE_MSG(false, "Invalid size request for a non-sized generator");
                    return {};
                }
                meta::if_<std::is_reference<Reference>, Reference, Reference const &>
                read() const noexcept
                {
                    return ref_;
                }

                static void* operator new(std::size_t const size)
                {
                    std::allocator<char> a{};
                    return operator new(size, std::allocator_arg, a);
                }

                template <typename RA, typename... Args>
                static void *operator new(std::size_t const size, std::allocator_arg_t,
                    RA &raw_allocator, Args &...)
                {
                    using Alloc = typename std::allocator_traits<RA>::template rebind_alloc<char>;
                    auto alloc = Alloc{raw_allocator};

                    char *ptr = alloc.allocate(get_padded_size<Alloc>(size));
                    if RANGES_CONSTEXPR_IF (SuperEmpty<Alloc>())
                    {
                        using DWA = detail::dealloc_with_alloc<Alloc>;
                        static_assert(std::is_standard_layout<DWA>::value, "FIXME");
                        static_assert(alignof(DWA) == alignof(detail::dealloc_ptr_t), "FIXME");
                        static_assert(offsetof(DWA, dealloc_) == 0, "FIXME");

                        void *const dwa_addr = ptr + get_dealloc_offset(size);
                        auto &dwa = *::new (dwa_addr) DWA{std::move(alloc)};
                        dwa.dealloc_ = [](void *const vp, std::size_t const size) noexcept
                        {
                            char *const ptr = static_cast<char *>(vp);
                            void *const dwa_addr = ptr + get_dealloc_offset(size);
                            auto &dwa = *static_cast<DWA *>(dwa_addr);
                            Alloc a{static_cast<Alloc &&>(dwa)};
                            dwa.~DWA();
                            a.deallocate(ptr, get_padded_size<Alloc>(size));
                        };
                    }
                    else
                    {
                        ::new (ptr + get_alloc_offset<Alloc>(size)) Alloc{std::move(alloc)};
                        void *const dptr = ptr + get_dealloc_offset(size);
                        auto &dealloc = *reinterpret_cast<detail::dealloc_ptr_t *>(dptr);
                        dealloc = [](void *const vp, std::size_t const size) noexcept
                        {
                            char *const ptr = static_cast<char *>(vp);
                            void *const alloc_addr = ptr + get_alloc_offset<Alloc>(size);
                            Alloc &stored_alloc = *static_cast<Alloc *>(alloc_addr);
                            Alloc a{std::move(stored_alloc)};
                            stored_alloc.~Alloc();
                            a.deallocate(ptr, get_padded_size<Alloc>(size));
                        };
                    }
                    return ptr;
                }

                static void operator delete(void *ptr, std::size_t size) noexcept
                {
                    get_dealloc_func(ptr, size)(ptr, size);
                }
            private:
                static constexpr std::size_t get_dealloc_offset(std::size_t size) noexcept
                {
                    return detail::round_up(size, alignof(detail::dealloc_ptr_t));
                }

                template<typename Alloc>
                static constexpr std::size_t get_alloc_offset(std::size_t size) noexcept
                {
                    static_assert(!detail::SuperEmpty<Alloc>(), "FIXME");
                    size = get_dealloc_offset(size);
                    size += sizeof(detail::dealloc_ptr_t);
                    return detail::round_up(size, alignof(Alloc));
                }

                static detail::dealloc_ptr_t get_dealloc_func(void *ptr, std::size_t size) noexcept
                {
                    // Magic: since detail::dealloc_with_alloc<A> is standard layout
                    // when SuperEmpty<A> is satisfied, it is pointer-interconvertible
                    // with its first non-static data member which has type
                    // detail::dealloc_ptr_t. Consequently, Whether there's a
                    // dealloc_with_alloc<A> or a dealloc_ptr_t stored at
                    // get_dealloc_offset(size), we can read a dealloc_ptr_t.
                    char *addr = static_cast<char *>(ptr) + get_dealloc_offset(size);
                    return *reinterpret_cast<detail::dealloc_ptr_t *>(addr);
                }

                template<typename Alloc>
                static constexpr std::size_t get_padded_size(std::size_t size) noexcept
                {
                    if RANGES_CONSTEXPR_IF (detail::SuperEmpty<Alloc>())
                    {
                        size = get_dealloc_offset(size);
                        size += sizeof(detail::dealloc_with_alloc<Alloc>);
                    }
                    else
                    {
                        size += get_alloc_offset<Alloc>(size);
                        size += sizeof(Alloc);
                    }
                    return size;
                }

                semiregular_t<Reference> ref_;
            };

            template<typename Reference>
            struct sized_generator_promise
              : generator_promise<Reference>
            {
                sized_generator_promise *get_return_object() noexcept
                {
                    return this;
                }
                std::experimental::suspend_never initial_suspend() const noexcept
                {
                    // sized_generator doesn't suspend at its initial suspend point because...
                    return {};
                }
                std::experimental::suspend_always
                await_transform(experimental::generator_size size) noexcept
                {
                    // ...the coroutine sets the size of the range first by
                    // co_awaiting on a generator_size.
                    auto const unwrapped = static_cast<experimental::generator_size_t>(size);
                    RANGES_EXPECT(unwrapped >= 0);
                    size_ = unwrapped;
                    return {};
                }
                template<typename Arg,
                    CONCEPT_REQUIRES_(ConvertibleTo<Arg, Reference>() &&
                        std::is_assignable<semiregular_t<Reference> &, Arg>::value)>
                std::experimental::suspend_always yield_value(Arg &&arg)
                    noexcept(std::is_nothrow_assignable<semiregular_t<Reference> &, Arg>::value)
                {
                    RANGES_EXPECT(size_ >= 0); // size_ must be set before generating values
                    return generator_promise<Reference>::yield_value(static_cast<Arg &&>(arg));
                }
                experimental::generator_size_t size() const noexcept
                {
                    RANGES_EXPECT(size_ >= 0);
                    return size_;
                }
            private:
                experimental::generator_size_t size_ = -1;
            };
        } // namespace detail
        /// \endcond

        namespace experimental
        {
            template<typename Reference, typename Value = uncvref_t<Reference>>
            struct generator
              : view_facade<generator<Reference, Value>>
            {
            public:
                using promise_type = detail::generator_promise<Reference>;

                constexpr generator() noexcept = default;
                generator(promise_type *p) // should not be public?
                  : coro_{handle::from_promise(*p)}
                {
                    RANGES_EXPECT(coro_);
                }

            protected:
                coroutine_owner<promise_type> coro_;

            private:
                friend range_access;
                using handle = std::experimental::coroutine_handle<promise_type>;

                struct cursor
                {
                    using value_type = Value;

                    cursor() = default;
                    constexpr explicit cursor(handle coro) noexcept
                      : coro_{coro}
                    {}
                    bool equal(default_sentinel) const
                    {
                        RANGES_EXPECT(coro_);
                        if (coro_.done())
                        {
                            auto &e = coro_.promise().except_;
                            if (e) std::rethrow_exception(std::move(e));
                            return true;
                        }
                        return false;
                    }
                    void next()
                    {
                        detail::resume(coro_);
                    }
                    Reference read() const
                    {
                        RANGES_EXPECT(coro_);
                        return coro_.promise().read();
                    }
                private:
                    handle coro_ = nullptr;
                };

                cursor begin_cursor()
                {
                    detail::resume(coro_);
                    return cursor{coro_};
                }
            };

            template<typename Reference, typename Value = uncvref_t<Reference>>
            struct sized_generator
              : generator<Reference, Value>
            {
            public:
                using promise_type = detail::sized_generator_promise<Reference>;

                constexpr sized_generator() noexcept = default;
                sized_generator(promise_type *p) // should not be public?
                  : generator<Reference, Value>{p}
                {}
                generator_size_t size() const noexcept
                {
                    return promise().size();
                }

            private:
                using generator<Reference, Value>::coro_;

                promise_type const &promise() const noexcept
                {
                    RANGES_EXPECT(coro_);
                    return static_cast<promise_type const &>(coro_.promise());
                }
            };
        } // namespace experimental

        /// @}
    } // inline namespace v3
} // namespace ranges
#endif // RANGES_CXX_COROUTINES >= RANGES_CXX_COROUTINES_TS1

#endif // RANGES_V3_EXPERIMENTAL_UTILITY_GENERATOR_HPP
