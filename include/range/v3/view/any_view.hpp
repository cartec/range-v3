/// \file
// Range v3 library
//
//  Copyright Eric Niebler 2014
//  Copyright Casey Carter 2017
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3
//

#ifndef RANGES_V3_VIEW_ANY_VIEW_HPP
#define RANGES_V3_VIEW_ANY_VIEW_HPP

#include <cstring>
#include <typeinfo>
#include <type_traits>
#include <utility>
#include <range/v3/begin_end.hpp>
#include <range/v3/range_concepts.hpp>
#include <range/v3/range_traits.hpp>
#include <range/v3/view_facade.hpp>
#include <range/v3/utility/compressed_pair.hpp>
#include <range/v3/utility/memory.hpp>
#include <range/v3/view/all.hpp>

namespace ranges
{
    inline namespace v3
    {
        enum class category
        {
            input,
            forward,
            bidirectional,
            random_access
        };

        template<typename Ref, category Cat = category::input>
        struct any_view;

        /// \cond
        namespace detail
        {
            constexpr category to_cat_(concepts::InputIterator *) { return category::input; }
            constexpr category to_cat_(concepts::InputRange *) { return category::input; }
            constexpr category to_cat_(concepts::ForwardIterator *) { return category::forward; }
            constexpr category to_cat_(concepts::ForwardRange *) { return category::forward; }
            constexpr category to_cat_(concepts::BidirectionalIterator *) { return category::bidirectional; }
            constexpr category to_cat_(concepts::BidirectionalRange *) { return category::bidirectional; }
            constexpr category to_cat_(concepts::RandomAccessIterator *) { return category::random_access; }
            constexpr category to_cat_(concepts::RandomAccessRange *) { return category::random_access; }

            template<typename Rng, typename Ref>
            using AnyCompatibleRange = ConvertibleTo<range_reference_t<Rng>, Ref>;

            template<typename Rng, typename = void>
            struct any_view_sentinel_impl
              : private box<sentinel_t<Rng>, any_view_sentinel_impl<Rng>>
            {
                any_view_sentinel_impl() = default;
                any_view_sentinel_impl(Rng &rng)
                    noexcept(noexcept(box_t(ranges::end(rng))))
                  : box_t(ranges::end(rng))
                {}
                void init(Rng &rng) noexcept
                {
                    box_t::get() = ranges::end(rng);
                }
                sentinel_t<Rng> const &get(Rng const &) const noexcept
                {
                    return box_t::get();
                }
            private:
                using box_t = typename any_view_sentinel_impl::box;
            };

            template<typename Rng>
            struct any_view_sentinel_impl<Rng, meta::void_<
                decltype(ranges::end(std::declval<Rng const &>()))>>
            {
                any_view_sentinel_impl() = default;
                any_view_sentinel_impl(Rng &) noexcept
                {}
                void init(Rng &) noexcept
                {}
                sentinel_t<Rng> get(Rng const &rng) const noexcept
                {
                    return ranges::end(rng);
                }
            };

            template<typename Ref>
            struct any_input_view_interface
            {
                virtual ~any_input_view_interface() = default;
                virtual void init() = 0;
                virtual bool done() const = 0;
                virtual Ref read() const = 0;
                virtual void next() = 0;
            };

            template<typename Ref>
            struct any_input_cursor
            {
                using single_pass = std::true_type;

                any_input_cursor() = default;
                constexpr any_input_cursor(any_input_view_interface<Ref> &view) noexcept
                  : view_{std::addressof(view)}
                {}
                Ref read() const { return view_->read(); }
                void next() { view_->next(); }
                bool equal(any_input_cursor const &) const noexcept
                {
                    return true;
                }
                bool equal(default_sentinel) const
                {
                    return !view_ || view_->done();
                }
            private:
                any_input_view_interface<Ref> *view_ = nullptr;
            };

            template<typename Rng, typename Ref>
            struct any_input_view_impl
              : any_input_view_interface<Ref>
              , private tagged_compressed_tuple<tag::range(Rng),
                    tag::current(iterator_t<Rng>)>
              , private any_view_sentinel_impl<Rng>
            {
                CONCEPT_ASSERT(AnyCompatibleRange<Rng, Ref>());

                explicit any_input_view_impl(Rng rng_)
                  : tagged_t{std::move(rng_), iterator_t<Rng>{}}
                {}
                any_input_view_impl(any_input_view_impl const &) = delete;
                any_input_view_impl &operator=(any_input_view_impl const &) = delete;

            private:
                using tagged_t = tagged_compressed_tuple<tag::range(Rng),
                    tag::current(iterator_t<Rng>)>;
                using tagged_t::range;
                using tagged_t::current;
                using sentinel_box_t = any_view_sentinel_impl<Rng>;

                virtual void init() override
                {
                    auto &rng = range();
                    sentinel_box_t::init(rng);
                    current() = ranges::begin(rng);
                }
                virtual bool done() const override
                {
                    return current() == sentinel_box_t::get(range());
                }
                virtual Ref read() const override { return *current(); }
                virtual void next() override { ++current(); }
            };

            template<std::size_t PointerCount>
            struct type_erased
            {
                CONCEPT_ASSERT(PointerCount >= 2);
                CONCEPT_ASSERT(PointerCount * sizeof(void *)
                    % alignof(std::max_align_t) == 0);

                alignas(std::max_align_t) char space_[
                    (PointerCount - 1) * sizeof(void *)];
                void const *raw_vtable_;

                explicit constexpr type_erased(void const *vtable) noexcept
                  : space_{}, raw_vtable_{vtable}
                {}

                type_erased(type_erased const &) = delete;
                type_erased &operator=(type_erased const &) = delete;

                template<typename T>
                using is_small = meta::bool_<
                    sizeof(T) <= sizeof(space_) &&
                    alignof(T) <= alignof(decltype(space_)) &&
                    std::is_nothrow_move_constructible<T>::value &&
                    std::is_nothrow_move_assignable<T>::value>;

                static void trivial_copy(type_erased &dst, type_erased const &src) noexcept
                {
                    std::memcpy(dst.space_, src.space_, sizeof(dst.space_));
                }
                static void trivial_move(type_erased &dst, type_erased &src) noexcept
                {
                    trivial_copy(dst, src);
                }
            };

            using any_cursor_storage = type_erased<4>;
            using any_view_storage = type_erased<6>;

            template<typename... Args>
            void nop_function(Args...) noexcept
            {}

            template<typename Result, typename... Args>
            Result uncalled_function(Args...) noexcept
            {
                RANGES_ENSURE(false);
            }

            template<typename Result, Result Value, typename... Args>
            Result const_function(Args...) noexcept
            {
                return Value;
            }

            template<typename T, typename Storage>
            struct type_erased_impl
            {
                static T *&big_pointer(Storage &self) noexcept
                {
                    return reinterpret_cast<T *&>(self.space_[0]);
                }
                static T const &get(Storage const &self) noexcept
                {
                    if /* constexpr */ (Storage::template is_small<T>::value)
                        return reinterpret_cast<T const &>(self.space_[0]);
                    else
                        return *reinterpret_cast<T const * const &>(self.space_[0]);
                }
                static T &get(Storage &self) noexcept
                {
                    return const_cast<T &>(get(as_const(self)));
                }

                static void small_destroy(Storage &self) noexcept
                {
                    get(self).~T();
                }
                static void small_copy_init(Storage &self, Storage const &that)
                {
                    ::new (static_cast<void *>(self.space_)) T(get(that));
                }
                static void small_move_init(Storage &self, Storage &that) noexcept
                {
                    ::new (static_cast<void *>(self.space_)) T(std::move(get(that)));
                }
                static void small_move_assign(Storage &self, Storage &that) noexcept
                {
                    get(self) = std::move(get(that));
                }

                static void big_destroy(Storage &self) noexcept
                {
                    delete big_pointer(self);
                }
                static void big_copy_init(Storage &self, Storage const &that)
                {
                    using IPtr = T *;
                    auto target = std::addressof(big_pointer(self));
                    ::new (static_cast<void *>(target)) IPtr{new T(get(that))};
                }
                template<typename VT, VT const *NullVTable>
                struct nested
                {
                    static void big_move_init(Storage &self, Storage &that) noexcept
                    {
                        using IPtr = T *;
                        auto target = std::addressof(big_pointer(self));
                        ::new (static_cast<void *>(target)) IPtr{big_pointer(that)};
                        that.raw_vtable_ = NullVTable;
                    }
                };
                static void big_move_assign(Storage &self, Storage &that) noexcept
                {
                    // Meh, move assign implemented with swap
                    ranges::swap(big_pointer(self), big_pointer(that));
                }

                static void copy_assign(Storage &self, Storage const &that)
                {
                    get(self) = get(that);
                }
            };

            template<typename I>
            struct any_cursor_impl
              : type_erased_impl<I, any_cursor_storage>
            {
                using ACS = any_cursor_storage;

                using type_erased_impl<I, any_cursor_storage>::get;

                template<typename Ref>
                static Ref read(ACS const &self)
                {
                    return *get(self);
                }
                static void next(ACS &self)
                {
                    ++get(self);
                }
                static bool equal(ACS const &self, ACS const &that)
                {
                    return get(self) == get(that);
                }

                static void prev(ACS &self)
                {
                    --get(self);
                }

                static void advance(ACS &self, std::ptrdiff_t n)
                {
                    get(self) += n;
                }
                static std::ptrdiff_t distance_to(ACS const &self, ACS const &that)
                {
                    return get(that) - get(self);
                }
            };

            template<typename Rng>
            struct any_view_and_sentinel
              : private box<Rng, any_view_and_sentinel<Rng>>
              , private any_view_sentinel_impl<Rng>
            {
                using range_box_t = box<Rng, any_view_and_sentinel>;
                using sentinel_box_t = any_view_sentinel_impl<Rng>;

                any_view_and_sentinel(Rng rng)
                  : range_box_t{std::move(rng)}
                  , sentinel_box_t{range_box_t::get()} // NB: initialization order dependence
                {}

                Rng &range() & noexcept { return range_box_t::get(); }
                Rng const &range() const & noexcept { return range_box_t::get(); }
                void range() const && = delete;

                auto sentinel() const &
                RANGES_DECLTYPE_NOEXCEPT(
                    std::declval<sentinel_box_t const &>().
                        get(std::declval<Rng const &>()))
                {
                    return sentinel_box_t::get(range_box_t::get());
                }
                void sentinel() const && = delete;
            };

            template<typename Ref, category Cat>
            struct any_cursor;

            template<typename Rng>
            struct any_view_impl
              : type_erased_impl<any_view_and_sentinel<Rng>, any_view_storage>
            {
                using AVS = any_view_storage;
                using type_erased_impl<any_view_and_sentinel<Rng>, AVS>::get;

                template<typename Ref, category Cat>
                static any_cursor<Ref, Cat> begin_cursor(AVS &self)
                {
                    return any_cursor<Ref, Cat>{get(self).range()};
                }

                static bool at_end(AVS const &self, any_cursor_storage const &cursor)
                {
                    using TEI = type_erased_impl<iterator_t<Rng>, any_cursor_storage>;
                    return TEI::get(cursor) == get(self).sentinel();
                }
            };

            template<typename Ref, category Cat>
            struct any_cursor_vtable; // undefined

            template<typename Ref>
            struct any_cursor_vtable<Ref, category::forward>
            {
                using ACS = any_cursor_storage;

                void (*destroy)(ACS &self) noexcept;
                void (*copy_init)(ACS &self, ACS const &that);
                void (*move_init)(ACS &self, ACS &that) noexcept;
                void (*copy_assign)(ACS &self, ACS const &that);
                void (*move_assign)(ACS &self, ACS &that) noexcept;
                Ref (*read)(ACS const &self);
                void (*next)(ACS &self);
                bool (*equal)(ACS const &self, ACS const &that);

                template<typename I, bool Small = any_cursor_storage::is_small<I>::value>
                constexpr any_cursor_vtable(meta::id<I>) noexcept;

                constexpr any_cursor_vtable() noexcept
                  : destroy{nop_function<ACS &>}
                  , copy_init{nop_function<ACS &, ACS const &>}
                  , move_init{nop_function<ACS &, ACS &>}
                  , copy_assign{nop_function<ACS &, ACS const &>}
                  , move_assign{nop_function<ACS &, ACS &>}
                  , read{uncalled_function<Ref, ACS const &>}
                  , next{nop_function<ACS &>}
                  , equal{const_function<bool, true, ACS const &, ACS const &>}
                {}
            };

            template<typename Ref>
            struct any_cursor_vtable<Ref, category::bidirectional>
              : any_cursor_vtable<Ref, category::forward>
            {
                void (*prev)(any_cursor_storage &self);

                template<typename I>
                constexpr any_cursor_vtable(meta::id<I>) noexcept
                  : any_cursor_vtable<Ref, category::forward>{meta::id<I>{}}
                  , prev{any_cursor_impl<I>::prev}
                {}

                constexpr any_cursor_vtable() noexcept
                  : any_cursor_vtable<Ref, category::forward>{}
                  , prev{nop_function<any_cursor_storage &>}
                {}
            };

            template<typename Ref>
            struct any_cursor_vtable<Ref, category::random_access>
              : any_cursor_vtable<Ref, category::bidirectional>
            {
                using ACS = any_cursor_storage;

                void (*advance)(ACS &self, std::ptrdiff_t);
                std::ptrdiff_t (*distance_to)(ACS const &self, ACS const &that);

                template<typename I>
                constexpr any_cursor_vtable(meta::id<I>) noexcept
                  : any_cursor_vtable<Ref, category::bidirectional>{meta::id<I>{}}
                  , advance{any_cursor_impl<I>::advance}
                  , distance_to{any_cursor_impl<I>::distance_to}
                {}

                constexpr any_cursor_vtable() noexcept
                  : any_cursor_vtable<Ref, category::bidirectional>{}
                  , advance{nop_function<ACS &, std::ptrdiff_t>}
                  , distance_to{const_function<std::ptrdiff_t, 0, ACS const &, ACS const &>}
                {}
            };

            template<typename Ref, typename I>
            struct any_cursor_vtable_for
            {
                static constexpr any_cursor_vtable<Ref, to_cat_(iterator_concept<I>{})> vtable{meta::id<I>{}};
            };

            template<typename Ref, typename I>
            constexpr any_cursor_vtable<Ref, to_cat_(iterator_concept<I>{})> any_cursor_vtable_for<Ref, I>::vtable;

            template<typename Ref>
            struct no_cursor_vtable_for
            {
                static constexpr any_cursor_vtable<Ref, category::random_access> vtable{};
            };

            template<typename Ref>
            constexpr any_cursor_vtable<Ref, category::random_access> no_cursor_vtable_for<Ref>::vtable;

            template<typename Ref>
            template<typename I, bool Small>
            constexpr any_cursor_vtable<Ref, category::forward>::any_cursor_vtable(meta::id<I>) noexcept
              : destroy{Small
                    ? (std::is_trivially_destructible<I>::value
                        ? nop_function<ACS &>
                        : type_erased_impl<I, ACS>::small_destroy)
                    : type_erased_impl<I, ACS>::big_destroy}
              , copy_init{Small
                    ? (is_trivially_copy_constructible<I>::value
                        ? ACS::trivial_copy
                        : type_erased_impl<I, ACS>::small_copy_init)
                    : type_erased_impl<I, ACS>::big_copy_init}
              , move_init{Small
                    ? (is_trivially_move_constructible<I>::value
                        ? ACS::trivial_move
                        : type_erased_impl<I, ACS>::small_move_init)
                    : type_erased_impl<I, ACS>::template nested<any_cursor_vtable<Ref, category::random_access>,
                        &no_cursor_vtable_for<Ref>::vtable>::big_move_init}
              , copy_assign{Small && is_trivially_copy_assignable<I>::value
                    ? ACS::trivial_copy
                    : type_erased_impl<I, ACS>::copy_assign}
              , move_assign{Small
                    ? (is_trivially_move_assignable<I>::value
                        ? ACS::trivial_move
                        : type_erased_impl<I, ACS>::small_move_assign)
                    : type_erased_impl<I, ACS>::big_move_assign}
              , read{any_cursor_impl<I>::template read<Ref>}
              , next{any_cursor_impl<I>::next}
              , equal{any_cursor_impl<I>::equal}
            {}

            struct fully_erased_view_vtable
            {
                void const *cursor_table = nullptr;

                bool (*at_end)(any_view_storage const &self,
                    any_cursor_storage const &cursor);

                template<typename Ref, category Cat, typename Rng>
                constexpr fully_erased_view_vtable(meta::id<Ref>, std::integral_constant<category, Cat>, meta::id<Rng>) noexcept
                  : cursor_table{&any_cursor_vtable_for<Ref, iterator_t<Rng>>::vtable}
                  , at_end{any_view_impl<Rng>::at_end}
                {}

                template<typename Ref, category Cat>
                constexpr fully_erased_view_vtable(meta::id<Ref>, std::integral_constant<category, Cat>) noexcept
                  : cursor_table{&no_cursor_vtable_for<Ref>::vtable}
                  , at_end{} // FIXME
                {}

                constexpr fully_erased_view_vtable() noexcept
                  : at_end{const_function<bool, true, any_view_storage const &,
                        any_cursor_storage const &>}
                {}
            };

            template<typename Ref, category Cat>
            struct any_view_vtable
              : fully_erased_view_vtable
            {
                using AVS = any_view_storage;
                using ACV = any_cursor_vtable<Ref, Cat>;

                void (*destroy)(AVS &self) noexcept;
                void (*copy_init)(AVS &self, AVS const &that);
                void (*move_init)(AVS &self, AVS &that) noexcept;
                void (*copy_assign)(AVS &self, AVS const &that);
                void (*move_assign)(AVS &self, AVS &that) noexcept;

                any_cursor<Ref, Cat> (*begin_cursor)(AVS &self);

                template<typename Rng, typename T = any_view_and_sentinel<Rng>,
                    bool Small = any_view_storage::is_small<T>::value>
                constexpr any_view_vtable(meta::id<Rng>) noexcept;

                constexpr any_view_vtable() noexcept
                  : fully_erased_view_vtable{}
                  , destroy{nop_function<AVS &>}
                  , copy_init{nop_function<AVS &, AVS const &>}
                  , move_init{nop_function<AVS &, AVS &>}
                  , copy_assign{nop_function<AVS &, AVS const &>}
                  , move_assign{nop_function<AVS &, AVS &>}
                  , begin_cursor{uncalled_function<any_cursor<Ref, Cat>, AVS &>}
                {}
            };

            template<typename Ref, category Cat, typename Rng>
            struct any_view_vtable_for
            {
                static constexpr any_view_vtable<Ref, Cat> vtable{meta::id<Rng>{}};
            };

            template<typename Ref, category Cat, typename Rng>
            constexpr any_view_vtable<Ref, Cat> any_view_vtable_for<Ref, Cat, Rng>::vtable;

            template<typename Ref, category Cat>
            struct no_view_vtable_for
            {
                static constexpr any_view_vtable<Ref, Cat> vtable{};
            };

            template<typename Ref, category Cat>
            constexpr any_view_vtable<Ref, Cat> no_view_vtable_for<Ref, Cat>::vtable;

            template<typename Ref, category Cat>
            template<typename Rng, typename T, bool Small>
            constexpr any_view_vtable<Ref, Cat>::any_view_vtable(meta::id<Rng>) noexcept
                : fully_erased_view_vtable{meta::id<Ref>{},
                    std::integral_constant<category, Cat>{}, meta::id<Rng>{}}
                , destroy{Small
                    ? (std::is_trivially_destructible<T>::value
                        ? nop_function<AVS &>
                        : type_erased_impl<T, AVS>::small_destroy)
                    : type_erased_impl<T, AVS>::big_destroy}
                , copy_init{Small
                    ? (is_trivially_copy_constructible<T>::value
                        ? AVS::trivial_copy
                        : type_erased_impl<T, AVS>::small_copy_init)
                    : type_erased_impl<T, AVS>::big_copy_init}
                , move_init{Small
                    ? (is_trivially_move_constructible<T>::value
                        ? AVS::trivial_move
                        : type_erased_impl<T, AVS>::small_move_init)
                    : type_erased_impl<T, AVS>::template nested<any_view_vtable<Ref, Cat>,
                        &no_view_vtable_for<Ref, Cat>::vtable>::big_move_init}
                , copy_assign{Small && is_trivially_copy_assignable<T>::value
                    ? AVS::trivial_copy
                    : type_erased_impl<T, AVS>::copy_assign}
                , move_assign{Small
                    ? (is_trivially_move_assignable<T>::value
                        ? AVS::trivial_move
                        : type_erased_impl<T, AVS>::small_move_assign)
                    : type_erased_impl<T, AVS>::big_move_assign}
                , begin_cursor{any_view_impl<Rng>::template begin_cursor<Ref, Cat>}
            {}

            struct fully_erased_view
            {
                constexpr fully_erased_view(void const *vtable) noexcept
                  : storage_{vtable}
                {}
                template<typename Ref, category Cat>
                constexpr fully_erased_view(meta::id<Ref>,
                    std::integral_constant<category, Cat>) noexcept
                  : storage_{&no_view_vtable_for<Ref, Cat>::vtable}
                {}
                template<typename Ref, category Cat>
                bool at_end(any_cursor<Ref, Cat> const& cursor) const noexcept
                {
                    RANGES_EXPECT(storage_.raw_vtable_);
                    auto vtable = static_cast<fully_erased_view_vtable const *>(storage_.raw_vtable_);
                    RANGES_EXPECT(vtable->cursor_table == cursor.vtable());
                    return vtable->at_end(storage_, cursor.storage_);
                }

                any_view_storage storage_;
            };

            template<typename = void>
            struct no_fully_erased_view
            {
                static constexpr fully_erased_view_vtable vtable{};
                static constexpr fully_erased_view view{&vtable};
            };

            template<typename T>
            constexpr fully_erased_view_vtable no_fully_erased_view<T>::vtable;
            template<typename T>
            constexpr fully_erased_view no_fully_erased_view<T>::view;

            struct any_sentinel
            {
                any_sentinel() = default;
                constexpr explicit any_sentinel(fully_erased_view const &view) noexcept
                  : view_{&view}
                {}
            private:
                template<typename, category> friend struct any_cursor;

                fully_erased_view const *view_ = &no_fully_erased_view<>::view;
            };

            template<typename Ref, category Cat>
            struct any_cursor
            {
                friend any_view<Ref, Cat>;
                friend fully_erased_view;
                CONCEPT_ASSERT(Cat >= category::forward);

                any_cursor() = default;
                template<typename Rng,
                    CONCEPT_REQUIRES_(!Same<detail::decay_t<Rng>, any_cursor>()),
                    CONCEPT_REQUIRES_(ForwardRange<Rng>() &&
                        AnyCompatibleRange<Rng, Ref>())>
                explicit any_cursor(Rng &&rng)
                  : any_cursor{any_cursor_storage::is_small<iterator_t<Rng>>{},
                        static_cast<Rng &&>(rng)}
                {}
                any_cursor(any_cursor &&that) noexcept
                  : storage_{that.storage_.raw_vtable_}
                {
                    vtable()->move_init(storage_, that.storage_);
                }
                any_cursor(any_cursor const &that)
                  : storage_{that.storage_.raw_vtable_}
                {
                    vtable()->copy_init(storage_, that.storage_);
                }
                ~any_cursor()
                {
                    reset();
                }
                any_cursor &operator=(any_cursor &&that) noexcept
                {
                    auto const mine = vtable();
                    auto const theirs = that.vtable();
                    if (mine == theirs)
                        mine->move_assign(storage_, that.storage_);
                    else
                    {
                        reset();
                        theirs->move_init(storage_, that.storage_);
                        storage_.raw_vtable_ = theirs;
                    }
                    return *this;
                }
                any_cursor &operator=(any_cursor const &that)
                {
                    auto const mine = vtable();
                    auto const theirs = that.vtable();
                    if (mine == theirs)
                        mine->copy_assign(storage_, that.storage_);
                    else
                    {
                        reset();
                        theirs->copy_init(storage_, that.storage_);
                        storage_.raw_vtable_ = theirs;
                    }
                    return *this;
                }
                Ref read() const
                {
                    return vtable()->read(storage_);
                }
                bool equal(any_cursor const &that) const
                {
                    RANGES_EXPECT(vtable() == that.vtable());
                    return vtable()->equal(storage_, that.storage_);
                }
                bool equal(any_sentinel const &that) const
                {
                    RANGES_EXPECT(that.view_);
                    return that.view_->at_end(*this);
                }
                void next()
                {
                    vtable()->next(storage_);
                }
                CONCEPT_REQUIRES(Cat >= category::bidirectional)
                void prev()
                {
                    vtable()->prev(storage_);
                }
                CONCEPT_REQUIRES(Cat >= category::random_access)
                void advance(std::ptrdiff_t n)
                {
                    vtable()->advance(storage_, n);
                }
                CONCEPT_REQUIRES(Cat >= category::random_access)
                std::ptrdiff_t distance_to(any_cursor const &that) const
                {
                    RANGES_EXPECT(vtable() == that.vtable());
                    return vtable()->distance_to(storage_, that.storage_);
                }
            private:
                template<typename Rng>
                any_cursor(std::true_type, Rng &&rng)
                {
                    ::new (static_cast<void *>(storage_.space_))
                        iterator_t<Rng>(ranges::begin(rng));

                    using ACVF = any_cursor_vtable_for<Ref, iterator_t<Rng>>;
                    storage_.raw_vtable_ = &ACVF::vtable;
                }
                template<typename Rng>
                any_cursor(std::false_type, Rng &&rng)
                {
                    using IPtr = iterator_t<Rng> *;
                    ::new (static_cast<void *>(storage_.space_))
                        IPtr{new iterator_t<Rng>(ranges::begin(rng))};

                    using ACVF = any_cursor_vtable_for<Ref, iterator_t<Rng>>;
                    storage_.raw_vtable_ = &ACVF::vtable;
                }

                void reset() noexcept
                {
                    vtable()->destroy(storage_);
                    storage_.raw_vtable_ = &no_cursor_vtable_for<Ref>::vtable;
                }

                any_cursor_vtable<Ref, Cat> const *vtable() const noexcept
                {
                    using ACV = any_cursor_vtable<Ref, Cat>;
                    auto table = static_cast<ACV const *>(storage_.raw_vtable_);
                    RANGES_EXPECT(table);
                    return table;
                }

                any_cursor_storage storage_{&no_cursor_vtable_for<Ref>::vtable};
            };
        }
        /// \endcond

        /// \brief A type-erased view
        /// \ingroup group-views
        template<typename Ref, category Cat>
        struct any_view
          : view_facade<any_view<Ref, Cat>, unknown>
          , detail::fully_erased_view
        {
            friend range_access;
            CONCEPT_ASSERT(Cat >= category::forward);

            constexpr any_view() noexcept
              : detail::fully_erased_view(meta::id<Ref>{},
                    std::integral_constant<category, Cat>{})
            {}
            template<typename Rng,
                CONCEPT_REQUIRES_(meta::and_<
                    meta::not_<Same<detail::decay_t<Rng>, any_view>>,
                    InputRange<Rng>,
                    meta::defer<detail::AnyCompatibleRange, Rng, Ref>>::value)>
            any_view(Rng &&rng)
              : any_view(view::all(static_cast<Rng &&>(rng)),
                    meta::bool_<detail::to_cat_(range_concept<Rng>{}) >= Cat>{},
                    detail::any_view_storage::is_small<detail::any_view_and_sentinel<view::all_t<Rng>>>{})
            {}
            any_view(any_view &&that) noexcept
              : detail::fully_erased_view{that.storage_.raw_vtable_}
            {
                vtable()->move_init(this->storage_, that.storage_);
            }
            any_view(any_view const &that) noexcept
              : detail::fully_erased_view{that.storage_.raw_vtable_}
            {
                vtable()->copy_init(this->storage_, that.storage_);
            }
            ~any_view()
            {
                reset();
            }
            any_view &operator=(any_view &&that) noexcept
            {
                auto const mine = vtable();
                auto const theirs = that.vtable();
                if (mine == theirs)
                    mine->move_assign(this->storage_, that.storage_);
                else
                {
                    reset();
                    theirs->move_init(this->storage_, that.storage_);
                    this->storage_.raw_vtable_ = theirs;
                }
                return *this;
            }
            any_view &operator=(any_view const &that) noexcept
            {
                auto const mine = vtable();
                auto const theirs = that.vtable();
                if (mine == theirs)
                    mine->copy_assign(this->storage_, that.storage_);
                else
                {
                    reset();
                    theirs->copy_init(this->storage_, that.storage_);
                    this->storage_.raw_vtable_ = theirs;
                }
                return *this;
            }
        private:
            template<typename View>
            any_view(View view, std::true_type, std::true_type)
              : detail::fully_erased_view(meta::id<Ref>{},
                    std::integral_constant<category, Cat>{})
            {
                ::new (static_cast<void *>(this->storage_.space_))
                    detail::any_view_and_sentinel<View>{std::move(view)};

                using AVVF = detail::any_view_vtable_for<Ref, Cat, View>;
                this->storage_.raw_vtable_ = &AVVF::vtable;
            }
            template<typename View>
            any_view(View view, std::true_type, std::false_type)
              : detail::fully_erased_view(meta::id<Ref>{},
                    std::integral_constant<category, Cat>{})
            {
                using Ptr = detail::any_view_and_sentinel<View> *;
                ::new (static_cast<void *>(this->storage_.space_))
                    Ptr{new detail::any_view_and_sentinel<View>{std::move(view)}};

                using AVVF = detail::any_view_vtable_for<Ref, Cat, View>;
                this->storage_.raw_vtable_ = &AVVF::vtable;
            }
            template<typename View>
            any_view(View, std::false_type, detail::any)
              : detail::fully_erased_view(meta::id<Ref>{},
                    std::integral_constant<category, Cat>{})
            {
                static_assert(detail::to_cat_(range_concept<View>{}) >= Cat,
                    "The range passed to any_view() does not model the requested category");
            }

            void reset() noexcept
            {
                vtable()->destroy(this->storage_);
                this->storage_.raw_vtable_ = &detail::no_view_vtable_for<Ref, Cat>::vtable;
            }

            detail::any_view_vtable<Ref, Cat> const *vtable() const noexcept
            {
                using AVV = detail::any_view_vtable<Ref, Cat>;
                auto table = static_cast<AVV const *>(this->storage_.raw_vtable_);
                RANGES_EXPECT(table);
                return table;
            }

            detail::any_cursor<Ref, Cat> begin_cursor()
            {
                return vtable()->begin_cursor(this->storage_);
            }
            detail::any_sentinel end_cursor() const noexcept
            {
                return detail::any_sentinel{*this};
            }

            bool at_end(detail::any_cursor<Ref, Cat> const &cursor) const
            {
                RANGES_EXPECT(vtable()->cursor_table == cursor.vtable());
                vtable()->at_end(this->storage_, cursor.storage_);
            }
        };

        template<typename Ref>
        struct any_view<Ref, category::input>
          : view_facade<any_view<Ref, category::input>, unknown>
        {
            friend range_access;

            any_view() = default;
            template<typename Rng,
                CONCEPT_REQUIRES_(meta::and_<
                    meta::not_<Same<detail::decay_t<Rng>, any_view>>,
                    InputRange<Rng>,
                    meta::defer<detail::AnyCompatibleRange, Rng, Ref>>::value)>
            any_view(Rng &&rng)
              : ptr_{std::make_shared<impl_t<Rng>>(view::all(static_cast<Rng &&>(rng)))}
            {}
        private:
            template<typename Rng>
            using impl_t = detail::any_input_view_impl<view::all_t<Rng>, Ref>;

            detail::any_input_cursor<Ref> begin_cursor()
            {
                if (!ptr_)
                    return {};

                ptr_->init();
                return detail::any_input_cursor<Ref>{*ptr_};
            }

            std::shared_ptr<detail::any_input_view_interface<Ref>> ptr_;
        };

        template<typename Ref>
        using any_input_view = any_view<Ref, category::input>;

        template<typename Ref>
        using any_forward_view = any_view<Ref, category::forward>;

        template<typename Ref>
        using any_bidirectional_view = any_view<Ref, category::bidirectional>;

        template<typename Ref>
        using any_random_access_view = any_view<Ref, category::random_access>;
    }
}

#endif
