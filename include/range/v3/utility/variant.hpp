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
#include <new>
#include <type_traits>
#include <utility>
#include <meta/meta.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/utility/concepts.hpp>
#include <range/v3/utility/memory.hpp>
#include <range/v3/utility/static_const.hpp>
#include <range/v3/utility/swap.hpp>

// TODO:
// * Merge intermediate SMF layers with optional as in STLSTL

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

        /// \cond
#if RANGES_CXX_VARIABLE_TEMPLATES
        /// \endcond

        template<std::size_t I>
        struct in_place_index_t
          : meta::size_t<I>
        {};

        template<typename T>
        struct in_place_type_t
          : meta::id<T>
        {};

#if RANGES_CXX_INLINE_VARIABLES
        template<std::size_t I>
        inline constexpr in_place_index_t<I> in_place_index{};

        template<typename T>
        inline constexpr in_place_type_t<T> in_place_type{};
#else
        inline namespace
        {
            template<std::size_t I>
            constexpr auto &in_place_index = static_const<in_place_index_t<I>>::value;

            template<typename T>
            constexpr auto &in_place_type = static_const<in_place_type_t<I>>::value;
        }
#endif

        /// \cond
#else // RANGES_CXX_VARIABLE_TEMPLATES
        template<std::size_t I>
        struct _in_place_index_
          : meta::size_t<I>
        {};

        template<std::size_t I>
        constexpr _in_place_index_<I> in_place_index() noexcept
        {
            return {};
        }
        template<std::size_t I>
        using in_place_index_t = _in_place_index_<I>(&)();

        template<typename T>
        struct _in_place_type_
          : meta::id<T>
        {};

        template<typename T>
        _in_place_type_<T> in_place_type()
        {
            return {};
        }
        template<typename T>
        using in_place_type_t = _in_place_type_<T>(&)();
