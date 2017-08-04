/*
 *
 *  Multi Process Garbage Collector
 *  Copyright © 2016 Hewlett Packard Enterprise Development Company LP.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  As an exception, the copyright holders of this Library grant you permission
 *  to (i) compile an Application with the Library, and (ii) distribute the 
 *  Application containing code generated by the Library and added to the 
 *  Application during this compilation process under terms of your choice, 
 *  provided you also meet the terms and conditions of the Application license.
 *
 */

#ifndef WEAK_GC_PTR_H
#define WEAK_GC_PTR_H

#include "mpgc/offset_ptr.h"

namespace mpgc {
  struct gc_control_block;

  template <typename T>
  class weak_gc_ptr {
    template <typename U> friend class weak_gc_ptr;
    offset_ptr<T> _ptr;

    offset_ptr<T> from_strong_to_weak(const offset_ptr<T> &rhs) {
      constexpr auto ptr_type_fld = bits::field<special_ptr_type, std::size_t>(0, 2);
      return rhs.is_null() ? offset_ptr<T>(nullptr) :
                             offset_ptr<T>(rhs.val() | ptr_type_fld.encode(special_ptr_type::Weak));
    }

    template <typename Fn>
    void write_barrier(Fn&&);//For gc_ptr and nullptr

  public:
    using element_type = T;

    weak_gc_ptr() noexcept {
      write_barrier([this] {_ptr = nullptr;});
    }
    constexpr weak_gc_ptr(std::nullptr_t) noexcept : weak_gc_ptr{} {}

    offset_ptr<T>& as_offset_pointer() { return _ptr; }

    template <typename LoadFn, typename ModFn>
    static void write_barrier(void*, const void*, LoadFn&&, ModFn&&) noexcept;

    static bool marked_or_sweep_allocated(gc_control_block&, const offset_ptr<T>&);
    static void clear_sweep_allocated(const offset_ptr<T>&);

    weak_gc_ptr(const offset_ptr<T> &rhs) noexcept {
      write_barrier(this, &rhs, [&rhs]{return rhs;},
                    [this](const offset_ptr<T>& p) {_ptr = p;});
    }
    weak_gc_ptr(offset_ptr<T> &&rhs) noexcept {
      offset_ptr<T> r = std::move(rhs);
      write_barrier(this, &r, [&r]{return r;}, [this](const offset_ptr<T>& p) {_ptr = p;});
    }

    weak_gc_ptr(const weak_gc_ptr &rhs) noexcept {
      write_barrier(this, &rhs, [&rhs] {return rhs._ptr;}, [this](const offset_ptr<T>& p) {_ptr = p;});
    }
    weak_gc_ptr(weak_gc_ptr &&rhs) noexcept {
      offset_ptr<T> r = std::move(rhs._ptr);
      write_barrier(this, &r, [&r]{return r;},
                          [this](const offset_ptr<T>& p) {_ptr = p;});
    }
    weak_gc_ptr &operator =(const weak_gc_ptr &rhs) noexcept {
      write_barrier(this, &rhs, [&rhs] {return rhs._ptr;}, [this](const offset_ptr<T>& p) {_ptr = p;});
      return *this;
    }
    weak_gc_ptr &operator =(weak_gc_ptr &&rhs) noexcept {
      offset_ptr<T> r = std::move(rhs._ptr);
      write_barrier(this, &r, [&r]{return r;},
                          [this](const offset_ptr<T>& p) {_ptr = p;});
      return *this;
    }

