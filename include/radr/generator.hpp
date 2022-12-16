///////////////////////////////////////////////////////////////////////////////
// Reference implementation of std::generator proposal P2168.
//
// See https://wg21.link/P2168 for details.
//
///////////////////////////////////////////////////////////////////////////////
// Copyright Lewis Baker, Corentin Jabot
//
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0.
// (See accompanying file LICENSE or http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////

#pragma once

#if __has_include(<coroutine>)
#    include <coroutine>
#else
// Fallback for older experimental implementations of coroutines.
#    include <experimental/coroutine>
namespace std
{
using std::experimental::coroutine_handle;
using std::experimental::coroutine_traits;
using std::experimental::noop_coroutine;
using std::experimental::suspend_always;
using std::experimental::suspend_never;
} // namespace std
#endif

#include <cassert>
#include <concepts>
#include <exception>
#include <iterator>
#include <new>
#include <ranges>
#include <type_traits>
#include <utility>

namespace radr::detail
{

template <typename T>
class manual_lifetime
{
public:
    manual_lifetime() noexcept {}
    ~manual_lifetime() {}

    template <typename... Args>
    T & construct(Args &&... __args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        return *::new (static_cast<void *>(std::addressof(__value_))) T((Args &&)__args...);
    }

    void destruct() noexcept(std::is_nothrow_destructible_v<T>) { __value_.~T(); }

    T &        get() & noexcept { return __value_; }
    T &&       get() && noexcept { return static_cast<T &&>(__value_); }
    T const &  get() const & noexcept { return __value_; }
    T const && get() const && noexcept { return static_cast<T const &&>(__value_); }

private:
    union
    {
        std::remove_const_t<T> __value_;
    };
};

template <typename T>
class manual_lifetime<T &>
{
public:
    manual_lifetime() noexcept : __value_(nullptr) {}
    ~manual_lifetime() {}

    T & construct(T & __value) noexcept
    {
        __value_ = std::addressof(__value);
        return __value;
    }

    void destruct() noexcept {}

    T & get() const noexcept { return *__value_; }

private:
    T * __value_;
};

template <typename T>
class manual_lifetime<T &&>
{
public:
    manual_lifetime() noexcept : __value_(nullptr) {}
    ~manual_lifetime() {}

    T && construct(T && __value) noexcept
    {
        __value_ = std::addressof(__value);
        return static_cast<T &&>(__value);
    }

    void destruct() noexcept {}

    T && get() const noexcept { return static_cast<T &&>(*__value_); }

private:
    T * __value_;
};

struct use_allocator_arg
{};

template <typename Alloc>
static constexpr bool __allocator_needs_to_be_stored =
  !std::allocator_traits<Alloc>::is_always_equal::value || !std::is_default_constructible_v<Alloc>;

// Round s up to next multiple of a.
constexpr std::size_t __aligned_allocation_size(std::size_t s, std::size_t a)
{
    return (s + a - 1) & ~(a - 1);
}

} // namespace radr::detail

namespace radr
{

template <typename Rng, typename Allocator = detail::use_allocator_arg>
struct elements_of
{
    explicit constexpr elements_of(Rng && __rng) noexcept
        requires std::is_default_constructible_v<Allocator>
      : __range(static_cast<Rng &&>(__rng))
    {}

    constexpr elements_of(Rng && __rng, Allocator && __alloc) noexcept :
      __range((Rng &&)__rng), __alloc((Allocator &&)__alloc)
    {}

    constexpr elements_of(elements_of &&) noexcept = default;

    constexpr elements_of(elements_of const &)             = delete;
    constexpr elements_of & operator=(elements_of const &) = delete;
    constexpr elements_of & operator=(elements_of &&)      = delete;

    constexpr Rng && get() noexcept { return static_cast<Rng &&>(__range); }

    constexpr Allocator get_allocator() const noexcept { return __alloc; }

private:
    [[no_unique_address]] Allocator __alloc; // \expos
    Rng &&                          __range; // \expos
};

template <typename Rng>
elements_of(Rng &&) -> elements_of<Rng>;

template <typename Rng, typename Allocator>
elements_of(Rng &&, Allocator &&) -> elements_of<Rng, Allocator>;

template <typename Ref, typename Value = std::remove_cvref_t<Ref>, typename Allocator = detail::use_allocator_arg>
class generator;

} // namespace radr

