#pragma once

#include <version>

#ifdef __cpp_lib_generator

#    include <generator>

namespace radr
{

using std::generator;

using std::ranges::elements_of;

} // namespace radr

#else

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

#    if __has_include(<coroutine>)
#        include <coroutine>
#    else
// Fallback for older experimental implementations of coroutines.
#        include <experimental/coroutine>
namespace std
{
using std::experimental::coroutine_handle;
using std::experimental::coroutine_traits;
using std::experimental::noop_coroutine;
using std::experimental::suspend_always;
using std::experimental::suspend_never;
} // namespace std
#    endif

#    include <cassert>
#    include <exception>
#    include <iterator>
#    include <memory>
#    include <ranges>
#    include <type_traits>
#    include <utility>

namespace radr::detail
{

template <typename T>
class manual_lifetime
{
public:
    manual_lifetime() noexcept {}
    ~manual_lifetime() {}

    template <typename... Args>
    T & construct(Args &&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        return *::new (static_cast<void *>(std::addressof(value_))) T((Args &&)args...);
    }

    void destruct() noexcept(std::is_nothrow_destructible_v<T>) { value_.~T(); }

    T &        get() & noexcept { return value_; }
    T &&       get() && noexcept { return static_cast<T &&>(value_); }
    T const &  get() const & noexcept { return value_; }
    T const && get() const && noexcept { return static_cast<T const &&>(value_); }

private:
    union
    {
        std::remove_const_t<T> value_;
    };
};

template <typename T>
class manual_lifetime<T &>
{
public:
    manual_lifetime() noexcept : value_(nullptr) {}
    ~manual_lifetime() {}

    T & construct(T & value) noexcept
    {
        value_ = std::addressof(value);
        return value;
    }

    void destruct() noexcept {}

    T & get() const noexcept { return *value_; }

private:
    T * value_;
};

template <typename T>
class manual_lifetime<T &&>
{
public:
    manual_lifetime() noexcept : value_(nullptr) {}
    ~manual_lifetime() {}

    T && construct(T && value) noexcept
    {
        value_ = std::addressof(value);
        return static_cast<T &&>(value);
    }

    void destruct() noexcept {}

    T && get() const noexcept { return static_cast<T &&>(*value_); }

private:
    T * value_;
};

struct use_allocator_arg
{};

template <typename Alloc>
static constexpr bool allocator_needs_to_be_stored =
  !std::allocator_traits<Alloc>::is_always_equal::value || !std::is_default_constructible_v<Alloc>;

// Round s up to next multiple of a.
constexpr std::size_t aligned_allocation_size(std::size_t s, std::size_t a)
{
    return (s + a - 1) & ~(a - 1);
}

} // namespace radr::detail

namespace radr
{

template <typename Rng, typename Allocator = detail::use_allocator_arg>
struct elements_of
{
    explicit constexpr elements_of(Rng && rng) noexcept
        requires std::is_default_constructible_v<Allocator>
      : range(static_cast<Rng &&>(rng))
    {}

    constexpr elements_of(Rng && rng, Allocator && alloc) noexcept : range((Rng &&)rng), alloc((Allocator &&)alloc) {}

    constexpr elements_of(elements_of &&) noexcept = default;

    constexpr elements_of(elements_of const &)             = delete;
    constexpr elements_of & operator=(elements_of const &) = delete;
    constexpr elements_of & operator=(elements_of &&)      = delete;

    constexpr Rng && get() noexcept { return static_cast<Rng &&>(range); }

    constexpr Allocator get_allocator() const noexcept { return alloc; }

private:
    [[no_unique_address]] Allocator alloc; // \expos
    Rng &&                          range; // \expos
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
class promise_base_alloc
{
    static constexpr std::size_t offset_of_allocator(std::size_t frameSize) noexcept
    {
        return aligned_allocation_size(frameSize, alignof(Alloc));
    }

    static constexpr std::size_t padded_frame_size(std::size_t frameSize) noexcept
    {
        return offset_of_allocator(frameSize) + sizeof(Alloc);
    }

    static Alloc & get_allocator(void * frame, std::size_t frameSize) noexcept
    {
        return *reinterpret_cast<Alloc *>(static_cast<char *>(frame) + offset_of_allocator(frameSize));
    }

public:
    template <typename... Args>
    static void * operator new(std::size_t frameSize, std::allocator_arg_t, Alloc alloc, Args &...)
    {
        void * frame = alloc.allocate(padded_frame_size(frameSize));

        // Store allocator at end of the coroutine frame.
        // Assuming the allocator's move constructor is non-throwing (a requirement for allocators)
        ::new (static_cast<void *>(std::addressof(get_allocator(frame, frameSize)))) Alloc(std::move(alloc));

        return frame;
    }