#endif //RANGES_CXX_VARIABLE_TEMPLATES
        /// \endcond

        namespace detail
        {
            template<typename>
            struct is_in_place_tag : std::false_type {};
            template<std::size_t I>
            struct is_in_place_tag<in_place_index_t<I>> : std::true_type {};
            template<typename T>
            struct is_in_place_tag<in_place_type_t<T>> : std::true_type {};
#if !RANGES_CXX_VARIABLE_TEMPLATES
            template<std::size_t I>
            struct is_in_place_tag<_in_place_index_<I>()> : std::true_type {};
            template<typename T>
            struct is_in_place_tag<_in_place_type_<T>()> : std::true_type {};
#endif

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
            template<typename... Types, typename T>
            struct find_unique_index_<meta::list<Types...>, T>
            {
                static constexpr bool bools[sizeof...(Types)] = {std::is_same<Types, T>::value...};
                using type = meta::size_t<find_unique_index_i_(bools, sizeof...(Types))>;
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

            template<typename = void>
            [[noreturn]] RANGES_NOINLINE bool throw_bad_variant_access()
            {
                throw bad_variant_access{};
            }

            template<typename T>
            struct variant_wrapper
            {
                meta::_t<std::remove_const<T>> t_;

                template<typename... Args,
                    CONCEPT_REQUIRES_(Constructible<T, Args...>())>
                constexpr variant_wrapper(Args &&... args)
                    noexcept(std::is_nothrow_constructible<T, Args...>::value)
                  : t_(static_cast<Args &&>(args)...)
                {}
            };

            template<typename T>
            constexpr T &cook(variant_wrapper<T> &w) noexcept
            {
                return w.t_;
            }
            template<typename T>
            constexpr T const &cook(variant_wrapper<T> const &w) noexcept
            {
                return w.t_;
            }
            template<typename T>
            constexpr T &&cook(variant_wrapper<T> &&w) noexcept
            {
                return static_cast<T &&>(w.t_);
            }
            template<typename T>
            constexpr T const &&cook(variant_wrapper<T> const &&w) noexcept
            {
                return static_cast<T const &&>(w.t_);
            }

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
                    return static_cast<Fn &&>(fn).bad_access();
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
                static void bad_access() noexcept
                {}
                template<std::size_t I, typename T>
                void operator()(meta::size_t<I>, T &t) const noexcept
                {
                    using U = uncvref_t<T>;
                    t.~U();
                }
            };

            template<std::size_t N>
            using variant_index_t = meta::if_c<
                (N < static_cast<unsigned char>(~0u)),
                unsigned char, unsigned short>;

            template<typename...>
            struct variant_base;

            template<typename... Types>
            struct variant_copymove_visitor
            {
                variant_base<Types...> &self_;

                static void bad_access() noexcept
                {}
                template<std::size_t I, typename T, typename U = uncvref_t<T>,
                    typename C = decltype(detail::cook(std::declval<T>())),
                    CONCEPT_REQUIRES_(Constructible<U, C>())>
                void operator()(meta::size_t<I>, T &&t) const
                    noexcept(std::is_nothrow_constructible<U, C>::value)
                {
                    RANGES_EXPECT(self_.index_ == 0);
                    auto &target = variant_raw_get<I>(self_.storage_);
                    CONCEPT_ASSERT(Same<U, uncvref_t<decltype(target)>>());
                    ::new (detail::addressof(target)) U(detail::cook(static_cast<T &&>(t)));
                    self_.index_ = I + 1;
                }
            };

            template<typename... Types>
            struct variant_transfer_visitor
              : variant_copymove_visitor<Types...>
            {
                variant_base<Types...> &source_;

                constexpr variant_transfer_visitor(variant_base<Types...> &source,
                    variant_base<Types...> &target) noexcept
                  : variant_copymove_visitor<Types...>{target}
                  , source_(source)
                {}

                template<std::size_t I, typename T, typename U = uncvref_t<T>,
                    typename C = decltype(detail::cook(std::declval<T>())),
                    CONCEPT_REQUIRES_(Constructible<U, C>())>
                void operator()(meta::size_t<I>, T &&t) const
                    noexcept(std::is_nothrow_constructible<U, C>::value)
                {
                    variant_copymove_visitor<Types...>::operator()(
                        meta::size_t<I>{}, static_cast<T &&>(t));
                    t.~U();
                    source_.index_ = 0;
                }
            };

            template<typename... Types>
            struct variant_base
            {
                using index_t = variant_index_t<sizeof...(Types)>;
                static_assert(sizeof...(Types) < index_t(-1),
                    "You really need a variant with more than 65535 alternatives?!?");

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

                void reset_() noexcept
                {   // transition to the valueless state
                    destroy_();
                    index_ = 0;
                }

                CONCEPT_REQUIRES(meta::strict_and<MoveConstructible<Types>...>::value)
                RANGES_CXX14_CONSTEXPR void swap(variant_base &that)
                    noexcept(meta::strict_and<std::is_nothrow_move_constructible<Types>...>::value)
                {
                    swap_(that, 42);
                }

                RANGES_CXX14_CONSTEXPR void destroy_() noexcept
                {   // destroy the currently active alternative (leave index_ unchanged)
#if RANGES_CXX_IF_CONSTEXPR >= RANGES_CXX_IF_CONSTEXPR_17
                    if constexpr (!all_trivially_destructible<Types...>::value)
                        variant_raw_visit(index_, storage_, variant_destroy_visitor{});
#else
                    destroy_(all_trivially_destructible<Types...>{});
#endif
                }

#if RANGES_CXX_IF_CONSTEXPR < RANGES_CXX_IF_CONSTEXPR_17
            private:
                RANGES_CXX14_CONSTEXPR void destroy_(std::true_type) noexcept
                {
                    CONCEPT_ASSERT(all_trivially_destructible<Types...>::value);
                }
                void destroy_(std::false_type) noexcept
                {
                    CONCEPT_ASSERT(!all_trivially_destructible<Types...>::value);
                    variant_raw_visit(index_, storage_, variant_destroy_visitor{});
                }
#endif
            private:
                CONCEPT_REQUIRES(detail::is_trivially_copyable<variant_storage<Types...>>::value
                    && !meta::strict_or<adl_swap_detail::is_adl_swappable_<Types &>...>::value)
                RANGES_CXX14_CONSTEXPR void swap_(variant_base &that, int) noexcept
                {
                    ranges::swap(*this, that);
                }
                CONCEPT_REQUIRES(meta::strict_and<MoveConstructible<Types>...>::value)
                void steal_from(variant_base &source)
                    noexcept(meta::strict_and<std::is_nothrow_move_constructible<Types>...>::value)
                {
                    variant_raw_visit(source.index_, detail::move(source.storage_),
                        variant_transfer_visitor<Types...>{source, *this});
                }
                CONCEPT_REQUIRES(meta::strict_and<MoveConstructible<Types>...>::value)
                void swap_(variant_base &that, ...)
                    noexcept(meta::strict_and<std::is_nothrow_move_constructible<Types>...>::value)
                {
                    variant_base tmp;
                    tmp.steal_from(that);
                    that.steal_from(*this);
                    this->steal_from(tmp);
                    RANGES_EXPECT(tmp.index_ == 0);
                }
            };

            template<typename... Types>
            struct variant_nontrivial_destruction
              : variant_base<Types...>
            {
                using variant_base<Types...>::variant_base;
                ~variant_nontrivial_destruction()
                {
                    CONCEPT_ASSERT(!all_trivially_destructible<Types...>::value);
                    this->destroy_();
                }
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
            struct variant_nontrivial_copy
              : variant_destruction_layer<Types...>
            {
                using variant_destruction_layer<Types...>::variant_destruction_layer;
                variant_nontrivial_copy() = default;
                variant_nontrivial_copy(variant_nontrivial_copy const& that)
                    noexcept(meta::strict_and<std::is_nothrow_copy_constructible<Types>...>::value)
                  : variant_destruction_layer<Types...>{}
                {
                    CONCEPT_ASSERT(all_copy_constructible<Types...>::value &&
                        !all_trivially_copy_constructible<Types...>::value);
                    variant_raw_visit(that.index_, that.storage_,
                        variant_copymove_visitor<Types...>{*this});
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
                    CONCEPT_ASSERT(all_move_constructible<Types...>::value &&
                        !all_trivially_move_constructible<Types...>::value);
                    variant_raw_visit(that.index_, std::move(that.storage_),
                        variant_copymove_visitor<Types...>{*this});
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
                    detail::cook(variant_raw_get<I>(self_.storage_)) = detail::cook(static_cast<T &&>(t)))
                )
            };

            template<typename... Types>
            struct variant_copy_assign_visitor
            {
                variant_base<Types...> &self_;

                void bad_access() noexcept
                {
                    self_.reset_();
                }
                template<std::size_t I, typename T>
                void operator()(meta::size_t<I>, T const &t) const
                    noexcept(std::is_nothrow_copy_constructible<T>::value)
                {
                    constexpr bool copy_does_not_throw =
                        std::is_nothrow_copy_constructible<T>::value ||
                            !std::is_nothrow_move_constructible<T>::value;
                    impl<I>(t, meta::bool_<copy_does_not_throw>{});
                }
            private:
                template<std::size_t I, typename T>
                void impl(T const &t, std::true_type) const
                    noexcept(std::is_nothrow_copy_constructible<T>::value)
                {
                    self_.reset_();
                    auto &target = variant_raw_get<I>(self_.storage_);
                    ::new (detail::addressof(target)) T(detail::cook(t));
                    self_.index_ = I + 1;
                }
                template<std::size_t I, typename T>
                void impl(T const &t, std::false_type) const
                {
                    auto tmp(detail::cook(t));
                    self_.reset_();
                    auto &target = variant_raw_get<I>(self_.storage_);
                    ::new (detail::addressof(target)) T(std::move(tmp));
                    self_.index_ = I + 1;
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
                    noexcept(meta::and_<std::is_nothrow_copy_constructible<Types>...,
                        std::is_nothrow_copy_assignable<Types>...>::value)
                {
                    auto const i = that.index_;
                    if (i == this->index_)
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
                    noexcept(meta::and_<std::is_nothrow_move_constructible<Types>...,
                        std::is_nothrow_move_assignable<Types>...>::value)
                {
                    auto const i = that.index_;
                    if (i == this->index_)
                    {
                        variant_raw_visit(i, std::move(that.storage_),
                            variant_assign_same_visitor<Types...>{*this});
                    }
                    else
                    {
                        this->reset_();
                        variant_raw_visit(that.index_, std::move(that.storage_),
                            variant_copymove_visitor<Types...>{*this});
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
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 5
                template<typename... Types>
                static constexpr variant_index_t<sizeof...(Types)> &
                index(variant<Types...> &v) noexcept
                {
                    return v.index_;
                }
                template<typename... Types>
                static constexpr variant_index_t<sizeof...(Types)> const &
                index(variant<Types...> const &v) noexcept
                {
                    return v.index_;
                }
                template<typename... Types>
                static constexpr variant_index_t<sizeof...(Types)> &&
                index(variant<Types...> &&v) noexcept
                {
                    return detail::move(v.index_);
                }
                template<typename... Types>
                static constexpr variant_index_t<sizeof...(Types)> const &&
                index(variant<Types...> const &&v) noexcept
                {
                    return detail::move(v.index_);
                }
                template<typename... Types>
                static constexpr variant_storage<Types...> &
                storage(variant<Types...> &v) noexcept
                {
                    return v.storage_;
                }
                template<typename... Types>
                static constexpr variant_storage<Types...> const &
                storage(variant<Types...> const &v) noexcept
                {
                    return v.storage_;
                }
                template<typename... Types>
                static constexpr variant_storage<Types...> &&
                storage(variant<Types...> &&v) noexcept
                {
                    return detail::move(v.storage_);
                }
                template<typename... Types>
                static constexpr variant_storage<Types...> const &&
                storage(variant<Types...> const &&v) noexcept
                {
                    return detail::move(v.storage_);
                }
#else // GCC4
                template<typename V,
                    CONCEPT_REQUIRES_(meta::is<uncvref_t<V>, variant>::value)>
                static constexpr auto index(V &&v) noexcept
                RANGES_DECLTYPE_AUTO_RETURN
                (
                    (static_cast<V &&>(v).index_)
                )
                template<typename V,
                    CONCEPT_REQUIRES_(meta::is<uncvref_t<V>, variant>::value)>
                static constexpr auto storage(V &&v) noexcept
                RANGES_DECLTYPE_AUTO_RETURN
                (
                    (static_cast<V &&>(v).storage_)
                )
#endif // GCC4
            };

            template<std::size_t I, typename T>
            struct variant_single_overload
            {
                using fn_t = meta::size_t<I>(*)(T);
                operator fn_t () const;
            };

            template<typename Indices, typename... Types>
            struct variant_overload_set;

            template<std::size_t... Is, typename... Types>
            struct variant_overload_set<meta::index_sequence<Is...>, Types...>
              : variant_single_overload<Is, Types>...
            {};
            template<typename T, typename... Types>
            using variant_construct_index_t = decltype(
                variant_overload_set<
                    meta::make_index_sequence<sizeof...(Types)>, Types...>{}(std::declval<T>()));

#if 0 // FIXME: unused
            template<std::size_t I, typename V>
            struct variant_get_visitor
            {
                using result_t = decltype(detail::cook(variant_raw_get<I>(
                    variant_access::storage(std::declval<V>()))));
                [[noreturn]] static result_t bad_access()
                {
                    throw_bad_variant_access();
                }
                template<typename Item>
                constexpr result_t operator()(meta::size_t<I>, Item &&i) noexcept
                {
                    return detail::cook(static_cast<Item &&>(i));
                }
                [[noreturn]] static result_t operator()(detail::any, detail::any)
                {
                    throw_bad_variant_access();
                }
            };
#endif
        } // namespace detail

        template<typename... Types>
        struct variant
          : private detail::variant_move_assign_layer<Types...>
        {
        private:
            friend detail::variant_access;
            using base_t = detail::variant_move_assign_layer<Types...>;
            using base_t::index_;
            using base_t::storage_;

            template<std::size_t I, typename U, typename T>
            void assign_(std::true_type, T &&t)
                noexcept(std::is_nothrow_constructible<U, T>::value)
            {
                emplace_<I, U>(static_cast<T &&>(t));
            }
            template<std::size_t I, typename U, typename T>
            void assign_(std::false_type, T &&t)
                noexcept(std::is_nothrow_constructible<U, T>::value &&
                    std::is_nothrow_move_constructible<U>::value)
            {
                using W = detail::variant_wrapper<U>;
                W tmp(static_cast<T &&>(t));
                this->reset_();
                auto &target = detail::variant_raw_get<I>(storage_);
                CONCEPT_ASSERT(Same<uncvref_t<decltype(target)>, W>());
                ::new (detail::addressof(target)) W(std::move(tmp));
                index_ = I + 1;
            }

            template<std::size_t I, typename T, typename... Args>
            T &emplace_(Args &&... args)
                noexcept(std::is_nothrow_constructible<T, Args...>::value)
            {
                CONCEPT_ASSERT(Same<T, meta::at_c<meta::list<Types...>, I>>());
                CONCEPT_ASSERT(Constructible<T, Args...>());
                using W = detail::variant_wrapper<T>;
                this->reset_();
                auto &target = detail::variant_raw_get<I>(storage_);
                CONCEPT_ASSERT(Same<uncvref_t<decltype(target)>, W>());
                ::new (detail::addressof(target)) W(static_cast<Args &&>(args)...);
                index_ = I + 1;
                return detail::cook(target);
            }
        public:
            template<typename First = meta::at_c<meta::list<Types...>, 0>,
                CONCEPT_REQUIRES_(DefaultConstructible<First>())>
            constexpr variant()
                noexcept(std::is_nothrow_default_constructible<First>::value)
              : base_t{meta::size_t<0>{}}
            {}

            template<typename T,
                CONCEPT_REQUIRES_(sizeof...(Types) > 0 &&
                    !Same<uncvref_t<T>, variant>() &&
                    !detail::is_in_place_tag<uncvref_t<T>>()),
                std::size_t I = detail::variant_construct_index_t<T, Types...>::value,
                typename U = meta::at_c<meta::list<Types...>, I>,
                CONCEPT_REQUIRES_(Constructible<U, T>())>
            constexpr variant(T &&t)
                noexcept(std::is_nothrow_constructible<U, T>::value)
              : base_t{meta::size_t<I>{}, static_cast<T &&>(t)}
            {}

            template<std::size_t I, typename... Args,
                typename T = meta::at_c<meta::list<Types...>, I>,
                CONCEPT_REQUIRES_(Constructible<T, Args...>())>
            explicit constexpr variant(in_place_index_t<I>, Args &&... args)
                noexcept(std::is_nothrow_constructible<T, Args...>::value)
              : base_t{meta::size_t<I>{}, static_cast<Args &&>(args)...}
            {}

            template<std::size_t I, typename E, typename... Args,
                typename T = meta::at_c<meta::list<Types...>, I>,
                CONCEPT_REQUIRES_(Constructible<T, std::initializer_list<E> &, Args...>())>
            explicit constexpr variant(in_place_index_t<I>, std::initializer_list<E> &&il, Args &&... args)
                noexcept(std::is_nothrow_constructible<T, std::initializer_list<E> &, Args...>::value)
              : base_t{meta::size_t<I>{}, il, static_cast<Args &&>(args)...}
            {}

            template<typename T, typename... Args,
                std::size_t I = detail::find_unique_index<meta::list<Types...>, T>::value,
                CONCEPT_REQUIRES_(I != std::size_t(-1) && Constructible<T, Args...>())>
            explicit constexpr variant(in_place_type_t<T>, Args &&... args)
                noexcept(std::is_nothrow_constructible<T, Args...>::value)
              : base_t{meta::size_t<I>{}, static_cast<Args &&>(args)...}
            {}

            template<typename T, typename E, typename... Args,
                std::size_t I = detail::find_unique_index<meta::list<Types...>, T>::value,
                CONCEPT_REQUIRES_(I != std::size_t(-1) &&
                    Constructible<T, std::initializer_list<E> &, Args...>())>
            explicit constexpr variant(
                in_place_type_t<T>, std::initializer_list<E> &&il, Args &&... args)
                noexcept(std::is_nothrow_constructible<
                    T, std::initializer_list<E> &, Args...>::value)
              : base_t{meta::size_t<I>{}, il, static_cast<Args &&>(args)...}
            {}

            template<typename T,
                CONCEPT_REQUIRES_(!Same<variant, uncvref_t<T>>()),
                std::size_t I = detail::variant_construct_index_t<T, Types...>::value,
                typename U = meta::at_c<meta::list<Types...>, I>,
                CONCEPT_REQUIRES_(Constructible<U, T>() && Assignable<U &, T>())>
            RANGES_CXX14_CONSTEXPR variant &operator=(T &&t)
                noexcept(std::is_nothrow_constructible<U, T>::value &&
                    std::is_nothrow_assignable<U &, T>::value)
            {
                if (index_ == I + 1)
                    detail::cook(detail::variant_raw_get<I>(storage_)) = static_cast<T &&>(t);
                else
                {
                    constexpr bool construct_in_place =
                        std::is_nothrow_constructible<U, T>::value ||
                        !std::is_nothrow_move_constructible<U>::value;
                    assign_<I, U>(meta::bool_<construct_in_place>{}, static_cast<T &&>(t));
                }
                return *this;
            }

            template<std::size_t I, typename... Args,
                typename T = meta::at_c<meta::list<Types...>, I>,
                CONCEPT_REQUIRES_(Constructible<T, Args...>())>
            T &emplace(Args &&... args)
                noexcept(std::is_nothrow_constructible<T, Args...>::value)
            {
                return emplace_<I, T>(static_cast<Args &&>(args)...);
            }

            template<std::size_t I, typename E, typename... Args,
                typename T = meta::at_c<meta::list<Types...>, I>,
                CONCEPT_REQUIRES_(Constructible<T, std::initializer_list<E> &, Args...>())>
            T &emplace(std::initializer_list<E> il, Args &&... args)
                noexcept(std::is_nothrow_constructible<T, std::initializer_list<E> &, Args...>::value)
            {
                return emplace_<I, T>(il, static_cast<Args &&>(args)...);
            }

            template<typename T, typename... Args,
                 std::size_t I = detail::find_unique_index<meta::list<Types...>, T>::value,
                CONCEPT_REQUIRES_(I != std::size_t(-1) && Constructible<T, Args...>())>
            T &emplace(Args &&... args)
                noexcept(std::is_nothrow_constructible<T, Args...>::value)
            {
                return emplace_<I, T>(static_cast<Args &&>(args)...);
            }

            template<typename T, typename E, typename... Args,
                 std::size_t I = detail::find_unique_index<meta::list<Types...>, T>::value,
                CONCEPT_REQUIRES_(I != std::size_t(-1) &&
                    Constructible<T, std::initializer_list<E> &, Args...>())>
            T &emplace(std::initializer_list<E> il, Args &&... args)
                noexcept(std::is_nothrow_constructible<T, std::initializer_list<E> &, Args...>::value)
            {
                return emplace_<I, T>(il, static_cast<Args &&>(args)...);
            }

            constexpr std::size_t index() const noexcept
            {
                return static_cast<std::size_t>(index_) - 1;
            }
            constexpr bool valueless_by_exception() const noexcept
            {
                return 0 == index_;
            }

            void swap(variant &that) // FIXME!!
            {
                base_t::swap(that);
            }
        };

        template<class T, class... Types>
        constexpr bool holds_alternative(variant<Types...> const &v) noexcept
        {
            static_assert(detail::find_unique_index<meta::list<Types...>, T>::value !=
                std::size_t(-1), "T must appear exactly once in Types");
            return detail::find_unique_index<meta::list<Types...>, T>::value == v.index();
        }

        template<std::size_t I, typename... Types,
            CONCEPT_REQUIRES_(I < sizeof...(Types))>
        constexpr meta::at_c<meta::list<Types...>, I> &
        get_unchecked(variant<Types...> &v) noexcept
        {
            return RANGES_EXPECT(v.index() == I),
                detail::cook(detail::variant_raw_get<I>(
                    detail::variant_access::storage(v)));
        }

        template<std::size_t I, typename... Types,
            CONCEPT_REQUIRES_(I < sizeof...(Types))>
        constexpr meta::at_c<meta::list<Types...>, I> const &
        get_unchecked(variant<Types...> const &v) noexcept
        {
            return RANGES_EXPECT(v.index() == I),
                detail::cook(detail::variant_raw_get<I>(
                    detail::variant_access::storage(v)));
        }

        template<std::size_t I, typename... Types,
            CONCEPT_REQUIRES_(I < sizeof...(Types))>
        constexpr meta::at_c<meta::list<Types...>, I> &&
        get_unchecked(variant<Types...> &&v) noexcept
        {
            return RANGES_EXPECT(v.index() == I),
                detail::cook(detail::variant_raw_get<I>(
                    detail::variant_access::storage(detail::move(v))));
        }

        template<std::size_t I, typename... Types,
            CONCEPT_REQUIRES_(I < sizeof...(Types))>
        constexpr meta::at_c<meta::list<Types...>, I> const &&
        get_unchecked(variant<Types...> const &&v) noexcept
        {
            return RANGES_EXPECT(v.index() == I),
                detail::cook(detail::variant_raw_get<I>(
                    detail::variant_access::storage(detail::move(v))));
        }

        template<std::size_t I, typename... Types,
            CONCEPT_REQUIRES_(I < sizeof...(Types))>
        constexpr meta::at_c<meta::list<Types...>, I> &get(variant<Types...> &v)
        {
            return (void)(v.index() == I || detail::throw_bad_variant_access()),
                detail::cook(detail::variant_raw_get<I>(
                    detail::variant_access::storage(v)));
        }

        template<std::size_t I, typename... Types,
            CONCEPT_REQUIRES_(I < sizeof...(Types))>
        constexpr meta::at_c<meta::list<Types...>, I> const &get(variant<Types...> const &v)
        {
            return (void)(v.index() == I || detail::throw_bad_variant_access()),
                detail::cook(detail::variant_raw_get<I>(
                    detail::variant_access::storage(v)));
        }

        template<std::size_t I, typename... Types,
            CONCEPT_REQUIRES_(I < sizeof...(Types))>
        constexpr meta::at_c<meta::list<Types...>, I> &&get(variant<Types...> &&v)
        {
            return (void)(v.index() == I || detail::throw_bad_variant_access()),
                detail::cook(detail::variant_raw_get<I>(
                    detail::variant_access::storage(detail::move(v))));
        }

        template<std::size_t I, typename... Types,
            CONCEPT_REQUIRES_(I < sizeof...(Types))>
        constexpr meta::at_c<meta::list<Types...>, I> const &&get(variant<Types...> const &&v)
        {
            return (void)(v.index() == I || detail::throw_bad_variant_access()),
                detail::cook(detail::variant_raw_get<I>(
                    detail::variant_access::storage(detail::move(v))));
        }

        template<typename T, typename... Types,
            std::size_t I = detail::find_unique_index<meta::list<Types...>, T>::value>
        constexpr T &get_unchecked(variant<Types...> &v) noexcept
        {
            static_assert(I != std::size_t(-1),
                "T must appear exactly once in Types...");
            return get_unchecked<I>(v);
        }

        template<typename T, typename... Types,
            std::size_t I = detail::find_unique_index<meta::list<Types...>, T>::value>
        constexpr T const &get_unchecked(variant<Types...> const &v) noexcept
        {
            static_assert(I != std::size_t(-1),
                "T must appear exactly once in Types...");
            return get_unchecked<I>(v);
        }

        template<typename T, typename... Types,
            std::size_t I = detail::find_unique_index<meta::list<Types...>, T>::value>
        constexpr T &&get_unchecked(variant<Types...> &&v) noexcept
        {
            static_assert(I != std::size_t(-1),
                "T must appear exactly once in Types...");
            return get_unchecked<I>(detail::move(v));
        }

        template<typename T, typename... Types,
            std::size_t I = detail::find_unique_index<meta::list<Types...>, T>::value>
        constexpr T const &&get_unchecked(variant<Types...> const &&v) noexcept
        {
            static_assert(I != std::size_t(-1),
                "T must appear exactly once in Types...");
            return get_unchecked<I>(detail::move(v));
        }

        template<typename T, typename... Types,
            std::size_t I = detail::find_unique_index<meta::list<Types...>, T>::value>
        constexpr T &get(variant<Types...> &v)
        {
            static_assert(I != std::size_t(-1),
                "T must appear exactly once in Types...");
            return get<I>(v);
        }

        template<typename T, typename... Types,
            std::size_t I = detail::find_unique_index<meta::list<Types...>, T>::value>
        constexpr T const &get(variant<Types...> const &v)
        {
            static_assert(I != std::size_t(-1),
                "T must appear exactly once in Types...");
            return get<I>(v);
        }

        template<typename T, typename... Types,
            std::size_t I = detail::find_unique_index<meta::list<Types...>, T>::value>
        constexpr T &&get(variant<Types...> &&v)
        {
            static_assert(I != std::size_t(-1),
                "T must appear exactly once in Types...");
            return get<I>(detail::move(v));
        }

        template<typename T, typename... Types,
            std::size_t I = detail::find_unique_index<meta::list<Types...>, T>::value>
        constexpr T const &&get(variant<Types...> const &&v)
        {
            static_assert(I != std::size_t(-1),
                "T must appear exactly once in Types...");
            return get<I>(detail::move(v));
        }

        template<std::size_t I, typename... Types,
            CONCEPT_REQUIRES_(I < sizeof...(Types))>
        constexpr meta::_t<std::add_pointer<meta::at_c<meta::list<Types...>, I>>>
        get_if(variant<Types...> *v) noexcept
        {
            return v && v->index() == I
                ? detail::addressof(detail::cook(detail::variant_raw_get<I>(
                    detail::variant_access::storage(*v))))
                : nullptr;
        }

        template<std::size_t I, typename... Types,
            CONCEPT_REQUIRES_(I < sizeof...(Types))>
        constexpr meta::_t<std::add_pointer<meta::at_c<meta::list<Types...>, I> const>>
        get_if(variant<Types...> const *v) noexcept
        {
            return v && v->index() == I
                ? detail::addressof(detail::cook(detail::variant_raw_get<I>(
                    detail::variant_access::storage(*v))))
                : nullptr;
        }

        template<typename T, typename... Types,
            std::size_t I = detail::find_unique_index<meta::list<Types...>, T>::value,
            CONCEPT_REQUIRES_(I != std::size_t(-1))>
        constexpr meta::_t<std::add_pointer<T>> get_if(variant<Types...> *v) noexcept
        {
            return ranges::get_if<I>(v);
        }

        template<typename T, typename... Types,
            std::size_t I = detail::find_unique_index<meta::list<Types...>, T>::value,
            CONCEPT_REQUIRES_(I != std::size_t(-1))>
        constexpr meta::_t<std::add_pointer<T const>> get_if(variant<Types...> const *v) noexcept
        {
            return ranges::get_if<I>(v);
        }

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

        struct monostate {};

        constexpr bool operator<(monostate, monostate) noexcept
        {
            return false;
        }
        constexpr bool operator>(monostate, monostate) noexcept
        {
            return false;
        }
        constexpr bool operator<=(monostate, monostate) noexcept
        {
            return true;
        }
        constexpr bool operator>=(monostate, monostate) noexcept
        {
            return true;
        }
        constexpr bool operator==(monostate, monostate) noexcept
        {
            return true;
        }
        constexpr bool operator!=(monostate, monostate) noexcept
        {
            return false;
        }
    } // namespace v3
} // namespace ranges

#endif