namespace radr::detail
{

template <typename Alloc>
class __promise_base_alloc
{
    static constexpr std::size_t __offset_of_allocator(std::size_t __frameSize) noexcept
    {
        return __aligned_allocation_size(__frameSize, alignof(Alloc));
    }

    static constexpr std::size_t __padded_frame_size(std::size_t __frameSize) noexcept
    {
        return __offset_of_allocator(__frameSize) + sizeof(Alloc);
    }

    static Alloc & __get_allocator(void * __frame, std::size_t __frameSize) noexcept
    {
        return *reinterpret_cast<Alloc *>(static_cast<char *>(__frame) + __offset_of_allocator(__frameSize));
    }

public:
    template <typename... Args>
    static void * operator new(std::size_t __frameSize, std::allocator_arg_t, Alloc __alloc, Args &...)
    {
        void * __frame = __alloc.allocate(__padded_frame_size(__frameSize));

        // Store allocator at end of the coroutine frame.
        // Assuming the allocator's move constructor is non-throwing (a requirement for allocators)
        ::new (static_cast<void *>(std::addressof(__get_allocator(__frame, __frameSize)))) Alloc(std::move(__alloc));

        return __frame;
    }

    template <typename This, typename... Args>
    static void * operator new(std::size_t __frameSize, This &, std::allocator_arg_t, Alloc __alloc, Args &...)
    {
        return __promise_base_alloc::operator new(__frameSize, std::allocator_arg, std::move(__alloc));
    }

    static void operator delete(void * __ptr, std::size_t __frameSize) noexcept
    {
        Alloc & __alloc = __get_allocator(__ptr, __frameSize);
        Alloc   __localAlloc(std::move(__alloc));
        __alloc.~Alloc();
        __localAlloc.deallocate(static_cast<std::byte *>(__ptr), __padded_frame_size(__frameSize));
    }
};

template <typename Alloc>
    requires(!__allocator_needs_to_be_stored<Alloc>)
class __promise_base_alloc<Alloc>
{
public:
    static void * operator new(std::size_t __size)
    {
        Alloc __alloc;
        return __alloc.allocate(__size);
    }

    static void operator delete(void * __ptr, std::size_t __size) noexcept
    {
        Alloc __alloc;
        __alloc.deallocate(static_cast<std::byte *>(__ptr), __size);
    }
};

template <typename Ref>
struct __generator_promise_base
{
    template <typename Ref2, typename Value, typename Alloc>
    friend class radr::generator;

    __generator_promise_base *                  __root_;
    std::coroutine_handle<>                     __parentOrLeaf_;
    // Note: Using manual_lifetime here to avoid extra calls to exception_ptr
    // constructor/destructor in cases where it is not needed (i.e. where this
    // generator coroutine is not used as a nested coroutine).
    // This member is lazily constructed by the __yield_sequence_awaiter::await_suspend()
    // method if this generator is used as a nested generator.
    detail::manual_lifetime<std::exception_ptr> __exception_;
    detail::manual_lifetime<Ref>                __value_;

    explicit __generator_promise_base(std::coroutine_handle<> thisCoro) noexcept :
      __root_(this), __parentOrLeaf_(thisCoro)
    {}

    ~__generator_promise_base()
    {
        if (__root_ != this)
        {
            // This coroutine was used as a nested generator and so will
            // have constructed its __exception_ member which needs to be
            // destroyed here.
            __exception_.destruct();
        }
    }

    std::suspend_always initial_suspend() noexcept { return {}; }

    void return_void() noexcept {}

    void unhandled_exception()
    {
        if (__root_ != this)
        {
            __exception_.get() = std::current_exception();
        }
        else
        {
            throw;
        }
    }

    // Transfers control back to the parent of a nested coroutine
    struct __final_awaiter
    {
        bool await_ready() noexcept { return false; }

        template <typename Promise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> __h) noexcept
        {
            Promise &                  __promise = __h.promise();
            __generator_promise_base & __root    = *__promise.__root_;
            if (&__root != &__promise)
            {
                auto __parent          = __promise.__parentOrLeaf_;
                __root.__parentOrLeaf_ = __parent;
                return __parent;
            }
            return std::noop_coroutine();
        }