    template <typename This, typename... Args>
    static void * operator new(std::size_t frameSize, This &, std::allocator_arg_t, Alloc alloc, Args &...)
    {
        return promise_base_alloc::operator new(frameSize, std::allocator_arg, std::move(alloc));
    }

    static void operator delete(void * ptr, std::size_t frameSize) noexcept
    {
        Alloc & alloc = get_allocator(ptr, frameSize);
        Alloc   localAlloc(std::move(alloc));
        alloc.~Alloc();
        localAlloc.deallocate(static_cast<std::byte *>(ptr), padded_frame_size(frameSize));
    }
};

template <typename Alloc>
    requires(!allocator_needs_to_be_stored<Alloc>)
class promise_base_alloc<Alloc>
{
public:
    static void * operator new(std::size_t size)
    {
        Alloc alloc;
        return alloc.allocate(size);
    }

    static void operator delete(void * ptr, std::size_t size) noexcept
    {
        Alloc alloc;
        alloc.deallocate(static_cast<std::byte *>(ptr), size);
    }
};

template <typename Ref>
struct generator_promise_base
{
    template <typename Ref2, typename Value, typename Alloc>
    friend class radr::generator;

    generator_promise_base *                    root_;
    std::coroutine_handle<>                     parentOrLeaf_;
    // Note: Using manual_lifetime here to avoid extra calls to exception_ptr
    // constructor/destructor in cases where it is not needed (i.e. where this
    // generator coroutine is not used as a nested coroutine).
    // This member is lazily constructed by the yield_sequence_awaiter::await_suspend()
    // method if this generator is used as a nested generator.
    detail::manual_lifetime<std::exception_ptr> exception_;
    detail::manual_lifetime<Ref>                value_;

    explicit generator_promise_base(std::coroutine_handle<> thisCoro) noexcept : root_(this), parentOrLeaf_(thisCoro) {}

    ~generator_promise_base()
    {
        if (root_ != this)
        {
            // This coroutine was used as a nested generator and so will
            // have constructed its exception_ member which needs to be
            // destroyed here.
            exception_.destruct();
        }
    }

    std::suspend_always initial_suspend() noexcept { return {}; }

    void return_void() noexcept {}

    void unhandled_exception()
    {
        if (root_ != this)
        {
            exception_.get() = std::current_exception();
        }
        else
        {
            throw;
        }
    }

    // Transfers control back to the parent of a nested coroutine
    struct final_awaiter
    {
        bool await_ready() noexcept { return false; }

        template <typename Promise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> h) noexcept
        {
            Promise &                promise = h.promise();
            generator_promise_base & root    = *promise.root_;
            if (&root != &promise)
            {
                auto parent        = promise.parentOrLeaf_;
                root.parentOrLeaf_ = parent;
                return parent;
            }
            return std::noop_coroutine();
        }

        void await_resume() noexcept {}
    };

    final_awaiter final_suspend() noexcept { return {}; }

    std::suspend_always yield_value(Ref && x) noexcept(std::is_nothrow_move_constructible_v<Ref>)
    {
        root_->value_.construct((Ref &&)x);
        return {};
    }

    template <typename T>
        requires(!std::is_reference_v<Ref>) && std::is_convertible_v<T, Ref>
    std::suspend_always yield_value(T && x) noexcept(std::is_nothrow_constructible_v<Ref, T>)
    {
        root_->value_.construct((T &&)x);
        return {};
    }

    template <typename Gen>
    struct yield_sequence_awaiter
    {
        Gen gen_;

        yield_sequence_awaiter(Gen && g) noexcept
          // Taking ownership of the generator ensures frame are destroyed
          // in the reverse order of their execution.
          :
          gen_((Gen &&)g)
        {}

        bool await_ready() noexcept { return false; }

        // set the parent, root and exceptions pointer and
        // resume the nested
        template <typename Promise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> h) noexcept
        {
            generator_promise_base & current = h.promise();
            generator_promise_base & nested  = *gen_.get_promise();
            generator_promise_base & root    = *current.root_;

            nested.root_         = current.root_;
            nested.parentOrLeaf_ = h;

            // Lazily construct the exception_ member here now that we
            // know it will be used as a nested generator. This will be
            // destroyed by the promise destructor.
            nested.exception_.construct();
            root.parentOrLeaf_ = gen_.get_coro();

            // Immediately resume the nested coroutine (nested generator)
            return gen_.get_coro();
        }

        void await_resume()
        {
            generator_promise_base & nestedPromise = *gen_.get_promise();
            if (nestedPromise.exception_.get())
            {
                std::rethrow_exception(std::move(nestedPromise.exception_.get()));
            }
        }
    };

