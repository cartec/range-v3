/// \file
// Range v3 library
//
//  Copyright Eric Niebler 2014
//  Copyright Casey Carter 2018
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
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <meta/meta.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/utility/concepts.hpp>
#include <range/v3/utility/static_const.hpp>

namespace ranges
{
    inline namespace v3
    {
        struct bad_variant_access : std::exception
        {
            virtual char const *what() const noexcept override
            {
                return "bad variant access";
            }
        };

        template<std::size_t I>
        struct in_place_index_t
          : meta::size_t<I>
        {};

        /// \cond
    #if !RANGES_CXX_VARIABLE_TEMPLATES
        template<std::size_t I>
        in_place_index_t<I> in_place_index()
        {
            return {};
        }
        template<std::size_t I>
        using _in_place_index_t_ = in_place_index_t<I>(&)();
    #define RANGES_IN_PLACE_INDEX_T(I) _in_place_index_t_<I>
    #else
        /// \endcond

    #if RANGES_CXX_INLINE_VARIABLES >= RANGES_CXX_INLINE_VARIABLES_17
        template<std::size_t I>
        inline constexpr auto in_place_index = in_place_index_t<I>{};
    #else
        inline namespace
        {
            template<std::size_t I>
            constexpr auto &in_place_index = static_const<emplaced_index_t<I>>::value;
        }
    #endif

        /// \cond
    #define RANGES_IN_PLACE_INDEX_T(I) in_place_index_t<I>
    #endif
        /// \endcond

        template<typename T>
        struct in_place_type_t
          : meta::id<T>
        {};

        /// \cond
    #if !RANGES_CXX_VARIABLE_TEMPLATES
        template<typename T>
        in_place_type_t<T> in_place_type()
        {
            return {};
        }
        template<typename T>
        using _in_place_type_t_ = in_place_type_t<T>(&)();
    #define RANGES_IN_PLACE_TYPE_T(I) _in_place_type_t_<I>
    #else
        /// \endcond

    #if RANGES_CXX_INLINE_VARIABLES >= RANGES_CXX_INLINE_VARIABLES_17
        template<typename T>
        inline constexpr auto in_place_type = in_place_type_t<T>{};
    #else
        inline namespace
        {
            template<typename T>
            constexpr auto &in_place_type = static_const<in_place_type_t<I>>::value;
        }
    #endif

        /// \cond
    #define RANGES_IN_PLACE_TYPE_T(I) in_place_type_t<I>
    #endif
        /// \endcond

        namespace detail
        {
            constexpr std::size_t find_unique_index_i_(
                bool const *first, std::size_t n, std::size_t i = 0,
                std::size_t found = std::size_t(-1)) noexcept
            {
                return i == n ? found :
                    first[i] ? (found == std::size_t(-1) ?
                        find_unique_index_i_(first, n, i + 1, i) :
                        std::size_t(-1)) :
                    find_unique_index_i_(first, n, i + 1, found);
            }

            template<typename, typename>
            struct find_unique_index_ {};
            template<typename List, typename T>
            using find_unique_index = meta::_t<find_unique_index_<List, T>>;
            template<typename... Ts, typename T>
            struct find_unique_index_<meta::list<Ts...>, T>
            {
                static constexpr bool bools[sizeof...(Ts)] = {std::is_same<Ts, T>::value...};
                using type = meta::size_t<find_unique_index_i_(bools, sizeof...(Ts))>;
            };

            template<typename... Types>
            using all_trivially_destructible = meta::strict_and<std::is_trivially_destructible<Types>...>;

            template<typename... Types>
            using all_copy_constructible = meta::strict_and<std::is_copy_constructible<Types>...>;

            template<typename... Types>
            using all_trivially_copy_constructible = meta::strict_and<detail::is_trivially_copy_constructible<Types>...>;

            template<typename... Types>
            using all_move_constructible = meta::strict_and<std::is_move_constructible<Types>...>;

            template<typename... Types>
            using all_trivially_move_constructible = meta::strict_and<detail::is_trivially_move_constructible<Types>...>;

            template<typename... Types>
            using all_copy_assignable = meta::strict_and<std::is_copy_assignable<Types>...>;

            template<typename... Types>
            using all_trivially_copy_assignable = meta::strict_and<detail::is_trivially_copy_assignable<Types>...>;

            template<typename... Types>
            using all_move_assignable = meta::strict_and<std::is_move_assignable<Types>...>;

            template<typename... Types>
            using all_trivially_move_assignable = meta::strict_and<detail::is_trivially_move_assignable<Types>...>;

            template<typename... Types>
            using any_reference_type = meta::strict_or<std::is_reference<Types>...>;

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

                RANGES_CXX14_CONSTEXPR T &cook() & noexcept
                {
                    return obj_;
                }
                constexpr T const &cook() const & noexcept
                {
                    return obj_;
                }
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
                  : ref_(static_cast<Arg &&>(arg))
                {}

                constexpr T &cook() const & noexcept
                {
                    return ref_;
                }
                constexpr T cook() const && noexcept
                {
                    return static_cast<T>(ref_);
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
            template<std::size_t I, typename S,
                CONCEPT_REQUIRES_(I < uncvref_t<S>::size)>
            RANGES_CXX14_CONSTEXPR auto&& variant_raw_get(S &&storage) noexcept
            {
                if constexpr (I == 0)
                    return static_cast<S &&>(storage).head_;
                else if constexpr (I == 1)
                    return static_cast<S &&>(storage).tail_.head_;
                else if constexpr (I == 2)
                    return static_cast<S &&>(storage).tail_.tail_.head_;
                else if constexpr (I == 3)
                    return static_cast<S &&>(storage).tail_.tail_.tail_.head_;
                else if constexpr (I < 8)
                    return variant_raw_get<I - 4>(static_cast<S &&>(storage)
                        .tail_.tail_.tail_.tail_);
                else if constexpr (I < 16)
                    return variant_raw_get<I - 8>(static_cast<S &&>(storage)
                        .tail_.tail_.tail_.tail_.tail_.tail_.tail_.tail_);
                else if constexpr (I < 32)
                    return variant_raw_get<I - 16>(static_cast<S &&>(storage)
                        .tail_.tail_.tail_.tail_.tail_.tail_.tail_.tail_
                        .tail_.tail_.tail_.tail_.tail_.tail_.tail_.tail_);
                else if constexpr (I < 64)
                    return variant_raw_get<I - 32>(static_cast<S &&>(storage)
                        .tail_.tail_.tail_.tail_.tail_.tail_.tail_.tail_
                        .tail_.tail_.tail_.tail_.tail_.tail_.tail_.tail_
                        .tail_.tail_.tail_.tail_.tail_.tail_.tail_.tail_
                        .tail_.tail_.tail_.tail_.tail_.tail_.tail_.tail_);
            }
#else
            template<std::size_t I, typename S,
                CONCEPT_REQUIRES_(I == 0 && 0 < uncvref_t<S>::size)>
            constexpr auto variant_raw_get(S &&storage) noexcept
            RANGES_DECLTYPE_AUTO_RETURN
            (
                (static_cast<S &&>(storage).head_)
            )

            template<std::size_t I, typename S,
                CONCEPT_REQUIRES_(I > 0 && I < uncvref_t<S>::size)>
            constexpr auto variant_raw_get(S &&storage) noexcept
            RANGES_DECLTYPE_AUTO_RETURN
            (
                variant_raw_get<I - 1>(static_cast<S &&>(storage).tail_)
            )
#endif

            template<typename Fn, typename Storage>
            using variant_visit_result_t = decltype(std::declval<Fn>()(meta::size_t<0>{},
                variant_raw_get<0>(std::declval<Storage>())));

            template<typename Fn, typename Storage, typename Indices>
            struct variant_raw_dispatch;
            template<typename Fn, typename Storage, std::size_t... Is>
            struct variant_raw_dispatch<Fn, Storage, meta::index_sequence<Is...>>
            {
                CONCEPT_ASSERT(sizeof...(Is) == uncvref_t<Storage>::size);
                using result_t = variant_visit_result_t<Fn, Storage>;
                using fn_t = result_t(*)(Fn &&, Storage &&);
                static constexpr result_t bad_access_target(Fn &&fn, Storage &&)
                {
                    static_cast<Fn &&>(fn).bad_access();
                }
                template<std::size_t I>
                static constexpr result_t dispatch_target(Fn &&fn, Storage &&storage)
                {
                    return static_cast<Fn &&>(fn)(meta::size_t<I>{},
                        variant_raw_get<I>(static_cast<Storage &&>(storage)));
                }
                static constexpr fn_t dispatch_table[sizeof...(Is) + 1] = {
                    bad_access_target,
                    dispatch_target<Is>...
                };
            };

            template<typename Fn, typename Storage, std::size_t... Is>
            constexpr typename variant_raw_dispatch<Fn, Storage, meta::index_sequence<Is...>>::fn_t
                variant_raw_dispatch<Fn, Storage, meta::index_sequence<Is...>>::dispatch_table[sizeof...(Is) + 1];

            template<typename Fn, typename Storage>
            constexpr variant_visit_result_t<Fn, Storage> variant_raw_visit(
                std::size_t index, Storage &&storage, Fn &&fn)
            {
                using Indices = meta::make_index_sequence<uncvref_t<Storage>::size>;
                using Dispatch = variant_raw_dispatch<Fn, Storage, Indices>;
                return Dispatch::dispatch_table[index](
                    static_cast<Fn &&>(fn), static_cast<Storage &&>(storage));
            }

            struct variant_destroy_visitor
            {
                static RANGES_CXX14_CONSTEXPR void bad_access() noexcept
                {}
                template<std::size_t I, typename T>
                RANGES_CXX14_CONSTEXPR void operator()(meta::size_t<I>, T &t) const noexcept
                {
                    t.~T();
                }
            };

            template<std::size_t N>
            using variant_index_t = meta::if_c<
                (N < static_cast<unsigned char>(~0u)),
                unsigned char, unsigned short>;

            template<typename... Types>
            struct variant_base
            {
                using index_t = variant_index_t<sizeof...(Types)>;
                static_assert(sizeof...(Types) < index_t(-1),
                    "You really need a variant with more than 2^16 alternatives?!?");

                variant_storage<Types...> storage_;
                index_t index_ = 0; // Invariant: index_ <= sizeof...(Types)

                variant_base() noexcept = default;
                template<std::size_t I, typename... Args,
                    typename T = meta::at_c<meta::list<Types...>, I>,
                    CONCEPT_REQUIRES_(Constructible<T, Args...>())>
                constexpr variant_base(meta::size_t<I>, Args &&... args)
                    noexcept(std::is_nothrow_default_constructible<T>::value)
                  : storage_{meta::size_t<I>{}, static_cast<Args &&>(args)...}
                  , index_(I + 1)
                {}

                void reset() noexcept
                {
                    destroy();
                    index_ = 0;
                }

                void destroy() noexcept
                {
#if RANGES_CXX_IF_CONSTEXPR >= RANGES_CXX_IF_CONSTEXPR_17
                    if constexpr (!all_trivially_destructible<Types...>::value)
                        variant_raw_visit(index_, storage_, variant_destroy_visitor{});
#else
                    destroy_(all_trivially_destructible<Types...>{});
#endif
                }

#if RANGES_CXX_IF_CONSTEXPR < RANGES_CXX_IF_CONSTEXPR_17
            private:
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
            };

            template<typename... Types>
            struct variant_nontrivial_destruction
              : variant_base<Types...>
            {
                using variant_base<Types...>::variant_base;
                ~variant_nontrivial_destruction() { this->destroy(); }
                variant_nontrivial_destruction() = default;
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
                variant_base<Types...> &self_;

                static RANGES_CXX14_CONSTEXPR void bad_access() noexcept
                {}
                template<std::size_t I, typename T, typename U = uncvref_t<T>,
                    typename C = decltype(std::declval<T>().cook()),
                    CONCEPT_REQUIRES_(Constructible<U, C>())>
                RANGES_CXX14_CONSTEXPR void operator()(meta::size_t<I>, T &&t) const
                    noexcept(std::is_nothrow_constructible<U, C>::value)
                {
                    RANGES_EXPECT(self_.index_ == 0);
                    auto &target = variant_raw_get<I>(self_.storage_);
                    CONCEPT_ASSERT(Same<U, uncvref_t<decltype(target)>>());
                    ::new (std::addressof(target)) U(static_cast<T &&>(t).cook());
                    self_.index_ = I + 1;
                }
            };

            template<typename... Types>
            struct variant_nontrivial_copy
              : variant_destruction_layer<Types...>
            {
                using variant_destruction_layer<Types...>::variant_destruction_layer;
                variant_nontrivial_copy() = default;
                variant_nontrivial_copy(variant_nontrivial_copy const& that)
                    noexcept(meta::strict_and<std::is_nothrow_copy_constructible<Types>...>::value)
                  : variant_destruction_layer<Types...>{}
                {
                    variant_raw_visit(that.index_, that.storage_,
                        variant_construct_visitor<Types...>{*this});
                }
                variant_nontrivial_copy(variant_nontrivial_copy &&) = default;
                variant_nontrivial_copy& operator=(variant_nontrivial_copy const&) = default;
                variant_nontrivial_copy& operator=(variant_nontrivial_copy &&) = default;
            };

            template<typename... Types>
            using variant_copy_layer = meta::if_c<
                all_copy_constructible<Types...>::value &&
                    !all_trivially_copy_constructible<Types...>::value,
                variant_nontrivial_copy<Types...>,
                variant_destruction_layer<Types...>>;

            template<typename... Types>
            struct variant_nontrivial_move
              : variant_copy_layer<Types...>
            {
                using variant_copy_layer<Types...>::variant_copy_layer;
                variant_nontrivial_move() = default;
                variant_nontrivial_move(variant_nontrivial_move const&) = default;
                variant_nontrivial_move(variant_nontrivial_move &&that)
                    noexcept(meta::and_<std::is_nothrow_move_constructible<Types>...>::value)
                  : variant_copy_layer<Types...>{}
                {
                    variant_raw_visit(that.index_, std::move(that.storage_),
                        variant_construct_visitor<Types...>{*this});
                }
                variant_nontrivial_move& operator=(variant_nontrivial_move const&) = default;
                variant_nontrivial_move& operator=(variant_nontrivial_move &&) = default;
            };

            template<typename... Types>
            using variant_move_layer = meta::if_c<
                all_move_constructible<Types...>::value &&
                    !all_trivially_move_constructible<Types...>::value,
                variant_nontrivial_move<Types...>,
                variant_copy_layer<Types...>>;

            template<typename... Types>
            struct variant_assign_same_visitor
            {
                variant_base<Types...> &self_;

                static RANGES_CXX14_CONSTEXPR void bad_access() noexcept
                {}
                template<std::size_t I, typename T>
                RANGES_CXX14_CONSTEXPR void operator()(meta::size_t<I>, T &&t) const
                RANGES_AUTO_RETURN_NOEXCEPT
                (
                    (void)(RANGES_EXPECT(self_.index_ == I + 1),
                    variant_raw_get<I>(self_.storage_).cook() = static_cast<T &&>(t).cook())
                )
            };

            template<typename... Types>
            struct variant_copy_assign_visitor
            {
                variant_base<Types...> &self_;

                static RANGES_CXX14_CONSTEXPR void bad_access() noexcept
                {}
                template<std::size_t I, typename T>
                RANGES_CXX14_CONSTEXPR void operator()(meta::size_t<I>, T const &t) const
                    noexcept(std::is_nothrow_copy_constructible<T>::value)
                {
                    constexpr bool copy_does_not_throw =
                        std::is_nothrow_copy_constructible<T>::value ||
                            !std::is_nothrow_move_constructible<T>::value;
                    helper<I>(t, meta::bool_<copy_does_not_throw>{});
                }

            private:
                template<std::size_t I, typename T>
                RANGES_CXX14_CONSTEXPR void helper(T const &t, std::true_type) const
                    noexcept(std::is_nothrow_copy_constructible<T>::value)
                {
                    this->reset();
                    auto &target = variant_raw_get<I>(self_.storage_);
                    ::new (std::addressof(target)) T(t.cook());
                }

                template<std::size_t I, typename T>
                RANGES_CXX14_CONSTEXPR void helper(T const &t, std::false_type) const
                {
                    auto tmp(t.cook());
                    this->reset();
                    auto &target = variant_raw_get<I>(self_.storage_);
                    ::new (std::addressof(target)) T(std::move(tmp));
                }
            };

            template<typename... Types>
            struct variant_nontrivial_copy_assign
              : variant_move_layer<Types...>
            {
                using variant_move_layer<Types...>::variant_move_layer;
                variant_nontrivial_copy_assign() = default;
                variant_nontrivial_copy_assign(variant_nontrivial_copy_assign const&) = default;
                variant_nontrivial_copy_assign(variant_nontrivial_copy_assign &&) = default;
                variant_nontrivial_copy_assign& operator=(variant_nontrivial_copy_assign const& that)
                    noexcept(meta::and_<std::is_nothrow_copy_assignable<Types>...>::value) // FIXME
                {
                    auto const i = this->index_;
                    if (i == that.index_)
                    {
                        variant_raw_visit(i, that.storage_,
                            variant_assign_same_visitor<Types...>{*this});
                    }
                    else
                    {
                        variant_raw_visit(i, that.storage_,
                            variant_copy_assign_visitor<Types...>{*this});
                    }
                    return *this;
                }
                variant_nontrivial_copy_assign& operator=(variant_nontrivial_copy_assign &&) = default;
            };

            template<typename... Types>
            struct variant_deleted_copy_assign
              : variant_move_layer<Types...>
            {
                using variant_move_layer<Types...>::variant_move_layer;
                variant_deleted_copy_assign() = default;
                variant_deleted_copy_assign(variant_deleted_copy_assign const&) = default;
                variant_deleted_copy_assign(variant_deleted_copy_assign &&) = default;
                variant_deleted_copy_assign& operator=(variant_deleted_copy_assign const&) = delete;
                variant_deleted_copy_assign& operator=(variant_deleted_copy_assign &&) = default;
            };

            template<typename... Types>
            using variant_copy_assign_layer = meta::if_c<
                all_copy_constructible<Types...>::value &&
                    all_copy_assignable<Types...>::value,
                meta::if_c<
                    !any_reference_type<Types...>::value &&
                        all_trivially_copy_constructible<Types...>::value &&
                        all_trivially_copy_assignable<Types...>::value,
                    variant_move_layer<Types...>,
                    variant_nontrivial_copy_assign<Types...>>,
                variant_deleted_copy_assign<Types...>>;

            template<typename... Types>
            struct variant_nontrivial_move_assign
              : variant_copy_assign_layer<Types...>
            {
                using variant_copy_assign_layer<Types...>::variant_copy_assign_layer;
                variant_nontrivial_move_assign() = default;
                variant_nontrivial_move_assign(variant_nontrivial_move_assign const&) = default;
                variant_nontrivial_move_assign(variant_nontrivial_move_assign &&) = default;
                variant_nontrivial_move_assign& operator=(variant_nontrivial_move_assign const&) = default;
                variant_nontrivial_move_assign& operator=(variant_nontrivial_move_assign &&that)
                    noexcept(meta::and_<std::is_nothrow_move_assignable<Types>...>::value) // FIXME
                {
                    auto const i = this->index_;
                    if (i == that.index_)
                    {
                        variant_raw_visit(i, std::move(that.storage_),
                            variant_assign_same_visitor<Types...>{*this});
                    }
                    else
                    {
                        this->reset();
                        variant_raw_visit(that.index_, std::move(that.storage_),
                            variant_construct_visitor<Types...>{*this});
                    }
                    return *this;
                }
            };

            template<typename... Types>
            struct variant_deleted_move_assign
              : variant_copy_assign_layer<Types...>
            {
                using variant_copy_assign_layer<Types...>::variant_copy_assign_layer;
                variant_deleted_move_assign() = default;
                variant_deleted_move_assign(variant_deleted_move_assign const&) = default;
                variant_deleted_move_assign(variant_deleted_move_assign &&) = default;
                variant_deleted_move_assign& operator=(variant_deleted_move_assign const&) = default;
                variant_deleted_move_assign& operator=(variant_deleted_move_assign &&) = delete;
            };

            template<typename... Types>
            using variant_move_assign_layer = meta::if_c<
                all_move_constructible<Types...>::value &&
                    all_move_assignable<Types...>::value,
                meta::if_c<
                    !any_reference_type<Types...>::value &&
                        all_trivially_move_constructible<Types...>::value &&
                        all_trivially_move_assignable<Types...>::value,
                    variant_copy_assign_layer<Types...>,
                    variant_nontrivial_move_assign<Types...>>,
                variant_deleted_move_assign<Types...>>;

            struct variant_access
            {
                template<typename V,
                    CONCEPT_REQUIRES_(meta::is<uncvref_t<V>, variant>::value)>
                static constexpr auto storage(V &&v)
                RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
                (
                    (static_cast<V &&>(v).storage_)
                )
            };
        } // namespace detail

        template<typename... Types>
        struct variant
          : private detail::variant_move_assign_layer<Types...>
        {
        private:
            friend detail::variant_access;
            using base_t = detail::variant_move_assign_layer<Types...>;
            using base_t::index_;
        public:
            template<typename First = meta::at_c<meta::list<Types...>, 0>,
                CONCEPT_REQUIRES_(DefaultConstructible<First>())>
            constexpr variant()
                noexcept(std::is_nothrow_default_constructible<First>::value)
              : base_t{meta::size_t<0>{}}
            {}

            template<std::size_t I, typename... Args,
                typename T = meta::at_c<meta::list<Types...>, I>,
                CONCEPT_REQUIRES_(Constructible<T, Args...>())>
            constexpr variant(RANGES_IN_PLACE_INDEX_T(I), Args &&... args)
                noexcept(std::is_nothrow_constructible<T, Args...>::value)
              : base_t{meta::size_t<I>{}, static_cast<Args &&>(args)...}
            {}

            template<typename T, typename... Args,
                std::size_t I = detail::find_unique_index<meta::list<Types...>, T>::value,
                CONCEPT_REQUIRES_(0 <= I && Constructible<T, Args...>())>
            constexpr variant(RANGES_IN_PLACE_TYPE_T(T), Args &&... args)
                noexcept(std::is_nothrow_constructible<T, Args...>::value)
              : base_t{meta::size_t<I>{}, static_cast<Args &&>(args)...}
            {}

            constexpr std::size_t index() const noexcept
            {
                return static_cast<std::size_t>(index_) - 1;
            }
            constexpr bool valueless_by_exception() const noexcept
            {
                return 0 == index_;
            }
        };

        template<class T, class... Types>
        constexpr bool holds_alternative(variant<Types...> const &v) noexcept
        {
            static_assert(detail::find_unique_index<meta::list<Types...>, T>::value >= 0,
                "T must appear exactly once in Types");
            return detail::find_unique_index<meta::list<Types...>, T>::value == v.index();
        }

        template<std::size_t I, typename V,
            CONCEPT_REQUIRES_(meta::is<uncvref_t<V>, variant>::value)>
        constexpr auto get(V &&v)
        RANGES_DECLTYPE_AUTO_RETURN
        (
            v.index() == I
                ? detail::variant_raw_get<I>(
                    detail::variant_access::storage(static_cast<V &&>(v))).cook()
                : throw bad_variant_access{}
        )

        template<std::size_t I, typename V,
            CONCEPT_REQUIRES_(meta::is<uncvref_t<V>, variant>::value)>
        constexpr auto get_unchecked(V &&v)
        RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
        (
            RANGES_EXPECT(v.index() == I),
            detail::variant_raw_get<I>(
                detail::variant_access::storage(static_cast<V &&>(v))).cook()
        )

        template<typename> struct variant_size;
        template<typename T> struct variant_size<T const> : variant_size<T> {};
        template<typename T> struct variant_size<T volatile> : variant_size<T> {};
        template<typename T> struct variant_size<T const volatile> : variant_size<T> {};
        template<typename... Types>
        struct variant_size<variant<Types...>> : meta::size_t<sizeof...(Types)> {};

#if RANGES_CXX_VARIABLE_TEMPLATES >= RANGES_CXX_VARIABLE_TEMPLATES_14
        template<typename T>
#if RANGES_CXX_INLINE_VARIABLES >= RANGES_CXX_INLINE_VARIABLES_17
        inline
#endif
        constexpr auto variant_size_v = variant_size<T>::value;
#endif

        template<std::size_t, typename>
        struct variant_alternative;

        template<std::size_t I, typename T>
        using variant_alternative_t = meta::_t<variant_alternative<I, T>>;

        template<std::size_t I, typename T>
        struct variant_alternative<I, const T> : std::add_const<variant_alternative_t<I, T>> {};
        template<std::size_t I, typename T>
        struct variant_alternative<I, volatile T> : std::add_volatile<variant_alternative_t<I, T>> {};
        template<std::size_t I, typename T>
        struct variant_alternative<I, const volatile T> : std::add_cv<variant_alternative_t<I, T>> {};

        template<std::size_t I, typename... Types>
        struct variant_alternative<I, variant<Types...>>
          : meta::id<meta::at_c<meta::list<Types...>, I>>
        {};

#if RANGES_CXX_INLINE_VARIABLES >= RANGES_CXX_INLINE_VARIABLES_17
        inline
#endif
        constexpr auto variant_npos = ~std::size_t{0};
    } // namespace v3
} // namespace ranges

#endif