        void await_resume() noexcept {}
    };

    __final_awaiter final_suspend() noexcept { return {}; }

    std::suspend_always yield_value(Ref && __x) noexcept(std::is_nothrow_move_constructible_v<Ref>)
    {
        __root_->__value_.construct((Ref &&)__x);
        return {};
    }

    template <typename T>
        requires(!std::is_reference_v<Ref>) && std::is_convertible_v<T, Ref>
    std::suspend_always yield_value(T && __x) noexcept(std::is_nothrow_constructible_v<Ref, T>)
    {
        __root_->__value_.construct((T &&)__x);
        return {};
    }

    template <typename Gen>
    struct __yield_sequence_awaiter
    {
        Gen __gen_;

        __yield_sequence_awaiter(Gen && __g) noexcept
          // Taking ownership of the generator ensures frame are destroyed
          // in the reverse order of their execution.
          :
          __gen_((Gen &&)__g)
        {}

        bool await_ready() noexcept { return false; }

        // set the parent, root and exceptions pointer and
        // resume the nested
        template <typename Promise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> __h) noexcept
        {
            __generator_promise_base & __current = __h.promise();
            __generator_promise_base & __nested  = *__gen_.__get_promise();
            __generator_promise_base & __root    = *__current.__root_;

            __nested.__root_         = __current.__root_;
            __nested.__parentOrLeaf_ = __h;

            // Lazily construct the __exception_ member here now that we
            // know it will be used as a nested generator. This will be
            // destroyed by the promise destructor.
            __nested.__exception_.construct();
            __root.__parentOrLeaf_ = __gen_.__get_coro();

            // Immediately resume the nested coroutine (nested generator)
            return __gen_.__get_coro();
        }

        void await_resume()
        {
            __generator_promise_base & __nestedPromise = *__gen_.__get_promise();
            if (__nestedPromise.__exception_.get())
            {
                std::rethrow_exception(std::move(__nestedPromise.__exception_.get()));
            }
        }
    };

    template <typename OValue, typename OAlloc>
    __yield_sequence_awaiter<generator<Ref, OValue, OAlloc>> yield_value(
      radr::elements_of<generator<Ref, OValue, OAlloc>> __g) noexcept
    {
        return std::move(__g).get();
    }

    template <std::ranges::range Rng, typename Allocator>
    __yield_sequence_awaiter<generator<Ref, std::remove_cvref_t<Ref>, Allocator>> yield_value(
      radr::elements_of<Rng, Allocator> && __x)
    {
        return [](std::allocator_arg_t,
                  Allocator alloc,
                  auto &&   __rng) -> generator<Ref, std::remove_cvref_t<Ref>, Allocator>
        {
            for (auto && e : __rng)
                co_yield static_cast<decltype(e)>(e);
        }(std::allocator_arg, __x.get_allocator(), std::forward<Rng>(__x.get()));
    }

    void resume() { __parentOrLeaf_.resume(); }

    // Disable use of co_await within this coroutine.
    void await_transform() = delete;
};

template <typename Generator, typename ByteAllocator, bool ExplicitAllocator = false>
struct __generator_promise;

template <typename Ref, typename Value, typename Alloc, typename ByteAllocator, bool ExplicitAllocator>
struct __generator_promise<generator<Ref, Value, Alloc>, ByteAllocator, ExplicitAllocator> final :
  public __generator_promise_base<Ref>,
  public __promise_base_alloc<ByteAllocator>
{
    __generator_promise() noexcept :
      __generator_promise_base<Ref>(std::coroutine_handle<__generator_promise>::from_promise(*this))
    {}

    generator<Ref, Value, Alloc> get_return_object() noexcept
    {
        return generator<Ref, Value, Alloc>{std::coroutine_handle<__generator_promise>::from_promise(*this)};
    }

    using __generator_promise_base<Ref>::yield_value;

    template <std::ranges::range Rng>
    typename __generator_promise_base<Ref>::template __yield_sequence_awaiter<generator<Ref, Value, Alloc>> yield_value(
      radr::elements_of<Rng> && __x)
    {
        static_assert(!ExplicitAllocator,
                      "This coroutine has an explicit allocator specified with std::allocator_arg so an allocator "
                      "needs to be passed "
                      "explicitely to std::elements_of");
        return [](auto && __rng) -> generator<Ref, Value, Alloc>
        {
            for (auto && e : __rng)
                co_yield static_cast<decltype(e)>(e);
        }(std::forward<Rng>(__x.get()));
    }
};