    template <typename Y, typename = std::enable_if_t<std::is_assignable<T*&,Y*>::value> >
    weak_gc_ptr(const weak_gc_ptr<Y> &rhs) noexcept {
      write_barrier(this, &rhs, [&rhs]{return rhs._ptr;},
                          [this](const offset_ptr<T>& p) {_ptr = p;});
    }
    template <typename Y, typename = std::enable_if_t<std::is_assignable<T*&,Y*>::value> >
    weak_gc_ptr(weak_gc_ptr<Y> &&rhs) noexcept {
      offset_ptr<Y> r = std::move(rhs._ptr);
      write_barrier(this, &r, [&r]{return r;},
                          [this](const offset_ptr<T>& p) {_ptr = p;});
    }
    template <typename Y, typename = std::enable_if_t<std::is_assignable<T*&,Y*>::value> >
    weak_gc_ptr(const gc_ptr<Y> &rhs) noexcept {
      write_barrier([this, &rhs] {
        _ptr = from_strong_to_weak(rhs.as_offset_pointer());
      });
    }
    template <typename Y, typename = std::enable_if_t<std::is_assignable<T*&,Y*>::value> >
    weak_gc_ptr(gc_ptr<Y> &&rhs) noexcept {
      write_barrier([this, r = std::move(rhs.as_offset_pointer())] {
        _ptr = from_strong_to_weak(r);
      });
    }
    template <typename Y, typename = std::enable_if_t<std::is_assignable<T*&,Y*>::value> >
    weak_gc_ptr(const external_gc_ptr<Y> &rhs) noexcept : weak_gc_ptr{rhs.value()} {}

    template <typename Y, typename = std::enable_if_t<std::is_assignable<T*&,Y*>::value> >
    weak_gc_ptr(external_gc_ptr<Y> &&rhs) noexcept : weak_gc_ptr{std::move(rhs.value())} {}

    weak_gc_ptr &operator =(nullptr_t) noexcept {
      write_barrier([this]{_ptr = nullptr;});
      return *this;
    }

    template <typename Y, typename = std::enable_if_t<std::is_assignable<T*&,Y*>::value> >
    weak_gc_ptr &operator =(const weak_gc_ptr<Y> &rhs) noexcept {
      write_barrier(this, &rhs, [&rhs]{return rhs._ptr;}, [this](const offset_ptr<T>& p) {_ptr = p;});
      return *this;
    }

    template <typename Y, typename = std::enable_if_t<std::is_assignable<T*&,Y*>::value> >
    weak_gc_ptr &operator =(weak_gc_ptr<Y> &&rhs) noexcept {
      offset_ptr<Y> r = std::move(rhs._ptr);
      write_barrier(this, &r, [&r]{return r;},
                          [this](const offset_ptr<T>& p) {_ptr = p;});
      return *this;
    }

    template <typename Y, typename = std::enable_if_t<std::is_assignable<T*&,Y*>::value> >
    weak_gc_ptr &operator =(const gc_ptr<Y> &rhs) noexcept {
      write_barrier([this, &rhs] {
        _ptr = from_strong_to_weak(rhs.as_offset_pointer());
      });
      return *this;
    }

    template <typename Y, typename = std::enable_if_t<std::is_assignable<T*&,Y*>::value> >
    weak_gc_ptr &operator =(gc_ptr<Y> &&rhs) noexcept {
      write_barrier([this, r = std::move(rhs.as_offset_pointer())] {
        _ptr = from_strong_to_weak(r);
      });
      return *this;
    }

    template <typename Y, typename = std::enable_if_t<std::is_assignable<T*&,Y*>::value> >
    weak_gc_ptr &operator =(const external_gc_ptr<Y> &rhs) noexcept {
      *this = rhs.value();
      return *this;
    }

    template <typename Y, typename = std::enable_if_t<std::is_assignable<T*&,Y*>::value> >
    weak_gc_ptr &operator =(external_gc_ptr<Y> &&rhs) noexcept {
      *this = std::move(rhs.value());
      return *this;
    }

    gc_ptr<T> lock() const noexcept;

    bool expired() const noexcept {
      return _ptr == nullptr;
    }

    void reset() {
      *this = nullptr;
    }

    void swap(weak_gc_ptr &r) {
      weak_gc_ptr temp = r;
      r = std::move(*this);
      *this = std::move(temp);
    }