    template <typename OValue, typename OAlloc>
    yield_sequence_awaiter<generator<Ref, OValue, OAlloc>> yield_value(
      radr::elements_of<generator<Ref, OValue, OAlloc>> g) noexcept
    {
        return std::move(g).get();
    }

    template <std::ranges::range Rng, typename Allocator>
    yield_sequence_awaiter<generator<Ref, std::remove_cvref_t<Ref>, Allocator>> yield_value(
      radr::elements_of<Rng, Allocator> && x)
    {
        return [](std::allocator_arg_t, Allocator, auto && rng) -> generator<Ref, std::remove_cvref_t<Ref>, Allocator>
        {
            for (auto && e : rng)
                co_yield static_cast<decltype(e)>(e);
        }(std::allocator_arg, x.get_allocator(), std::forward<Rng>(x.get()));
    }

    void resume() { parentOrLeaf_.resume(); }

    // Disable use of co_await within this coroutine.
    void await_transform() = delete;
};

template <typename Generator, typename ByteAllocator, bool ExplicitAllocator = false>
struct generator_promise;

template <typename Ref, typename Value, typename Alloc, typename ByteAllocator, bool ExplicitAllocator>
struct generator_promise<generator<Ref, Value, Alloc>, ByteAllocator, ExplicitAllocator> final :
  public generator_promise_base<Ref>,
  public promise_base_alloc<ByteAllocator>
{
    generator_promise() noexcept :
      generator_promise_base<Ref>(std::coroutine_handle<generator_promise>::from_promise(*this))
    {}

    generator<Ref, Value, Alloc> get_return_object() noexcept
    {
        return generator<Ref, Value, Alloc>{std::coroutine_handle<generator_promise>::from_promise(*this)};
    }

    using generator_promise_base<Ref>::yield_value;

    template <std::ranges::range Rng>
    typename generator_promise_base<Ref>::template yield_sequence_awaiter<generator<Ref, Value, Alloc>> yield_value(
      radr::elements_of<Rng> && x)
    {
        static_assert(!ExplicitAllocator,
                      "This coroutine has an explicit allocator specified with std::allocator_arg so an allocator "
                      "needs to be passed "
                      "explicitely to std::elements_of");
        return [](auto && rng) -> generator<Ref, Value, Alloc>
        {
            for (auto && e : rng)
                co_yield static_cast<decltype(e)>(e);
        }(std::forward<Rng>(x.get()));
    }
};

template <typename Alloc>
using byte_allocator_t = typename std::allocator_traits<std::remove_cvref_t<Alloc>>::template rebind_alloc<std::byte>;

} // namespace radr::detail

namespace std
{

// Type-erased allocator with default allocator behaviour.
template <typename Ref, typename Value, typename... Args>
struct coroutine_traits<radr::generator<Ref, Value>, Args...>
{
    using promise_type = radr::detail::generator_promise<radr::generator<Ref, Value>, std::allocator<std::byte>>;
};

// Type-erased allocator with std::allocator_arg parameter
template <typename Ref, typename Value, typename Alloc, typename... Args>
struct coroutine_traits<radr::generator<Ref, Value>, allocator_arg_t, Alloc, Args...>
{
private:
    using byte_allocator = radr::detail::byte_allocator_t<Alloc>;

public:
    using promise_type =
      radr::detail::generator_promise<radr::generator<Ref, Value>, byte_allocator, true /*explicit Allocator*/>;
};

// Type-erased allocator with std::allocator_arg parameter (non-static member functions)
template <typename Ref, typename Value, typename This, typename Alloc, typename... Args>
struct coroutine_traits<radr::generator<Ref, Value>, This, allocator_arg_t, Alloc, Args...>
{
private:
    using byte_allocator = radr::detail::byte_allocator_t<Alloc>;

public:
    using promise_type =
      radr::detail::generator_promise<radr::generator<Ref, Value>, byte_allocator, true /*explicit Allocator*/>;
};

// Generator with specified allocator type
template <typename Ref, typename Value, typename Alloc, typename... Args>
struct coroutine_traits<radr::generator<Ref, Value, Alloc>, Args...>
{
    using byte_allocator = radr::detail::byte_allocator_t<Alloc>;

public:
    using promise_type = radr::detail::generator_promise<radr::generator<Ref, Value, Alloc>, byte_allocator>;
};

} // namespace std

namespace radr
{

// TODO :  make layout compatible promise casts possible
template <typename Ref, typename Value, typename Alloc>
class generator
{
    using byte_allocator = detail::byte_allocator_t<Alloc>;

public:
    using promise_type = detail::generator_promise<generator<Ref, Value, Alloc>, byte_allocator>;
    friend promise_type;

private:
    using coroutine_handle = std::coroutine_handle<promise_type>;

public:
    generator() noexcept = default;

    generator(generator && other) noexcept :
      coro_(std::exchange(other.coro_, {})), started_(std::exchange(other.started_, false))
    {}