template <typename Alloc>
using __byte_allocator_t = typename std::allocator_traits<std::remove_cvref_t<Alloc>>::template rebind_alloc<std::byte>;

} // namespace radr::detail

namespace std
{

// Type-erased allocator with default allocator behaviour.
template <typename Ref, typename Value, typename... Args>
struct coroutine_traits<radr::generator<Ref, Value>, Args...>
{
    using promise_type = radr::detail::__generator_promise<radr::generator<Ref, Value>, std::allocator<std::byte>>;
};

// Type-erased allocator with std::allocator_arg parameter
template <typename Ref, typename Value, typename Alloc, typename... Args>
struct coroutine_traits<radr::generator<Ref, Value>, allocator_arg_t, Alloc, Args...>
{
private:
    using __byte_allocator = radr::detail::__byte_allocator_t<Alloc>;

public:
    using promise_type =
      radr::detail::__generator_promise<radr::generator<Ref, Value>, __byte_allocator, true /*explicit Allocator*/>;
};

// Type-erased allocator with std::allocator_arg parameter (non-static member functions)
template <typename Ref, typename Value, typename This, typename Alloc, typename... Args>
struct coroutine_traits<radr::generator<Ref, Value>, This, allocator_arg_t, Alloc, Args...>
{
private:
    using __byte_allocator = radr::detail::__byte_allocator_t<Alloc>;

public:
    using promise_type =
      radr::detail::__generator_promise<radr::generator<Ref, Value>, __byte_allocator, true /*explicit Allocator*/>;
};

// Generator with specified allocator type
template <typename Ref, typename Value, typename Alloc, typename... Args>
struct coroutine_traits<radr::generator<Ref, Value, Alloc>, Args...>
{
    using __byte_allocator = radr::detail::__byte_allocator_t<Alloc>;

public:
    using promise_type = radr::detail::__generator_promise<radr::generator<Ref, Value, Alloc>, __byte_allocator>;
};

} // namespace std

namespace radr
{

// TODO :  make layout compatible promise casts possible
template <typename Ref, typename Value, typename Alloc>
class generator
{
    using __byte_allocator = detail::__byte_allocator_t<Alloc>;

public:
    using promise_type = detail::__generator_promise<generator<Ref, Value, Alloc>, __byte_allocator>;
    friend promise_type;

private:
    using __coroutine_handle = std::coroutine_handle<promise_type>;

public:
    generator() noexcept = default;

    generator(generator && __other) noexcept :
      __coro_(std::exchange(__other.__coro_, {})), __started_(std::exchange(__other.__started_, false))
    {}

    ~generator() noexcept
    {
        if (__coro_)
        {
            if (__started_ && !__coro_.done())
            {
                __coro_.promise().__value_.destruct();
            }
            __coro_.destroy();
        }
    }

    generator & operator=(generator && g) noexcept
    {
        swap(g);
        return *this;
    }

    void swap(generator & __other) noexcept
    {
        std::swap(__coro_, __other.__coro_);
        std::swap(__started_, __other.__started_);
    }

    struct sentinel
    {};

    class iterator
    {
    public:
        using iterator_category = std::input_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = Value;
        using reference         = Ref;
        using pointer           = std::add_pointer_t<Ref>;

        iterator() noexcept        = default;
        iterator(iterator const &) = delete;

        iterator(iterator && __other) noexcept : __coro_(std::exchange(__other.__coro_, {})) {}

        iterator & operator=(iterator && __other)
        {
            std::swap(__coro_, __other.__coro_);
            return *this;
        }

        ~iterator() {}

        friend bool operator==(iterator const & it, sentinel) noexcept { return it.__coro_.done(); }

        iterator & operator++()
        {
            __coro_.promise().__value_.destruct();
            __coro_.promise().resume();
            return *this;
        }
        void operator++(int) { (void)operator++(); }

        reference operator*() const noexcept { return static_cast<reference>(__coro_.promise().__value_.get()); }

