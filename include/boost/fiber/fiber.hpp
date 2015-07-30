
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_FIBERS_FIBER_H
#define BOOST_FIBERS_FIBER_H

#include <algorithm>
#include <exception>
#include <memory>
#include <tuple>
#include <utility>

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/context/all.hpp>
#include <boost/intrusive_ptr.hpp>

#include <boost/fiber/detail/config.hpp>
#include <boost/fiber/fiber_context.hpp>
#include <boost/fiber/fixedsize_stack.hpp>
#include <boost/fiber/detail/scheduler.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace fibers {

class fiber_context;

class BOOST_FIBERS_DECL fiber {
private:
    friend struct detail::scheduler;

    typedef intrusive_ptr< fiber_context >  ptr_t;

    ptr_t       impl_;

    void start_();

    template< typename StackAlloc, typename Fn, typename ... Args >
    static ptr_t create( StackAlloc salloc, Fn && fn, Args && ... args) {
        context::stack_context sctx( salloc.allocate() );
#if defined(BOOST_NO_CXX14_CONSTEXPR) || defined(BOOST_NO_CXX11_STD_ALIGN)
        // reserve space for control structure
        std::size_t size = sctx.size - sizeof( fiber_context);
        void * sp = static_cast< char * >( sctx.sp) - sizeof( fiber_context);
#else
        constexpr std::size_t func_alignment = 64; // alignof( fiber_context);
        constexpr std::size_t func_size = sizeof( fiber_context);
        // reserve space on stack
        void * sp = static_cast< char * >( sctx.sp) - func_size - func_alignment;
        // align sp pointer
        std::size_t space = func_size + func_alignment;
        sp = std::align( func_alignment, func_size, sp, space);
        BOOST_ASSERT( nullptr != sp);
        // calculate remaining size
        std::size_t size = sctx.size - ( static_cast< char * >( sctx.sp) - static_cast< char * >( sp) );
#endif
        // placement new of fiber_context on top of fiber's stack
        return ptr_t( 
            new ( sp) fiber_context( context::preallocated( sp, size, sctx), salloc,
                                     std::forward< Fn >( fn),
                                     std::make_tuple( std::forward< Args >( args) ... ),
                                     std::index_sequence_for< Args ... >() ) );
    }

public:
    typedef fiber_context::id    id;

    fiber() noexcept :
        impl_() {
    }

    template< typename Fn, typename ... Args >
    explicit fiber( Fn && fn, Args && ... args) :
        fiber( std::allocator_arg, fixedsize_stack(),
               std::forward< Fn >( fn), std::forward< Args >( args) ... ) {
    }

    template< typename StackAllocator, typename Fn, typename ... Args >
    explicit fiber( std::allocator_arg_t, StackAllocator salloc, Fn && fn, Args && ... args) :
        impl_( create( salloc, std::forward< Fn >( fn), std::forward< Args >( args) ... ) ) {
        start_();
    }

    ~fiber() {
        if ( joinable() ) {
            std::terminate();
        }
    }

    fiber( fiber const&) = delete;
    fiber & operator=( fiber const&) = delete;

    fiber( fiber && other) noexcept :
        impl_() {
        impl_.swap( other.impl_);
    }

    fiber & operator=( fiber && other) noexcept {
        if ( joinable() ) {
            std::terminate();
        }
        if ( this != & other) {
            impl_.swap( other.impl_);
        }
        return * this;
    }

    explicit operator bool() const noexcept {
        return impl_ && ! impl_->is_terminated();
    }

    bool operator!() const noexcept {
        return ! impl_ || impl_->is_terminated();
    }

    void swap( fiber & other) noexcept {
        impl_.swap( other.impl_);
    }

    bool joinable() const noexcept {
        return nullptr != impl_ /* && ! impl_->is_terminated() */;
    }

    id get_id() const noexcept {
        return impl_ ? impl_->get_id() : id();
    }

    void detach();

    void join();

    void interrupt() noexcept;

    template< typename PROPS >
    PROPS & properties() {
        fiber_properties* props = impl_->get_properties();
        BOOST_ASSERT_MSG(props, "fiber::properties not set");
        return dynamic_cast< PROPS & >( * props );
    }
};

inline
bool operator<( fiber const& l, fiber const& r) {
    return l.get_id() < r.get_id();
}

inline
void swap( fiber & l, fiber & r) {
    return l.swap( r);
}

}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // BOOST_FIBERS_FIBER_H