    /**
     * Compute the first hash by delegating to the underlying bare pointer.
     *
     * hash1() and hash2() allow gc_ptr to be used as a key in
     * gc_cuckoo_map, as long as there is a specialization for
     * hash1<T*>() and hash2<T*>().
     */
    constexpr std::uint64_t hash1() const {
      return _ptr.hash1();
    }
    /**
     * Compute the second hash by delegating to the underlying bare pointer.
     *
     * hash1() and hash2() allow gc_ptr to be used as a key in
     * gc_cuckoo_map, as long as there is a specialization for
     * hash1<T*>() and hash2<T*>().
     */
    constexpr std::uint64_t hash2() const {
      return _ptr.hash2();
    }

    template <typename X, typename Y>
    friend mpgc::weak_gc_ptr<X> std::static_pointer_cast(const mpgc::weak_gc_ptr<Y> &);
    template <typename X, typename Y>
    friend mpgc::weak_gc_ptr<X> std::dynamic_pointer_cast(const mpgc::weak_gc_ptr<Y> &);
    template <typename X, typename Y>
    friend mpgc::weak_gc_ptr<X> std::const_pointer_cast(const mpgc::weak_gc_ptr<Y> &);
  };
}

namespace ruts {
  template <typename T>
  struct hash1<mpgc::weak_gc_ptr<T>> {
    auto operator()(const mpgc::weak_gc_ptr<T> &ptr) const {
      return ptr.hash1();
    }
  };
  template <typename T>
  struct hash2<mpgc::weak_gc_ptr<T>> {
    auto operator()(const mpgc::weak_gc_ptr<T> &ptr) const {
      return ptr.hash2();
    }
  };

}

namespace std {
  template <typename T>
  inline
  void swap(mpgc::weak_gc_ptr<T> &lhs, mpgc::weak_gc_ptr<T> &rhs) {
    lhs.swap(rhs);
  }

  template <typename C, typename T, typename X>
  basic_ostream<C,T> &
  operator <<(basic_ostream<C,T> &os, const mpgc::weak_gc_ptr<X> &ptr) {
    // We get rid of the lock as soon as we can.
    auto op = ptr.lock().as_offset_pointer();
    return os << op;
  }

  template <typename T, typename U>
  mpgc::weak_gc_ptr<T>
  static_pointer_cast(const mpgc::weak_gc_ptr<U> &r) {
    return mpgc::weak_gc_ptr<T>(static_pointer_cast<mpgc::gc_ptr<T>>(r.lock()));
  }

  template <typename T, typename U>
  inline
  mpgc::weak_gc_ptr<T>
  dynamic_pointer_cast(const mpgc::weak_gc_ptr<U> &r) {
    return mpgc::weak_gc_ptr<T>(dynamic_pointer_cast<mpgc::gc_ptr<T>>(r.lock()));
  }

  template <typename T, typename U>
  inline
  mpgc::weak_gc_ptr<T>
  const_pointer_cast(const mpgc::weak_gc_ptr<U> &r) {
    return mpgc::weak_gc_ptr<T>(const_pointer_cast<mpgc::gc_ptr<T>>(r.lock()));
  }

  template <typename T>
  struct hash<mpgc::weak_gc_ptr<T>> {
    std::size_t operator()(const mpgc::weak_gc_ptr<T> &val) const noexcept {
      return hash<mpgc::gc_ptr<T>>()(val.lock());
    }
  };