    private:
        friend generator;

        explicit iterator(__coroutine_handle __coro) noexcept : __coro_(__coro) {}

        __coroutine_handle __coro_;
    };

    iterator begin()
    {
        assert(__coro_);
        assert(!__started_);
        __started_ = true;
        __coro_.resume();
        return iterator{__coro_};
    }

    sentinel end() noexcept { return {}; }

private:
    explicit generator(__coroutine_handle __coro) noexcept : __coro_(__coro) {}

public: // to get around access restrictions for __yield_sequence_awaitable
    std::coroutine_handle<> __get_coro() noexcept { return __coro_; }
    promise_type *          __get_promise() noexcept { return std::addressof(__coro_.promise()); }

private:
    __coroutine_handle __coro_;
    bool               __started_ = false;
};

// Specialisation for type-erased allocator implementation.
template <typename Ref, typename Value>
class generator<Ref, Value, detail::use_allocator_arg>
{
    using __promise_base = detail::__generator_promise_base<Ref>;

public:
    generator() noexcept : __promise_(nullptr), __coro_(), __started_(false) {}

    generator(generator && __other) noexcept :
      __promise_(std::exchange(__other.__promise_, nullptr)),
      __coro_(std::exchange(__other.__coro_, {})),
      __started_(std::exchange(__other.__started_, false))
    {}

    ~generator() noexcept
    {
        if (__coro_)
        {
            if (__started_ && !__coro_.done())
            {
                __promise_->__value_.destruct();
            }
            __coro_.destroy();
        }
    }

    generator & operator=(generator g) noexcept
    {
        swap(g);
        return *this;
    }

    void swap(generator & __other) noexcept
    {
        std::swap(__promise_, __other.__promise_);
        std::swap(__coro_, __other.__coro_);
        std::swap(__started_, __other.__started_);
    }

    struct sentinel
    {};

    class iterator
    {
    public:
        using iterator_category = std::input_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = Value;
        using reference         = Ref;
        using pointer           = std::add_pointer_t<Ref>;

        iterator() noexcept        = default;
        iterator(iterator const &) = delete;

        iterator(iterator && __other) noexcept :
          __promise_(std::exchange(__other.__promise_, nullptr)), __coro_(std::exchange(__other.__coro_, {}))
        {}

        iterator & operator=(iterator && __other)
        {
            __promise_ = std::exchange(__other.__promise_, nullptr);
            __coro_    = std::exchange(__other.__coro_, {});
            return *this;
        }

        ~iterator() = default;

        friend bool operator==(iterator const & it, sentinel) noexcept { return it.__coro_.done(); }

        iterator & operator++()
        {
            __promise_->__value_.destruct();
            __promise_->resume();
            return *this;
        }

        void operator++(int) { (void)operator++(); }

        reference operator*() const noexcept { return static_cast<reference>(__promise_->__value_.get()); }

    private:
        friend generator;

        explicit iterator(__promise_base * __promise, std::coroutine_handle<> __coro) noexcept :
          __promise_(__promise), __coro_(__coro)
        {}

        __promise_base *        __promise_;
        std::coroutine_handle<> __coro_;
    };

    iterator begin()
    {
        assert(__coro_);
        assert(!__started_);
        __started_ = true;
        __coro_.resume();
        return iterator{__promise_, __coro_};
    }

    sentinel end() noexcept { return {}; }

private:
    template <typename Generator, typename ByteAllocator, bool ExplicitAllocator>
    friend struct detail::__generator_promise;

    template <typename Promise>
    explicit generator(std::coroutine_handle<Promise> __coro) noexcept :
      __promise_(std::addressof(__coro.promise())), __coro_(__coro)
    {}

public: // to get around access restrictions for __yield_sequence_awaitable
    std::coroutine_handle<> __get_coro() noexcept { return __coro_; }
    __promise_base *        __get_promise() noexcept { return __promise_; }

private:
    __promise_base *        __promise_;
    std::coroutine_handle<> __coro_;
    bool                    __started_ = false;
};

} // namespace radr

namespace std::ranges
{

template <typename T, typename U, typename Alloc>
inline constexpr bool enable_view<radr::generator<T, U, Alloc>> = true;

} // namespace std::ranges
