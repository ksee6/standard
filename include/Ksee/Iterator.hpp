/**
 * @file Iterator.hpp
 * @brief High-performance coroutine / generator iterator for Ksee and Meca.
 */

#ifndef KSEE_ITERATOR_HPP
#define KSEE_ITERATOR_HPP

#include "Types.hpp"
#include "Func.hpp"
#include "Array.hpp"

#if !defined(__KERNEL__) && !defined(KSEE_NO_STD) && !defined(MECA_EMBEDDED)
#include <ucontext.h>
#include <new>
#endif

namespace Ksee {

template <typename T>
inline T&& Forward(typename RemoveRef<T>::Type& t) noexcept {
    return static_cast<T&&>(t);
}

template <typename T>
inline T&& Forward(typename RemoveRef<T>::Type&& t) noexcept {
    return static_cast<T&&>(t);
}

struct YieldContext {
    void* userPtr = nullptr;
    void (*yieldVal)(void* userPtr, const void* valPtr, usz valSize) = nullptr;
    void (*yieldVoid)(void* userPtr) = nullptr;
};

#if !defined(__KERNEL__) && !defined(KSEE_NO_STD)
#if defined(MECA_EMBEDDED) && !defined(__x86_64__) && !defined(_M_X64)
inline YieldContext* tl_current_yield = nullptr;
#else
inline thread_local YieldContext* tl_current_yield = nullptr;
#endif

template <typename T>
inline void yield(const T& val) {
    if (tl_current_yield && tl_current_yield->yieldVal) {
        tl_current_yield->yieldVal(tl_current_yield->userPtr, &val, sizeof(T));
    }
}

inline void yield() {
    if (tl_current_yield && tl_current_yield->yieldVoid) {
        tl_current_yield->yieldVoid(tl_current_yield->userPtr);
    }
}
#else
template <typename T> inline void yield(const T&) {}
inline void yield() {}
#endif

template <typename R>
struct ReturnStorage {
    R retVal{};
    R& getReturn() { return retVal; }
    const R& getReturn() const { return retVal; }
    void setReturn(const R& v) { retVal = v; }
    void setReturn(R&& v) { retVal = Ksee::Move(v); }
};

template <>
struct ReturnStorage<void> {
    void getReturn() const {}
};

/**
 * @class Iterator
 * @brief Coroutine generator producing StepType items and returning ReturnType.
 */
template <typename StepType, typename ReturnType = void>
class Iterator : public ReturnStorage<ReturnType> {
public:
    Func<ReturnType()> fn;
#if !defined(__KERNEL__) && !defined(KSEE_NO_STD) && !defined(MECA_EMBEDDED)
    ucontext_t callerCtx;
    ucontext_t fiberCtx;
    char* stack = nullptr;
    usz stackSize = 128 * 1024;
#endif

    Array<StepType> queue;
    bool done = false;
    bool started = false;
    YieldContext yctx;

    Iterator() {
        initYCtx();
    }

    template <typename Callable, typename = typename EnableIf<!IsSame<typename Decay<Callable>::Type, Iterator>::Value>::Type>
    Iterator(Callable&& f) : fn(Ksee::Forward<Callable>(f)) {
        initYCtx();
#if !defined(__KERNEL__) && !defined(KSEE_NO_STD) && !defined(MECA_EMBEDDED)
        stack = new char[stackSize];
#endif
    }

    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;

    Iterator(Iterator&& o) noexcept 
        : ReturnStorage<ReturnType>(Ksee::Move(o)),
          fn(static_cast<Func<ReturnType()>&&>(o.fn)),
          queue(static_cast<Array<StepType>&&>(o.queue)),
          done(o.done), started(o.started), yctx(o.yctx)
#if !defined(__KERNEL__) && !defined(KSEE_NO_STD) && !defined(MECA_EMBEDDED)
          , callerCtx(o.callerCtx), fiberCtx(o.fiberCtx), stack(o.stack), stackSize(o.stackSize)
#endif
    {
#if !defined(__KERNEL__) && !defined(KSEE_NO_STD) && !defined(MECA_EMBEDDED)
        o.stack = nullptr;
#endif
        yctx.userPtr = this;
    }