  template <typename T>
  struct versioned_pointer_traits<mpgc::weak_gc_ptr<T>>
    : default_versioned_pointer_traits<mpgc::weak_gc_ptr<T>>
  {
  private:
    using opvpt = versioned_pointer_traits<mpgc::offset_ptr<T>>;
  public:
    using prim_rep = typename opvpt::prim_rep;

    template <typename M>
    static void construct_prim_rep(const void *loc,
                                   const mpgc::weak_gc_ptr<T> &p,
                                   M&& mod) {
      if (!p.expired()) {
        mpgc::weak_gc_ptr<T>::write_barrier(loc, &p, [&p]{return p.as_offset_pointer();},
                   [mod=forward<M>(mod)](const mpgc::offset_ptr<T> &ptr) {
                     return mod(reinterpret_cast<const mpgc::weak_gc_ptr<T>&>(ptr));
                   });
      }
    }
    constexpr static mpgc::weak_gc_ptr<T> from_prim_rep(prim_rep p) {
      return mpgc::weak_gc_ptr<T>(opvpt::from_prim_rep(p));
    }
    constexpr static prim_rep to_prim_rep(mpgc::weak_gc_ptr<T> p) {
      return opvpt::to_prim_rep(p.as_offset_pointer());
    }
    template <typename M, typename OV, typename NV>
    static void modify(const void *loc, M&& mod, OV&& old_val, NV&& new_val) {
      mpgc::offset_ptr<T> n_val = forward<NV>(new_val)().as_offset_pointer();
      mpgc::weak_gc_ptr<T>::write_barrier(loc, &n_val,
                   [&n_val]{return n_val;},
                   [mod=forward<M>(mod)](const mpgc::offset_ptr<T> &p){
                     return mod(reinterpret_cast<const mpgc::weak_gc_ptr<T>&>(p));
                   });
    }

    struct copy_ctor_behavior {
      template <typename M>
      static void doit(const void *loc,
                       const mpgc::weak_gc_ptr<T> &p,
                       M &&mod) {
        construct_prim_rep(loc, p, mod);
      }
    };
    struct is_dereference_allowed : false_type {};
  };