    ~generator() noexcept
    {
        if (coro_)
        {
            if (started_ && !coro_.done())
            {
                coro_.promise().value_.destruct();
            }
            coro_.destroy();
        }
    }

    generator & operator=(generator && g) noexcept
    {
        swap(g);
        return *this;
    }

    void swap(generator & other) noexcept
    {
        std::swap(coro_, other.coro_);
        std::swap(started_, other.started_);
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

        iterator(iterator && other) noexcept : coro_(std::exchange(other.coro_, {})) {}

        iterator & operator=(iterator && other)
        {
            std::swap(coro_, other.coro_);
            return *this;
        }

        ~iterator() {}

        friend bool operator==(iterator const & it, sentinel) noexcept { return it.coro_.done(); }

        iterator & operator++()
        {
            coro_.promise().value_.destruct();
            coro_.promise().resume();
            return *this;
        }
        void operator++(int) { (void)operator++(); }

        reference operator*() const noexcept { return static_cast<reference>(coro_.promise().value_.get()); }

    private:
        friend generator;

        explicit iterator(coroutine_handle coro) noexcept : coro_(coro) {}

        coroutine_handle coro_;
    };

    iterator begin()
    {
        assert(coro_);
        assert(!started_);
        started_ = true;
        coro_.resume();
        return iterator{coro_};
    }

    sentinel end() noexcept { return {}; }

private:
    explicit generator(coroutine_handle coro) noexcept : coro_(coro) {}

public: // to get around access restrictions for yield_sequence_awaitable
    std::coroutine_handle<> get_coro() noexcept { return coro_; }
    promise_type *          get_promise() noexcept { return std::addressof(coro_.promise()); }

private:
    coroutine_handle coro_;
    bool             started_ = false;
};

// Specialisation for type-erased allocator implementation.
template <typename Ref, typename Value>
class generator<Ref, Value, detail::use_allocator_arg>
{
    using promise_base = detail::generator_promise_base<Ref>;

public:
    generator() noexcept : promise_(nullptr), coro_(), started_(false) {}

    generator(generator && other) noexcept :
      promise_(std::exchange(other.promise_, nullptr)),
      coro_(std::exchange(other.coro_, {})),
      started_(std::exchange(other.started_, false))
    {}

    ~generator() noexcept
    {
        if (coro_)
        {
            if (started_ && !coro_.done())
            {
                promise_->value_.destruct();
            }
            coro_.destroy();
        }
    }

    generator & operator=(generator g) noexcept
    {
        swap(g);
        return *this;
    }

    void swap(generator & other) noexcept
    {
        std::swap(promise_, other.promise_);
        std::swap(coro_, other.coro_);
        std::swap(started_, other.started_);
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

        iterator(iterator && other) noexcept :
          promise_(std::exchange(other.promise_, nullptr)), coro_(std::exchange(other.coro_, {}))
        {}

        iterator & operator=(iterator && other)
        {
            promise_ = std::exchange(other.promise_, nullptr);
            coro_    = std::exchange(other.coro_, {});
            return *this;
        }

        ~iterator() = default;

        friend bool operator==(iterator const & it, sentinel) noexcept { return it.coro_.done(); }

        iterator & operator++()
        {
            promise_->value_.destruct();
            promise_->resume();
            return *this;
        }

        void operator++(int) { (void)operator++(); }

        reference operator*() const noexcept { return static_cast<reference>(promise_->value_.get()); }

    private:
        friend generator;

        explicit iterator(promise_base * promise, std::coroutine_handle<> coro) noexcept :
          promise_(promise), coro_(coro)
        {}

        promise_base *          promise_;
        std::coroutine_handle<> coro_;
    };

    iterator begin()
    {
        assert(coro_);
        assert(!started_);
        started_ = true;
        coro_.resume();
        return iterator{promise_, coro_};
    }

    sentinel end() noexcept { return {}; }

private:
    template <typename Generator, typename ByteAllocator, bool ExplicitAllocator>
    friend struct detail::generator_promise;

    template <typename Promise>
    explicit generator(std::coroutine_handle<Promise> coro) noexcept :
      promise_(std::addressof(coro.promise())), coro_(coro)
    {}

public: // to get around access restrictions for yield_sequence_awaitable
    std::coroutine_handle<> get_coro() noexcept { return coro_; }
    promise_base *          get_promise() noexcept { return promise_; }

private:
    promise_base *          promise_;
    std::coroutine_handle<> coro_;
    bool                    started_ = false;
};

} // namespace radr

namespace std::ranges
{

template <typename T, typename U, typename Alloc>
inline constexpr bool enable_view<radr::generator<T, U, Alloc>> = true;

} // namespace std::ranges

#endif
