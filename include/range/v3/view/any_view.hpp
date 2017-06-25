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
    #include <range/v3/detail/satisfy_boost_range.hpp>
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

            /// \cond
            namespace detail
            {
                // workaround the fact that typeid ignores cv-qualifiers
                template<typename> struct rtti_tag {};

                struct any_ref
                {
                    any_ref() = default;
                    template<class T>
                    constexpr any_ref(T &obj) noexcept
                      : obj_{std::addressof(obj)}
    #ifndef NDEBUG
                      , info_{&typeid(rtti_tag<T>)}
    #endif
                    {}
                    template<class T>
                    T &get() const noexcept
                    {
                        RANGES_ASSERT(obj_ && info_ && *info_ == typeid(rtti_tag<T>));
                        return *const_cast<T *>(static_cast<T const volatile *>(obj_));
                    }
                private:
                    void const volatile *obj_ = nullptr;
    #ifndef NDEBUG
                    std::type_info const *info_ = nullptr;
    #endif
                };

                template<typename Base>
                struct cloneable
                  : Base
                {
                    using Base::Base;
                    virtual ~cloneable() = default;
                    cloneable() = default;
                    cloneable(cloneable const &) = delete;
                    cloneable &operator=(cloneable const &) = delete;
                    virtual std::unique_ptr<cloneable> clone() const = 0;
                };

                constexpr category to_cat_(concepts::InputRange *) { return category::input; }
                constexpr category to_cat_(concepts::ForwardRange *) { return category::forward; }
                constexpr category to_cat_(concepts::BidirectionalRange *) { return category::bidirectional; }
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

                constexpr size_t any_cursor_pointer_size = 4;
                struct any_cursor_storage
                {
                    alignas(std::max_align_t) char space_[
                        (any_cursor_pointer_size - 1) * sizeof(void *)];
                    void const *raw_vtable_;

                    explicit constexpr any_cursor_storage(void const *vtable) noexcept
                      : space_{}, raw_vtable_{vtable}
                    {}
                };

                template<typename T>
                using any_cursor_is_small = meta::bool_<
                    sizeof(T) <= sizeof(any_cursor_storage::space_) &&
                    alignof(T) <= alignof(std::max_align_t) &&
                    std::is_nothrow_move_constructible<T>::value &&
                    std::is_nothrow_move_assignable<T>::value>;

                struct fully_erased_view
                {
                    // the argument is a const ref to a wrapped iterator
                    // to be compared to the erased view's end sentinel
                    virtual bool at_end(any_ref) const = 0;

                protected:
                    ~fully_erased_view() = default;
                };

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

                inline void trivial_cursor_copy(
                    any_cursor_storage &self, any_cursor_storage const &that) noexcept
                {
                    std::memcpy(self.space_, that.space_, sizeof(self.space_));
                }
                inline void trivial_cursor_move(
                    any_cursor_storage &self, any_cursor_storage &that) noexcept
                {
                    trivial_cursor_copy(self, that);
                }

                template<typename Ref, category = category::forward>
                struct any_cursor_vtable;

                template<typename Ref>
                any_cursor_vtable<Ref, category::random_access> const *
                not_a_cursor_table() noexcept;

                template<typename I>
                struct any_cursor_impl
                {
                    using ACS = any_cursor_storage;

                    static I *&big_pointer(ACS &self) noexcept
                    {
                        return reinterpret_cast<I *&>(self.space_[0]);
                    }
                    static I const &get(ACS const &self) noexcept
                    {
                        if /* constexpr */ (any_cursor_is_small<I>())
                            return reinterpret_cast<I const &>(self.space_[0]);
                        else
                            return *reinterpret_cast<I const * const &>(self.space_[0]);
                    }
                    static I &get(ACS &self) noexcept
                    {
                        return const_cast<I &>(get(as_const(self)));
                    }

                    static void small_destroy(ACS &self) noexcept
                    {
                        get(self).~I();
                    }
                    static void small_copy_init(ACS &self, ACS const &that)
                    {
                        ::new (static_cast<void *>(self.space_)) I(get(that));
                    }
                    static void small_move_init(ACS &self, ACS &that) noexcept
                    {
                        ::new (static_cast<void *>(self.space_)) I(std::move(get(that)));
                    }
                    static void small_move_assign(ACS &self, ACS &that) noexcept
                    {
                        get(self) = std::move(get(that));
                    }

                    static void big_destroy(ACS &self) noexcept
                    {
                        delete big_pointer(self);
                    }
                    static void big_copy_init(ACS &self, ACS const &that)
                    {
                        using IPtr = I *;
                        auto target = std::addressof(big_pointer(self));
                        ::new (static_cast<void *>(target)) IPtr{new I(get(that))};
                    }
                    template<typename Ref>
                    static void big_move_init(ACS &self, ACS &that) noexcept
                    {
                        using IPtr = I *;
                        auto target = std::addressof(big_pointer(self));
                        ::new (static_cast<void *>(target)) IPtr{big_pointer(that)};
                        that.raw_vtable_ = not_a_cursor_table<Ref>();
                    }
                    static void big_move_assign(ACS &self, ACS &that) noexcept
                    {
                        // Meh, move assign implemented with swap
                        ranges::swap(big_pointer(self), big_pointer(that));
                    }

                    static void copy_assign(ACS &self, ACS const &that)
                    {
                        get(self) = get(that);
                    }
                    static any_ref iter(ACS const &self)
                    {
                        return get(self);
                    }
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
                    static bool at_end(ACS const &self, fully_erased_view const *view)
                    {
                        return view->at_end(get(self));
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

                template<typename Ref>
                struct any_cursor_vtable<Ref, category::forward>
                {
                    using ACS = any_cursor_storage;

                    void (*destroy)(ACS &self) noexcept;
                    void (*copy_init)(ACS &self, ACS const &that);
                    void (*move_init)(ACS &self, ACS &that) noexcept;
                    void (*copy_assign)(ACS &self, ACS const &that);
                    void (*move_assign)(ACS &self, ACS &that) noexcept;
                    any_ref (*iter)(ACS const &self);
                    Ref (*read)(ACS const &self);
                    void (*next)(ACS &self);
                    bool (*equal)(ACS const &self, ACS const &that);
                    bool (*at_end)(ACS const &self, fully_erased_view const *view);

                    template<typename I, bool Small = any_cursor_is_small<I>::value>
                    constexpr any_cursor_vtable(meta::id<I>) noexcept
                      : destroy{Small
                            ? (std::is_trivially_destructible<I>::value
                                ? nop_function<ACS &>
                                : any_cursor_impl<I>::small_destroy)
                            : any_cursor_impl<I>::big_destroy}
                      , copy_init{Small
                            ? (is_trivially_copy_constructible<I>::value
                                ? trivial_cursor_copy
                                : any_cursor_impl<I>::small_copy_init)
                            : any_cursor_impl<I>::big_copy_init}
                      , move_init{Small
                            ? (is_trivially_move_constructible<I>::value
                                ? trivial_cursor_move
                                : any_cursor_impl<I>::small_move_init)
                            : any_cursor_impl<I>::template big_move_init<Ref>}
                      , copy_assign{Small && is_trivially_copy_assignable<I>::value
                            ? trivial_cursor_copy
                            : any_cursor_impl<I>::copy_assign}
                      , move_assign{Small
                            ? (is_trivially_move_assignable<I>::value
                                ? trivial_cursor_move
                                : any_cursor_impl<I>::small_move_assign)
                            : any_cursor_impl<I>::big_move_assign}
                      , iter{any_cursor_impl<I>::iter}
                      , read{any_cursor_impl<I>::template read<Ref>}
                      , next{any_cursor_impl<I>::next}
                      , equal{any_cursor_impl<I>::equal}
                      , at_end{any_cursor_impl<I>::at_end}
                    {}

                    constexpr any_cursor_vtable() noexcept
                      : destroy{nop_function<ACS &>}
                      , copy_init{nop_function<ACS &, ACS const &>}
                      , move_init{nop_function<ACS &, ACS &>}
                      , copy_assign{nop_function<ACS &, ACS const &>}
                      , move_assign{nop_function<ACS &, ACS &>}
                      , iter{uncalled_function<any_ref, ACS const &>}
                      , read{uncalled_function<Ref, ACS const &>}
                      , next{nop_function<ACS &>}
                      , equal{const_function<bool, true, ACS const &, ACS const &>}
                      , at_end{const_function<bool, true, ACS const &, fully_erased_view const *>}
                    {}

                    template<typename I>
                    static any_cursor_vtable const *table() noexcept
                    {
                        static constexpr any_cursor_vtable t{meta::id<I>{}};
                        return std::addressof(t);
                    }
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

                    template<typename I>
                    static any_cursor_vtable const *table()
                    {
                        static constexpr any_cursor_vtable t{meta::id<I>{}};
                        return std::addressof(t);
                    }
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

                    template<typename I>
                    static any_cursor_vtable const *table()
                    {
                        static constexpr any_cursor_vtable t{meta::id<I>{}};
                        return std::addressof(t);
                    }
                };

                template<typename Ref>
                any_cursor_vtable<Ref, category::random_access> const *
                not_a_cursor_table() noexcept
                {
                    static constexpr any_cursor_vtable<Ref, category::random_access> t{};
                    return std::addressof(t);
                }

                struct any_sentinel
                {
                    any_sentinel() = default;
                    constexpr explicit any_sentinel(fully_erased_view const &view) noexcept
                      : view_{&view}
                    {}
                private:
                    template<typename, category> friend struct any_cursor;

                    fully_erased_view const *view_ = nullptr;
                };

                template<typename Ref, category Cat>
                struct any_cursor
                {
                    CONCEPT_ASSERT(Cat >= category::forward);

                    any_cursor() = default;
                    template<typename Rng,
                        CONCEPT_REQUIRES_(!Same<detail::decay_t<Rng>, any_cursor>()),
                        CONCEPT_REQUIRES_(ForwardRange<Rng>() &&
                                        AnyCompatibleRange<Rng, Ref>())>
                    explicit any_cursor(Rng &&rng)
                      : any_cursor{any_cursor_is_small<iterator_t<Rng>>{},
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
                        RANGES_ASSERT(!that.view_ == (vtable() == not_a_cursor_table<Ref>()));
                        return !that.view_ || that.view_->at_end(vtable()->iter(storage_));
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
                        using ACV = any_cursor_vtable<Ref, Cat>;
                        storage_.raw_vtable_ = ACV::template table<iterator_t<Rng>>();
                    }
                    template<typename Rng>
                    any_cursor(std::false_type, Rng &&rng)
                    {
                        using IPtr = iterator_t<Rng> *;
                        ::new (static_cast<void *>(storage_.space_))
                            IPtr{new iterator_t<Rng>(ranges::begin(rng))};
                        using ACV = any_cursor_vtable<Ref, Cat>;
                        storage_.raw_vtable_ = ACV::template table<iterator_t<Rng>>();
                    }

                    void reset() noexcept
                    {
                        vtable()->destroy(storage_);
                        storage_.raw_vtable_ = not_a_cursor_table<Ref>();
                    }

                    any_cursor_vtable<Ref, Cat> const *vtable() const noexcept
                    {
                        using ACV = any_cursor_vtable<Ref, Cat>;
                        auto tmp = static_cast<ACV const *>(storage_.raw_vtable_);
                        RANGES_EXPECT(tmp);
                        return tmp;
                    }

                    any_cursor_storage storage_{not_a_cursor_table<Ref>()};
                };

                template<typename Ref, category Cat>
                struct any_view_interface
                  : fully_erased_view
                {
                    CONCEPT_ASSERT(Cat >= category::forward);

                    virtual ~any_view_interface() = default;
                    virtual any_cursor<Ref, Cat> begin_cursor() = 0;
                };

                template<typename Ref, category Cat>
                using any_cloneable_view_interface =
                    cloneable<any_view_interface<Ref, Cat>>;

                template<typename Rng, typename Ref, category Cat>
                struct any_view_impl
                  : any_cloneable_view_interface<Ref, Cat>
                  , private box<Rng, any_view_impl<Rng, Ref, Cat>>
                  , private any_view_sentinel_impl<Rng>
                {
                    CONCEPT_ASSERT(Cat >= category::forward);
                    CONCEPT_ASSERT(AnyCompatibleRange<Rng, Ref>());

                    any_view_impl() = default;
                    any_view_impl(Rng rng)
                      : range_box_t{std::move(rng)}
                      , sentinel_box_t{range_box_t::get()} // NB: initialization order dependence
                    {}
                private:
                    using range_box_t = box<Rng, any_view_impl>;
                    using sentinel_box_t = any_view_sentinel_impl<Rng>;

                    any_cursor<Ref, Cat> begin_cursor() override
                    {
                        return any_cursor<Ref, Cat>{range_box_t::get()};
                    }
                    bool at_end(any_ref it_) const override
                    {
                        auto &it = it_.get<iterator_t<Rng> const>();
                        return it == sentinel_box_t::get(range_box_t::get());
                    }
                    std::unique_ptr<any_cloneable_view_interface<Ref, Cat>> clone() const override
                    {
                        return detail::make_unique<any_view_impl>(range_box_t::get());
                    }
                };
            }
            /// \endcond

            /// \brief A type-erased view
            /// \ingroup group-views
            template<typename Ref, category Cat = category::input>
            struct any_view
              : view_facade<any_view<Ref, Cat>, unknown>
            {
                friend range_access;
                CONCEPT_ASSERT(Cat >= category::forward);

                any_view() = default;
                template<typename Rng,
                    CONCEPT_REQUIRES_(meta::and_<
                        meta::not_<Same<detail::decay_t<Rng>, any_view>>,
                        InputRange<Rng>,
                        meta::defer<detail::AnyCompatibleRange, Rng, Ref>>::value)>
                any_view(Rng &&rng)
                  : any_view(static_cast<Rng &&>(rng),
                    meta::bool_<detail::to_cat_(range_concept<Rng>{}) >= Cat>{})
                {}
                any_view(any_view &&) = default;
                any_view(any_view const &that)
                  : ptr_{that.ptr_ ? that.ptr_->clone() : nullptr}
                {}
                any_view &operator=(any_view &&) = default;
                any_view &operator=(any_view const &that)
                {
                    ptr_ = (that.ptr_ ? that.ptr_->clone() : nullptr);
                    return *this;
                }
            private:
                template<typename Rng>
                using impl_t = detail::any_view_impl<view::all_t<Rng>, Ref, Cat>;
                template<typename Rng>
                any_view(Rng &&rng, std::true_type)
                  : ptr_{detail::make_unique<impl_t<Rng>>(view::all(static_cast<Rng &&>(rng)))}
                {}
                template<typename Rng>
                any_view(Rng &&, std::false_type)
                {
                    static_assert(detail::to_cat_(range_concept<Rng>{}) >= Cat,
                        "The range passed to any_view() does not model the requested category");
                }

                detail::any_cursor<Ref, Cat> begin_cursor()
                {
                    return ptr_ ? ptr_->begin_cursor() : detail::value_init{};
                }
                detail::any_sentinel end_cursor() const noexcept
                {
                    return detail::any_sentinel{*ptr_};
                }

                std::unique_ptr<detail::any_cloneable_view_interface<Ref, Cat>> ptr_;
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

    RANGES_SATISFY_BOOST_RANGE(::ranges::v3::any_view)

    #endif