  template <typename T>
  class atomic<mpgc::weak_gc_ptr<T>>
    : public ruts::default_atomic<mpgc::weak_gc_ptr<T>>
  {
    using base = ruts::default_atomic<mpgc::weak_gc_ptr<T>>;
    using typename base::contained_type;
  public:
    /*
     * All subclasses need to import constructors and
     * assignment
     */
    using base::load_order;
    using base::load;

    atomic(const contained_type &desired) {
      mpgc::weak_gc_ptr<T>::write_barrier(this, &desired,
                             [&desired]{return desired.as_offset_pointer();},
                             [this](const mpgc::offset_ptr<T> &p) {
                               base(reinterpret_cast<const mpgc::weak_gc_ptr<T>&>(p));
                             }
      );
    }
    constexpr atomic() : base(contained_type()) {};
    atomic(const atomic &) = delete;

    contained_type operator =(const contained_type &desired) {
      store(desired);
      return desired;
    }
    contained_type operator =(const contained_type &desired) volatile {
      store(desired);
      return desired;
    }

    void store(const mpgc::weak_gc_ptr<T> &desired,
               std::memory_order order = std::memory_order_seq_cst)
    {
      mpgc::weak_gc_ptr<T>::write_barrier(this, &desired,
                         [&desired]{return desired.as_offset_pointer();},
                         [this, order](const mpgc::offset_ptr<T> &p) {
                           base::store(reinterpret_cast<const mpgc::weak_gc_ptr<T>&>(p), order);
                         }
      );
    }
    void store(const mpgc::gc_ptr<T> &desired,
               std::memory_order order = std::memory_order_seq_cst) volatile
    {
      mpgc::weak_gc_ptr<T>::write_barrier(this, &desired,
                         [&desired]{return desired.as_offset_pointer();},
                         [this, order](const mpgc::offset_ptr<T> &p) {
                           base::store(reinterpret_cast<const mpgc::weak_gc_ptr<T>&>(p), order);
                         }
      );
    }

    mpgc::weak_gc_ptr<T> exchange(const mpgc::weak_gc_ptr<T> &desired,
                                  std::memory_order order = std::memory_order_seq_cst)
    {
      mpgc::weak_gc_ptr<T> ret;
      mpgc::weak_gc_ptr<T>::write_barrier(this, &desired,
                  [&desired]{return desired.as_offset_pointer();},
                  [this, order, &ret](const mpgc::offset_ptr<T> &p) {
                    ret = base::exchange(reinterpret_cast<const mpgc::weak_gc_ptr<T>&>(p), order);
                  }
      );
      return ret;
    }
    mpgc::weak_gc_ptr<T> exchange(const mpgc::weak_gc_ptr<T> &desired,
                                  std::memory_order order = std::memory_order_seq_cst) volatile
    {
      mpgc::weak_gc_ptr<T> ret;
      mpgc::weak_gc_ptr<T>::write_barrier(this, &desired,
                  [&desired]{return desired.as_offset_pointer();},
                  [this, order, &ret](const mpgc::offset_ptr<T> &p) {
                    ret = base::exchange(reinterpret_cast<const mpgc::weak_gc_ptr<T>&>(p), order);
                  }
      );
      return ret;
    }

    bool compare_exchange_weak(mpgc::weak_gc_ptr<T> &expected,
                               const mpgc::weak_gc_ptr<T> &desired,
                               std::memory_order success,
                               std::memory_order failure)
    {
      bool ret;
      mpgc::weak_gc_ptr<T>::write_barrier(this, &desired,
            [&desired]{return desired.as_offset_pointer();},
            [&](const mpgc::offset_ptr<T> &p) {
              ret = base::compare_exchange_weak(expected,
                                                reinterpret_cast<const mpgc::weak_gc_ptr<T>&>(p),
                                                success,
                                                failure);
            }
      );
      return ret;
    }
    bool compare_exchange_weak(contained_type &expected,
                               const contained_type &desired,
                               std::memory_order order = std::memory_order_seq_cst)
    {
      return compare_exchange_weak(expected, desired, order, load_order(order));
    }
    bool compare_exchange_weak(mpgc::weak_gc_ptr<T> &expected,
                               const mpgc::weak_gc_ptr<T> &desired,
                               std::memory_order success,
                               std::memory_order failure) volatile
    {
      bool ret;
      mpgc::weak_gc_ptr<T>::write_barrier(this, &desired,
            [&desired]{return desired.as_offset_pointer();},
            [&](const mpgc::offset_ptr<T> &p) {
              ret = base::compare_exchange_weak(expected,
                                                reinterpret_cast<const mpgc::weak_gc_ptr<T>&>(p),
                                                success,
                                                failure);
            }
      );
      return ret;
    }
    bool compare_exchange_weak(contained_type &expected,
                               const contained_type &desired,
                               std::memory_order order = std::memory_order_seq_cst) volatile
    {
      return compare_exchange_weak(expected, desired, order, load_order(order));
    }

    /*
     * If you override one with a name, you have to import the whole
     * set, otherwise you lose the others.
     */
    bool compare_exchange_strong(mpgc::weak_gc_ptr<T> &expected,
				 const mpgc::weak_gc_ptr<T> &desired,
				 std::memory_order success,
				 std::memory_order failure)
    {
      // mark expected
      bool ret;
      mpgc::weak_gc_ptr<T>::write_barrier(this, &desired,
            [&desired]{return desired.as_offset_pointer();},
            [&](const mpgc::offset_ptr<T> &p) {
              ret = base::compare_exchange_strong(expected,
                                                  reinterpret_cast<const mpgc::weak_gc_ptr<T>&>(p),
                                                  success,
                                                  failure);
            }
      );
      return ret;
    }
    bool compare_exchange_strong(contained_type &expected,
                                 const contained_type &desired,
                                 std::memory_order order = std::memory_order_seq_cst)
    {
      return compare_exchange_strong(expected, desired, order, load_order(order));
    }
    bool compare_exchange_strong(mpgc::weak_gc_ptr<T> &expected,
                                 const mpgc::weak_gc_ptr<T> &desired,
                                 std::memory_order success,
                                 std::memory_order failure) volatile
    {
      bool ret;
      mpgc::weak_gc_ptr<T>::write_barrier(this, &desired,
            [&desired]{return desired.as_offset_pointer();},
            [&](const mpgc::offset_ptr<T> &p) {
              ret = base::compare_exchange_strong(expected,
                                                  reinterpret_cast<const mpgc::weak_gc_ptr<T>&>(p),
                                                  success,
                                                  failure);
            }
      );
      return ret;
    }
    bool compare_exchange_strong(contained_type &expected,
                                 const contained_type &desired,
                                 std::memory_order order = std::memory_order_seq_cst) volatile
    {
      return compare_exchange_strong(expected, desired, order, load_order(order));
    }


    auto lock() const {
      return load().lock();
    }
  };
}
#endif