    Iterator& operator=(Iterator&& o) noexcept {
        if (this != &o) {
#if !defined(__KERNEL__) && !defined(KSEE_NO_STD) && !defined(MECA_EMBEDDED)
            delete[] stack;
            stack = o.stack;
            stackSize = o.stackSize;
            callerCtx = o.callerCtx;
            fiberCtx = o.fiberCtx;
            o.stack = nullptr;
#endif
            ReturnStorage<ReturnType>::operator=(Ksee::Move(o));
            fn = static_cast<Func<ReturnType()>&&>(o.fn);
            queue = static_cast<Array<StepType>&&>(o.queue);
            done = o.done;
            started = o.started;
            yctx = o.yctx;
            yctx.userPtr = this;
        }
        return *this;
    }

    virtual ~Iterator() {
#if !defined(__KERNEL__) && !defined(KSEE_NO_STD) && !defined(MECA_EMBEDDED)
        delete[] stack;
#endif
    }

    void initYCtx() {
        yctx.userPtr = this;
        yctx.yieldVal = [](void* p, const void* v, usz) {
#if !defined(__KERNEL__) && !defined(KSEE_NO_STD) && !defined(MECA_EMBEDDED)
            auto* self = static_cast<Iterator*>(p);
            self->queue.push(*static_cast<const StepType*>(v));
            YieldContext* prev = tl_current_yield;
            tl_current_yield = nullptr;
            swapcontext(&self->fiberCtx, &self->callerCtx);
            tl_current_yield = prev;
#endif
        };
        yctx.yieldVoid = [](void* p) {
#if !defined(__KERNEL__) && !defined(KSEE_NO_STD) && !defined(MECA_EMBEDDED)
            auto* self = static_cast<Iterator*>(p);
            YieldContext* prev = tl_current_yield;
            tl_current_yield = nullptr;
            swapcontext(&self->fiberCtx, &self->callerCtx);
            tl_current_yield = prev;
#endif
        };
    }

#if !defined(__KERNEL__) && !defined(KSEE_NO_STD) && !defined(MECA_EMBEDDED)
    static void fiberEntry(u32 lo, u32 hi) {
        usz ptr = ((usz)hi << 32) | (usz)lo;
        auto* self = reinterpret_cast<Iterator*>(ptr);
        tl_current_yield = &self->yctx;
        if constexpr (IsSame<ReturnType, void>::Value) {
            self->fn();
        } else {
            self->setReturn(self->fn());
        }
        self->done = true;
        tl_current_yield = nullptr;
        swapcontext(&self->fiberCtx, &self->callerCtx);
    }
#endif

    virtual void resume() {
        if (done) return;
#if !defined(__KERNEL__) && !defined(KSEE_NO_STD) && !defined(MECA_EMBEDDED)
        if (!started) {
            started = true;
            if (!stack) stack = new char[stackSize];
            getcontext(&fiberCtx);
            fiberCtx.uc_stack.ss_sp = stack;
            fiberCtx.uc_stack.ss_size = stackSize;
            fiberCtx.uc_link = &callerCtx;
            usz ptr = reinterpret_cast<usz>(this);
            u32 lo = (u32)(ptr & 0xFFFFFFFF);
            u32 hi = (u32)(ptr >> 32);
            makecontext(&fiberCtx, (void(*)())fiberEntry, 2, lo, hi);
        }
        YieldContext* prev = tl_current_yield;
        swapcontext(&callerCtx, &fiberCtx);
        tl_current_yield = prev;
#else
        done = true;
#endif
    }

    virtual usz available() {
        while (queue.size() == 0 && !done) {
            resume();
        }
        return queue.size();
    }

    ReturnType returnValue() const { return this->getReturn(); }
    ReturnType result() const { return this->getReturn(); }

    virtual StepType next() {
        if (available() > 0) {
            StepType val = queue[0];
            queue.remove(0);
            return val;
        }
        return StepType{};
    }

    virtual StepType operator*() {
        available();
        if (queue.size() > 0) return queue[0];
        return StepType{};
    }

    struct IterSentinel {};
    struct IterCursor {
        Iterator* self = nullptr;
        bool operator!=(const IterSentinel&) const {
            return self ? self->available() > 0 : false;
        }
        StepType operator*() const {
            return self ? self->operator*() : StepType{};
        }
        IterCursor& operator++() {
            if (self) {
                self->next();
            }
            return *this;
        }
    };

    IterCursor begin() {
        return IterCursor{this};
    }
    IterSentinel end() { return IterSentinel{}; }
};

} // namespace Ksee

#endif // KSEE_ITERATOR_HPP
