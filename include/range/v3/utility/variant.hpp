/// \file
// Range v3 library
//
//  Copyright Eric Niebler 2014
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3
//

#ifndef RANGES_V3_UTILITY_VARIANT_HPP
#define RANGES_V3_UTILITY_VARIANT_HPP

#include <exception>
#include <type_traits>
#include <utility>
#include <meta/meta.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/utility/concepts.hpp>

namespace ranges
{
    inline namespace v3
    {
        struct bad_variant_access : std::exception
        {
            char const *what() const noexcept override
            {
                return "bad variant access";
            }
        };

        namespace detail
        {
            template<typename... Types>
            using all_trivially_destructible = meta::strict_and<std::is_trivially_destructible<Types>...>;

            template<typename... Types>
            using all_copy_constructible = meta::strict_and<std::is_copy_constructible<Types>...>;

            template<typename... Types>
            using all_trivially_copy_constructible = meta::strict_and<std::is_trivially_copy_constructible<Types>...>;

            template<typename... Types>
            using all_move_constructible = meta::strict_and<std::is_move_constructible<Types>...>;

            template<typename... Types>
            using all_trivially_move_constructible = meta::strict_and<std::is_trivially_move_constructible<Types>...>;

            template<typename T, typename = void>
            struct variant_wrapper
            {
                CONCEPT_ASSERT(std::is_object<T>::value);

                meta::_t<std::remove_const<T>> obj_;

                template<typename... Args,
                    CONCEPT_REQUIRES_(Constructible<T, Args...>())>
                constexpr variant_wrapper(Args &&... args)
                    noexcept(std::is_nothrow_constructible<T, Args...>::value)
                  : obj_(static_cast<Args &&>(args)...)
                {}

                RANGES_CXX14_CONSTEXPR T &cook() & noexcept { return obj_; }
                constexpr T const &cook() const & noexcept { return obj_; }
                RANGES_CXX14_CONSTEXPR T &&cook() && noexcept
                {
                    return static_cast<T &&>(obj_);
                }
                constexpr T const &&cook() const && noexcept
                {
                    return static_cast<T const &&>(obj_);
                }
            };

            template<typename T>
            struct variant_wrapper<T, meta::if_<std::is_reference<T>>>
            {
                T ref_;

                template<typename Arg,
                    CONCEPT_REQUIRES_(Constructible<T, Arg>())>
                constexpr variant_wrapper(Arg &&arg)
                    noexcept(std::is_nothrow_constructible<T, Arg>::value)
                  : obj_(static_cast<Arg &&>(arg))
                {}

                constexpr T &cook() const & noexcept { return obj_; }
                constexpr T cook() const && noexcept
                {
                    return static_cast<T>(obj_);
                }
            };

            template<bool AllTriviallyDestructible, typename... Types>
            struct variant_storage_
            {
                CONCEPT_ASSERT(sizeof...(Types) == 0);
                static constexpr std::size_t size = 0;
            };

            template<typename... Types>
            using variant_storage = variant_storage_<
                all_trivially_destructible<Types...>::value, Types...>;

            template<typename First, typename... Rest>
            struct variant_storage_<true, First, Rest...>
            {
                CONCEPT_ASSERT(all_trivially_destructible<First, Rest...>::value);
                static constexpr std::size_t size = 1 + sizeof...(Rest);

                using head_t = variant_wrapper<First>;
                using tail_t = variant_storage<Rest...>;

                union
                {
                    head_t head_;
                    tail_t tail_;
                };

                variant_storage_() noexcept {}

                template<typename... Args,
                    CONCEPT_REQUIRES_(Constructible<head_t, Args...>())>
                constexpr variant_storage_(meta::size_t<0>, Args &&... args)
                    noexcept(std::is_nothrow_constructible<head_t, Args...>::value)
                  : head_(static_cast<Args &&>(args)...)
                {}

                template<std::size_t I, typename... Args,
                    CONCEPT_REQUIRES_(I < 1 + sizeof...(Rest) &&
                        Constructible<tail_t, meta::size_t<I - 1>, Args...>())>
                constexpr variant_storage_(meta::size_t<I>, Args &&... args)
                    noexcept(std::is_nothrow_constructible<
                        tail_t, meta::size_t<I - 1>, Args...>::value)
                  : tail_(meta::size_t<I - 1>{}, static_cast<Args &&>(args)...)
                {}
            };

            template<typename First, typename... Rest>
            struct variant_storage_<false, First, Rest...>
            {
                CONCEPT_ASSERT(!all_trivially_destructible<First, Rest...>::value);
                static constexpr std::size_t size = 1 + sizeof...(Rest);

                using head_t = variant_wrapper<First>;
                using tail_t = variant_storage<Rest...>;

                union
                {
                    head_t head_;
                    tail_t tail_;
                };

                variant_storage_() noexcept {}

                template<typename... Args,
                    CONCEPT_REQUIRES_(Constructible<head_t, Args...>())>
                constexpr variant_storage_(meta::size_t<0>, Args &&... args)
                    noexcept(std::is_nothrow_constructible<head_t, Args...>::value)
                  : head_(static_cast<Args &&>(args)...)
                {}

                template<std::size_t I, typename... Args,
                    CONCEPT_REQUIRES_(I < 1 + sizeof...(Rest) &&
                        Constructible<tail_t, meta::size_t<I - 1>, Args...>())>
                constexpr variant_storage_(meta::size_t<I>, Args &&... args)
                    noexcept(std::is_nothrow_constructible<
                        tail_t, meta::size_t<I - 1>, Args...>::value)
                  : tail_(meta::size_t<I - 1>{}, static_cast<Args &&>(args)...)
                {}

                ~variant_storage_() {}
                variant_storage_(variant_storage_ const &) = default;
                variant_storage_(variant_storage_ &&) = default;
                variant_storage_& operator=(variant_storage_ const &) = default;
                variant_storage_& operator=(variant_storage_ &&) = default;
            };

#if RANGES_CXX_IF_CONSTEXPR >= RANGES_CXX_IF_CONSTEXPR_17 && \
    RANGES_CXX_RETURN_TYPE_DEDUCTION >= RANGES_CXX_RETURN_TYPE_DEDUCTION_14
            template<std::size_t I>
            RANGES_CXX14_CONSTEXPR auto&& variant_raw_get(T &&storage) noexcept
            {
                if constexpr (I == 0)
                    return static_cast<T &&>(storage).head_;
                else if constexpr (I == 1)
                    return static_cast<T &&>(storage).tail_.head_;
                else if constexpr (I == 2)
                    return static_cast<T &&>(storage).tail_.tail_.head_;
                else if constexpr (I == 3)
                    return static_cast<T &&>(storage).tail_.tail_.tail_.head_;
                else if constexpr (I < 8)
                    return variant_raw_get<I - 4>(static_cast<T &&>(storage)
                        .tail_.tail_.tail_.tail_);
                else if constexpr (I < 16)
                    return variant_raw_get<I - 8>(static_cast<T &&>(storage)
                        .tail_.tail_.tail_.tail_.tail_.tail_.tail_.tail_);
                else if constexpr (I < 32)
                    return variant_raw_get<I - 16>(static_cast<T &&>(storage)
                        .tail_.tail_.tail_.tail_.tail_.tail_.tail_.tail_
                        .tail_.tail_.tail_.tail_.tail_.tail_.tail_.tail_);
                else if constexpr (I < 64)
                    return variant_raw_get<I - 32>(static_cast<T &&>(storage)
                        .tail_.tail_.tail_.tail_.tail_.tail_.tail_.tail_
                        .tail_.tail_.tail_.tail_.tail_.tail_.tail_.tail_
                        .tail_.tail_.tail_.tail_.tail_.tail_.tail_.tail_
                        .tail_.tail_.tail_.tail_.tail_.tail_.tail_.tail_);
            }
#else
            template<std::size_t I, CONCEPT_REQUIRES(I == 0)>
            constexpr auto variant_raw_get(T &&storage) noexcept
            RANGES_DECLTYPE_AUTO_RETURN
            (
                static_cast<T &&>(storage).head_
            )

            template<std::size_t I, CONCEPT_REQUIRES_(I > 0)>
            constexpr auto variant_raw_get(T &&storage) noexcept
            RANGES_DECLTYPE_AUTO_RETURN
            (
                variant_raw_get<I - 1>(static_cast<T &&>(storage).tail_)
            )
#endif

            template<typename Fn, typename Storage, typename Indices>
            struct variant_raw_dispatch;
            template<typename Fn, typename Storage, std::size_t... Is>
            struct variant_raw_dispatch<Fn, Storage, meta::index_sequence<Is...>>
            {
                CONCEPT_ASSERT(sizeof...(Is) == Storage::size);
                using result_t = decltype(std::declval<Fn>()(meta::size_t<0>{},
                    variant_raw_get<0>(std::declval<Storage>())));
                using fn_t = result_t(*)(Fn &&, Storage &&);
                static constexpr result_t bad_access_target(Fn &&fn, Storage &&)
                {
                    static_cast<Fn &&>(fn).bad_access();
                }
                template<std::size_t I>
                static constexpr result_t dispatch_target(Fn &&fn, Storage &&storage)
                {
                    return static_cast<Fn &&>(fn)(meta::size_t<I - 1>{},
                        variant_raw_get<I - 1>(static_cast<Storage &&>(storage)));
                }
                static constexpr fn_t dispatch_table[sizeof...(Is) + 1] = {
                    bad_access_target,
                    dispatch_target<Is>...
                };
            };

            template<typename Fn, typename Storage, std::size_t... Is>
            constexpr variant_raw_dispatch<Fn, Storage, meta::index_sequence<Is...>::fn_t
                variant_raw_dispatch<Fn, Storage, meta::index_sequence<Is...>>::dispatch_table[sizeof...(Is) + 1];

            template<typename Fn, typename Storage>
            constexpr auto variant_raw_visit(std::size_t index, Storage &&storage, Fn &&fn)
            {
                using Indices = meta::make_index_sequence<Storage::size>;
                using Dispatch = variant_raw_dispatch<Fn, Storage, Indices>;
                return Dispatch::dispatch_table[index](
                    static_cast<Fn &&>(fn), static_cast<Storage &&>(storage));
            }

            struct variant_destroy_visitor
            {
                RANGES_CXX14_CONSTEXPR static void bad_access() noexcept
                {}
                template<std::size_t I, typename T>
                RANGES_CXX14_CONSTEXPR void operator()(meta::size_t<I>, T &t) const noexcept
                {
                    t.~T();
                }
            };

            template<std::size_t N>
            using variant_index_t =
                meta::if_c<(N < std::numeric_limits<unsigned char>::max()), unsigned char,
                meta::if_c<(N < std::numeric_limits<unsigned short>::max()), unsigned short,
                unsigned int>;

            template<typename... Types>
            struct variant_base
            {
            private:
#if RANGES_CXX_IF_CONSTEXPR < RANGES_CXX_IF_CONSTEXPR_17
                void destroy_(std::true_type) noexcept
                {
                    CONCEPT_ASSERT(all_trivially_destructible<Types...>::value);
                }
                void destroy_(std::false_type) noexcept
                {
                    CONCEPT_ASSERT(!all_trivially_destructible<Types...>::value);
                    variant_raw_visit(index_, storage_, variant_destroy_visitor{});
                }
#endif
            public:
                variant_storage<Types...> storage_;
                variant_index_t<Types...> index_ = 0; // Invariant: index_ <= sizeof...(Types)

                void destroy() noexcept
                {
#if RANGES_CXX_IF_CONSTEXPR >= RANGES_CXX_IF_CONSTEXPR_17
                    if constexpr (!all_trivially_destructible<Types...>::value)
                        variant_raw_visit(index_, storage_, variant_destroy_visitor{});
#else
                    destroy_(all_trivially_destructible<Types...>{});
#endif
                }

                variant_base() noexcept = default;
                template<std::size_t I, typename... Args,
                    typename T = meta::at_c<meta::list<Types...>, I>,
                    CONCEPT_REQUIRES_(Constructible<T, Args...>())>
                constexpr variant_base(meta::size_t<I>, Args &&... args)
                    noexcept(std::is_nothrow_default_constructible<T>::value)
                  : storage_(meta::size_t<0>{}), index_{1}
                {}
            };

            template<typename... Types>
            struct variant_nontrivial_destruction
              : variant_base<Types...>
            {
                using variant_base<Types...>::variant_base;
                ~variant_nontrivial_destruction() { this->destroy(); }
                variant_nontrivial_destruction(variant_nontrivial_destruction const&) = default;
                variant_nontrivial_destruction(variant_nontrivial_destruction &&) = default;
                variant_nontrivial_destruction& operator=(variant_nontrivial_destruction const&) = default;
                variant_nontrivial_destruction& operator=(variant_nontrivial_destruction &&) = default;
            };

            template<typename... Types>
            using variant_destruction_layer = meta::if_<
                all_trivially_destructible<Types...>,
                variant_base<Types...>,
                variant_nontrivial_destruction<Types...>>;

            template<typename... Types>
            struct variant_construct_visitor
            {
                variant_base &self_;

                RANGES_CXX14_CONSTEXPR static void bad_access() noexcept
                {}
                template<std::size_t I, typename T>
                RANGES_CXX14_CONSTEXPR void operator()(meta::size_t<I>, T &&t) const noexcept
                {
                    auto &target = variant_raw_get<I>(self_.storage_);
                    using target_t = meta::_t<remove_reference<decltype(target)>>;
                    ::new (std::addressof(target)) target_t(static_cast<T &&>(t));
                }
            };

            template<typename... Types>
            struct variant_nontrivial_copy
              : variant_destruction_layer<Types...>
            {
                using variant_destruction_layer<Types...>::variant_destruction_layer;
                variant_nontrivial_copy(variant_nontrivial_copy const& that)
                {
                    variant_raw_visit(this->index_ = that.index_, that.storage_,
                        variant_construct_visitor<Types...>{*this});
                }
                variant_nontrivial_copy(variant_nontrivial_copy &&) = default;
                variant_nontrivial_copy& operator=(variant_nontrivial_copy const&) = default;
                variant_nontrivial_copy& operator=(variant_nontrivial_copy &&) = default;
            };

            template<typename... Types>
            using variant_copy_layer = meta::if_c<
                all_copy_constructible<Types...>::value && !all_trivially_copy_constructible<Types...>,
                variant_nontrivial_copy<Types...>,
                variant_destruction_layer<Types...>>;

            template<typename... Types>
            struct variant_nontrivial_move
              : variant_copy_layer<Types...>
            {
                using variant_copy_layer<Types...>::variant_copy_layer;
                variant_nontrivial_move(variant_nontrivial_move const&) = default;
                variant_nontrivial_move(variant_nontrivial_move &&that)
                {
                    variant_raw_visit(this->index_ = that.index_, std::move(that.storage_),
                        variant_construct_visitor<Types...>{*this});
                }
                variant_nontrivial_move& operator=(variant_nontrivial_move const&) = default;
                variant_nontrivial_move& operator=(variant_nontrivial_move &&) = default;
            };

            template<typename... Types>
            using variant_move_layer = meta::if_c<
                all_move_constructible<Types...>::value && !all_trivially_move_constructible<Types...>,
                variant_nontrivial_move<Types...>,
                variant_copy_layer<Types...>>;
        } // namespace detail

        template<typename... Types>
        struct variant
          : private detail::variant_move_layer<Types...>
        {
        private:
            using base_t = detail::variant_move_layer<Types...>;
        public:
            template<class First = meta::at_c<meta::list<Types...>, 0>,
                CONCEPT_REQUIRES_(DefaultConstructible<First>())>
            constexpr variant()
                noexcept(std::is_nothrow_default_constructible<First>::value)
                : base_t{meta::size_t<0>{}}
            {}

            constexpr std::size_t index() const noexcept
            {
                return static_cast<std::size_t>(index_) - 1;
            }
            constexpr bool valueless_by_exception() const noexcept
            {
                return index_ == 0;
            }
        };
    } // namespace v3
} // namespace ranges

#endif
