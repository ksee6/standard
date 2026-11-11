/**
 * @file Array.hpp
 * @brief High-performance unified contiguous and sparse dynamic array with CoW and device support.
 */

#ifndef KSEE_ARRAY_HPP
#define KSEE_ARRAY_HPP

#include "Ksee.hpp"
#if defined(__has_include)
#  if __has_include(<initializer_list>)
#    include <initializer_list>
#    define KSEE_HAS_STD_INITIALIZER_LIST 1
#  endif
#else
#  include <initializer_list>
#  define KSEE_HAS_STD_INITIALIZER_LIST 1
#endif
#if !defined(__KERNEL__) && !defined(XI_NO_STD) && !defined(MECA_EMBEDDED)
#if defined(__has_include)
#  if __has_include(<cstring>)
#    include <cstring>
#  endif
#  if __has_include(<type_traits>)
#    include <type_traits>
#    define KSEE_HAS_STD_TYPE_TRAITS 1
#  endif
#  if __has_include(<atomic>)
#    include <atomic>
#    define KSEE_HAS_STD_ATOMIC 1
#  endif
#else
#  include <cstring>
#  include <type_traits>
#  define KSEE_HAS_STD_TYPE_TRAITS 1
#  include <atomic>
#  define KSEE_HAS_STD_ATOMIC 1
#endif
#endif

#if defined(AVR) || defined(ARDUINO)
#define KSEE_ARRAY_MIN_CAP 4
#else
#define KSEE_ARRAY_MIN_CAP 16
#endif

namespace Ksee {

namespace ArrayDetail {
template <typename U>
inline void copyOrMove(U *dst, U &src, bool canMove) {
#if defined(KSEE_HAS_STD_TYPE_TRAITS)
  if constexpr (std::is_copy_constructible<U>::value) {
    if (canMove) {
      new (dst) U(Ksee::Move(src));
    } else {
      new (dst) U(src);
    }
  } else {
    new (dst) U(Ksee::Move(src));
  }
#else
  new (dst) U(Ksee::Move(src));
#endif
}
}

/**
 * @class Array
 * @brief Unified dynamic array container supporting contiguous buffers, sparse fragments,
 * reference counting with CoW, and multidimensional tensors.
 *
 * @tparam T Element type.
 */
template <typename T> class Array {
public:
  /**
   * @struct Block
   * @brief Internal physical control block for contiguous memory management and reference counting.
   */
  struct Block {
    usz useCount; ///< Number of Array/Fragment instances sharing this block.
    usz capacity; ///< Total allocated capacity in elements.
    usz _length;  ///< Actually used length of the allocated memory.

    IMemoryDevice *device =
        nullptr; ///< Pointer to memory device (nullptr for CPU).
    void *deviceHandle = nullptr; ///< Opaque handle for device-resident data.

    /** @brief Returns a pointer to the start of the payload data. */
    T *get_data() {
      usz header = sizeof(Block);
      u8 *base = (u8 *)this;
      u8 *data_start = base + header;
      usz addr = (usz)data_start;
      usz align = alignof(T);
      if ((addr % align) != 0) {
        usz pad = align - (addr % align);
        data_start += pad;
      }
      return (T *)data_start;
    }

    /** @brief Allocates a new CPU-resident block. */
    static Block *allocate(usz cap) {
      usz align = alignof(T);
      if (align < alignof(Block))
        align = alignof(Block);
      usz header_size = sizeof(Block);
      usz worst_padding = align;
      usz payload_size = (cap + 1) * sizeof(T);
      usz total = header_size + worst_padding + payload_size;
      u8 *raw = (u8 *)::operator new(total);
      Block *b = (Block *)raw;
      b->useCount = 1;
      b->capacity = cap;
      b->_length = 0;
      b->device = nullptr;
      b->deviceHandle = nullptr;
      return b;
    }

    /** @brief Allocates a block for device-resident data. */
    static Block *allocateDevice(IMemoryDevice *dev, usz size) {
      usz total = sizeof(Block) + alignof(T);
      u8 *raw = (u8 *)::operator new(total);
      Block *b = (Block *)raw;
      b->useCount = 1;
      b->capacity = 0;
      b->_length = size / sizeof(T);
      b->device = dev;
      b->deviceHandle = dev->alloc(size);
      return b;
    }

    /** @brief Wraps an existing device allocation. */
    static Block *wrapDevice(IMemoryDevice *dev, void *handle, usz count) {
      usz total = sizeof(Block) + alignof(T);
      u8 *raw = (u8 *)::operator new(total);
      Block *b = (Block *)raw;
      b->useCount = 1;
      b->capacity = 0;
      b->_length = count;
      b->device = dev;
      b->deviceHandle = handle;
      return b;
    }

    /** @brief Destroys the block and its contents. */
    static void destroy(Block *b) {
      if (!b)
        return;
      if (b->device && b->deviceHandle) {
        b->device->free(b->deviceHandle);
        b->deviceHandle = nullptr;
      }
      if (!b->device) {
        T *ptr = b->get_data();
        for (usz i = 0; i < b->_length; ++i)
          ptr[i].~T();
      }
      ::operator delete(b);
    }
  };

  /**
   * @struct Fragment
   * @brief Represents a non-contiguous slice in virtual index space.
   */
  struct Fragment {
    Block *block = nullptr;
    T *data = nullptr;
    usz length = 0;
    usz offset = 0;

    bool has(usz idx) const {
      return (idx >= offset) && (idx - offset < length);
    }
  };

  Block *block;
  T *_data;    ///< Pointer to the start of this slice.
  usz _length; ///< Number of elements in this slice.
  usz offset;  ///< Global index offset.
  usz *_dims;  ///< Dimensions for multi-dimensional views.
  u8 _rank;    ///< Rank (number of dimensions).

  Fragment *_fragments;
  usz _fragmentCount;
  usz _fragmentCap;

  Array()
      : block(nullptr), _data(nullptr), _length(0), offset(0), _dims(nullptr),
        _rank(1), _fragments(nullptr), _fragmentCount(0), _fragmentCap(0) {}

#if defined(KSEE_HAS_STD_INITIALIZER_LIST)
  /** @brief Constructs an Array from an initializer list: Array<int> a = {1, 2, 3}; */
  Array(std::initializer_list<T> list)
      : block(nullptr), _data(nullptr), _length(0), offset(0), _dims(nullptr),
        _rank(1), _fragments(nullptr), _fragmentCount(0), _fragmentCap(0) {
    if (list.size() > 0) {
      allocate(list.size());
      for (const auto &item : list) {
        push(item);
      }
    }
  }
#endif

  /** @brief Variadic multi-element constructor: Array(x, y, z, ...) like [...k] */
  template <typename First, typename Second, typename... Rest,
            typename = typename Ksee::EnableIf<
                !Ksee::IsSame<typename Ksee::Decay<First>::Type, Array>::Value
            >::Type>
  Array(First &&first, Second &&second, Rest &&...rest)
      : block(nullptr), _data(nullptr), _length(0), offset(0), _dims(nullptr),
        _rank(1), _fragments(nullptr), _fragmentCount(0), _fragmentCap(0) {
    constexpr usz count = 2 + sizeof...(Rest);
    allocate(count);
    push(T(Ksee::Forward<First>(first)));
    push(T(Ksee::Forward<Second>(second)));
    (push(T(Ksee::Forward<Rest>(rest))), ...);
  }

  ~Array() {
    destroy();
    if (_dims)
      delete[] _dims;
  }

  Array(const Array &other)
      : block(nullptr), _data(nullptr), _length(0), offset(0), _dims(nullptr),
        _rank(other._rank), _fragments(nullptr), _fragmentCount(0), _fragmentCap(0) {
    if (other._dims) {
      _dims = new usz[_rank];
      for (u8 i = 0; i < _rank; i++)
        _dims[i] = other._dims[i];
    }
    if (other._fragmentCount > 0) {
      _fragmentCap = other._fragmentCount;
      _fragmentCount = other._fragmentCount;
      _fragments = new Fragment[_fragmentCap];
      for (usz i = 0; i < _fragmentCount; ++i) {
        _fragments[i] = other._fragments[i];
        if (_fragments[i].block)
          _fragments[i].block->useCount++;
      }
    } else {
      block = other.block;
      _data = other._data;
      _length = other._length;
      offset = other.offset;
      if (block)
        block->useCount++;
    }
  }

  Array(Array &&other) noexcept
      : block(other.block), _data(other._data), _length(other._length),
        offset(other.offset), _dims(other._dims), _rank(other._rank),
        _fragments(other._fragments), _fragmentCount(other._fragmentCount),
        _fragmentCap(other._fragmentCap) {
    other.block = nullptr;
    other._data = nullptr;
    other._length = 0;
    other.offset = 0;
    other._dims = nullptr;
    other._rank = 1;
    other._fragments = nullptr;
    other._fragmentCount = 0;
    other._fragmentCap = 0;
  }

  Array &operator=(const Array &other) {
    if (this == &other)
      return *this;
    if (other._fragmentCount > 0) {
      for (usz i = 0; i < other._fragmentCount; ++i) {
        if (other._fragments[i].block)
          other._fragments[i].block->useCount++;
      }
    } else if (other.block) {
      other.block->useCount++;
    }
    destroy();
    if (_dims) {
      delete[] _dims;
      _dims = nullptr;
    }
    _rank = other._rank;
    if (other._dims) {
      _dims = new usz[_rank];
      for (u8 i = 0; i < _rank; i++)
        _dims[i] = other._dims[i];
    }
    if (other._fragmentCount > 0) {
      _fragmentCap = other._fragmentCount;
      _fragmentCount = other._fragmentCount;
      _fragments = new Fragment[_fragmentCap];
      for (usz i = 0; i < _fragmentCount; ++i) {
        _fragments[i] = other._fragments[i];
      }
    } else {
      block = other.block;
      _data = other._data;
      _length = other._length;
      offset = other.offset;
    }
    return *this;
  }

  Array &operator=(Array &&other) noexcept {
    if (this == &other)
      return *this;
    destroy();
    if (_dims) {
      delete[] _dims;
      _dims = nullptr;
    }
    block = other.block;
    _data = other._data;
    _length = other._length;
    offset = other.offset;
    _dims = other._dims;
    _rank = other._rank;
    _fragments = other._fragments;
    _fragmentCount = other._fragmentCount;
    _fragmentCap = other._fragmentCap;

    other.block = nullptr;
    other._data = nullptr;
    other._length = 0;
    other.offset = 0;
    other._dims = nullptr;
    other._rank = 1;
    other._fragments = nullptr;
    other._fragmentCount = 0;
    other._fragmentCap = 0;
    return *this;
  }

private:
  void ensureFragmentCap(usz cap) {
    if (cap <= _fragmentCap)
      return;
    usz newCap = _fragmentCap == 0 ? 4 : _fragmentCap * 2;
    if (newCap < cap)
      newCap = cap;
    Fragment *nf = new Fragment[newCap];
    for (usz k = 0; k < _fragmentCount; ++k)
      nf[k] = _fragments[k];
    if (_fragments)
      delete[] _fragments;
    _fragments = nf;
    _fragmentCap = newCap;
  }

  void insertFragment(usz pos, Fragment f) {
    ensureFragmentCap(_fragmentCount + 1);
    for (usz k = _fragmentCount; k > pos; --k)
      _fragments[k] = _fragments[k - 1];
    _fragments[pos] = f;
    _fragmentCount++;
  }

  static Fragment createChunk(usz off, usz cap = KSEE_ARRAY_MIN_CAP) {
    Fragment f;
    if (cap < KSEE_ARRAY_MIN_CAP)
      cap = KSEE_ARRAY_MIN_CAP;
    f.block = Block::allocate(cap);
    f.data = f.block->get_data();
    new (&f.data[0]) T();
    f.length = 1;
    f.block->_length = 1;
    new (&f.data[1]) T();
    f.offset = off;
    return f;
  }

  void insertNewFragment(usz i) {
    usz pos = 0;
    while (pos < _fragmentCount && _fragments[pos].offset < i)
      pos++;
    Fragment chunk = createChunk(i);
    insertFragment(pos, chunk);
  }

  void pushFragmentElement(usz idx, const T &val) {
    Fragment &f = _fragments[idx];
    if (!f.block) {
      f.block = Block::allocate(KSEE_ARRAY_MIN_CAP);
      f.data = f.block->get_data();
      f.length = 0;
      new (&f.data[0]) T();
    }
    if (f.block->useCount > 1 || f.data != f.block->get_data() ||
        f.length + 1 > f.block->capacity) {
      usz old_s = f.length;
      usz new_cap = (old_s + 1) * 2;
      if (new_cap < KSEE_ARRAY_MIN_CAP)
        new_cap = KSEE_ARRAY_MIN_CAP;
      Block *nb = Block::allocate(new_cap);
      T *dst = nb->get_data();
      bool canMove = (f.block && f.block->useCount == 1);
      for (usz i = 0; i < old_s; ++i) {
        ArrayDetail::copyOrMove(&dst[i], f.data[i], canMove);
      }
      new (&dst[old_s]) T(val);
      nb->_length = old_s + 1;
      new (&dst[nb->_length]) T();
      if (f.block) {
#if defined(__GNUC__) || defined(__clang__)
        if (__atomic_fetch_sub(&f.block->useCount, 1, __ATOMIC_ACQ_REL) == 1) {
#else
        if (--f.block->useCount == 0) {
#endif
          Block::destroy(f.block);
        }
      }
      f.block = nb;
      f.data = nb->get_data();
      f.length = nb->_length;
      return;
    }
    f.data[f.length].~T();
    new (&f.data[f.length]) T(val);
    f.block->_length++;
    f.length++;
    new (&f.data[f.length]) T();
  }

  void sparsify() {
    if (_fragmentCount > 0)
      return;
    _fragmentCap = 4;
    _fragments = new Fragment[_fragmentCap];
    _fragmentCount = 0;
    if (block && _length > 0) {
      _fragments[0].block = block;
      _fragments[0].data = _data;
      _fragments[0].length = _length;
      _fragments[0].offset = offset;
      _fragmentCount = 1;
    } else if (block) {
#if defined(__GNUC__) || defined(__clang__)
      if (__atomic_fetch_sub(&block->useCount, 1, __ATOMIC_ACQ_REL) == 1) {
#else
      if (--block->useCount == 0) {
#endif
        Block::destroy(block);
      }
    }
    block = nullptr;
    _data = nullptr;
    _length = 0;
    offset = 0;
  }

  void allocateAt(usz off, usz len) {
    usz cap = len < KSEE_ARRAY_MIN_CAP ? KSEE_ARRAY_MIN_CAP : len;
    block = Block::allocate(cap);
    T *ptr = block->get_data();
    for (usz k = 0; k < len; ++k)
      new (&ptr[k]) T();
    block->_length = len;
    new (&ptr[len]) T();
    _data = ptr;
    _length = len;
    offset = off;
  }

  T &accessSparse(usz idx) {
    if (_fragmentCount == 0) {
      if (!_data && _length == 0) {
        allocateAt(idx, 1);
        return _data[0];
      }
      if (idx == offset + _length) {
        if (!block || _length + 1 > block->capacity || block->useCount > 1) {
          allocate((_length + 1) * 2);
        }
        _data[_length].~T();
        new (&_data[_length]) T();
        _length++;
        block->_length = _length;
        new (&_data[_length]) T();
        return _data[_length - 1];
      }
      if (offset > 0 && idx == offset - 1) {
        if (!block || _length + 1 > block->capacity || block->useCount > 1) {
          allocate((_length + 1) * 2);
        }
        _data[_length].~T();
        for (usz i = _length; i > 0; --i) {
          new (&_data[i]) T(Ksee::Move(_data[i - 1]));
          _data[i - 1].~T();
        }
        new (&_data[0]) T();
        _length++;
        offset--;
        block->_length = _length;
        new (&_data[_length]) T();
        return _data[0];
      }
      sparsify();
    }

    long long best_ext = -1, best_pre = -1;
    for (usz k = 0; k < _fragmentCount; ++k) {
      Fragment &f = _fragments[k];
      if (f.has(idx))
        return f.data[idx - f.offset];
      if (idx == f.offset + f.length)
        best_ext = (long long)k;
      if (f.offset > 0 && idx == f.offset - 1)
        best_pre = (long long)k;
    }

    if (best_ext != -1) {
      Fragment &f = _fragments[best_ext];
      if (!f.block || f.length + 1 > f.block->capacity || f.block->useCount > 1) {
        usz new_cap = (f.length + 1) * 2;
        if (new_cap < KSEE_ARRAY_MIN_CAP)
          new_cap = KSEE_ARRAY_MIN_CAP;
        Block *nb = Block::allocate(new_cap);
        T *dst = nb->get_data();
        bool canMove = (f.block && f.block->useCount == 1);
        for (usz i = 0; i < f.length; ++i) {
          ArrayDetail::copyOrMove(&dst[i], f.data[i], canMove);
        }
        new (&dst[f.length]) T();
        nb->_length = f.length + 1;
        new (&dst[nb->_length]) T();
        if (f.block) {
          if (--f.block->useCount == 0)
            Block::destroy(f.block);
        }
        f.block = nb;
        f.data = nb->get_data();
        f.length = nb->_length;
        return f.data[idx - f.offset];
      }
      f.data[f.length].~T();
      new (&f.data[f.length]) T();
      f.block->_length++;
      f.length++;
      new (&f.data[f.length]) T();
      return f.data[idx - f.offset];
    }

    if (best_pre != -1) {
      Fragment &f = _fragments[best_pre];
      if (!f.block || f.length + 1 > f.block->capacity || f.block->useCount > 1) {
        usz new_cap = (f.length + 1) * 2;
        if (new_cap < KSEE_ARRAY_MIN_CAP)
          new_cap = KSEE_ARRAY_MIN_CAP;
        Block *nb = Block::allocate(new_cap);
        T *dst = nb->get_data();
        new (&dst[0]) T();
        bool canMove = (f.block && f.block->useCount == 1);
        for (usz i = 0; i < f.length; ++i) {
          ArrayDetail::copyOrMove(&dst[i + 1], f.data[i], canMove);
        }
        nb->_length = f.length + 1;
        new (&dst[nb->_length]) T();
        if (f.block) {
          if (--f.block->useCount == 0)
            Block::destroy(f.block);
        }
        f.block = nb;
        f.data = nb->get_data();
        f.length = nb->_length;
        f.offset--;
        return f.data[0];
      }
      f.data[f.length].~T();
      for (usz i = f.length; i > 0; --i) {
        new (&f.data[i]) T(Ksee::Move(f.data[i - 1]));
        f.data[i - 1].~T();
      }
      new (&f.data[0]) T();
      f.block->_length++;
      f.length++;
      f.offset--;
      new (&f.data[f.length]) T();
      return f.data[0];
    }

    insertNewFragment(idx);
    for (usz k = 0; k < _fragmentCount; ++k) {
      if (_fragments[k].has(idx))
        return _fragments[k].data[idx - _fragments[k].offset];
    }
    static T fallback{};
    return fallback;
  }

  void pushDense(const T &val) {
    if (!block) {
      block = Block::allocate(KSEE_ARRAY_MIN_CAP);
      _data = block->get_data();
      _length = 0;
      offset = 0;
      new (&_data[0]) T();
    }
    if (block->useCount > 1 || _data != block->get_data() ||
        (_data + _length) != (block->get_data() + block->_length)) {
      usz old_s = _length;
      usz new_cap = (old_s + 1) * 2;
      if (new_cap < KSEE_ARRAY_MIN_CAP)
        new_cap = KSEE_ARRAY_MIN_CAP;
      Block *nb = Block::allocate(new_cap);
      T *dst = nb->get_data();
      bool canMove = (block->useCount == 1);
      for (usz i = 0; i < old_s; ++i) {
        ArrayDetail::copyOrMove(&dst[i], _data[i], canMove);
      }
      new (&dst[old_s]) T(val);
      nb->_length = old_s + 1;
      new (&dst[nb->_length]) T();
      destroy();
      block = nb;
      _data = nb->get_data();
      _length = nb->_length;
      return;
    }
    if (block->_length + 1 > block->capacity) {
      usz new_cap = block->capacity * 2;
      Block *old = block;
      Block *nb = Block::allocate(new_cap);
      T *dst = nb->get_data();
      T *src = _data;
      for (usz i = 0; i < _length; ++i)
        new (&dst[i]) T(Ksee::Move(src[i]));
      new (&dst[_length]) T(val);
      nb->_length = _length + 1;
      new (&dst[nb->_length]) T();
      Block::destroy(old);
      block = nb;
      _data = nb->get_data();
      _length = nb->_length;
    } else {
      _data[_length].~T();
      new (&_data[_length]) T(val);
      block->_length++;
      _length++;
      new (&_data[_length]) T();
    }
  }

public:
  /** @brief Flattens all sparse fragments into a single contiguous block of memory. */
  void flatten() {
    if (_fragmentCount == 0)
      return;
    if (_fragmentCount == 1 && _fragments[0].offset == 0) {
      block = _fragments[0].block;
      _data = _fragments[0].data;
      _length = _fragments[0].length;
      offset = 0;
      delete[] _fragments;
      _fragments = nullptr;
      _fragmentCount = 0;
      _fragmentCap = 0;
      return;
    }

    usz totalLen = _fragments[_fragmentCount - 1].offset + _fragments[_fragmentCount - 1].length;
    Block *nb = Block::allocate(totalLen);
    T *dst = nb->get_data();

    for (usz i = 0; i < totalLen; ++i)
      new (&dst[i]) T();

    for (usz i = 0; i < _fragmentCount; ++i) {
      Fragment &f = _fragments[i];
      for (usz k = 0; k < f.length; ++k) {
        dst[f.offset + k] = Ksee::Move(f.data[k]);
      }
      if (f.block) {
#if defined(__GNUC__) || defined(__clang__)
        if (__atomic_fetch_sub(&f.block->useCount, 1, __ATOMIC_ACQ_REL) == 1) {
#else
        if (--f.block->useCount == 0) {
#endif
          Block::destroy(f.block);
        }
      }
    }

    nb->_length = totalLen;
    new (&dst[totalLen]) T();

    delete[] _fragments;
    _fragments = nullptr;
    _fragmentCount = 0;
    _fragmentCap = 0;

    block = nb;
    _data = nb->get_data();
    _length = totalLen;
    offset = 0;
  }

  /** @brief Configures the multi-dimensional shape of the array. */
  Array &shape(u8 rank, const usz *d) {
    if (_dims) {
      delete[] _dims;
      _dims = nullptr;
    }
    _rank = (rank > 0 ? rank : 1);
    if (d && _rank > 1) {
      _dims = new usz[_rank];
      for (u8 i = 0; i < _rank; i++)
        _dims[i] = d[i];
    }
    return *this;
  }

  template <typename First, typename Second, typename... Rest,
            typename = typename Ksee::EnableIf<
                Ksee::IsIntegral<First>::Value &&
                Ksee::IsIntegral<Second>::Value
            >::Type>
  Array &shape(First first, Second second, Rest... rest) {
    usz d[] = {(usz)first, (usz)second, (usz)rest...};
    return shape((u8)(2 + sizeof...(Rest)), (const usz*)d);
  }

  /** @brief Transposes multidimensional tensor by reversing its shape dimensions. */
  Array &transpose() {
    if (_rank <= 1 || !_dims)
      return *this;
    usz *new_d = new usz[_rank];
    for (u8 i = 0; i < _rank; ++i)
      new_d[i] = _dims[_rank - 1 - i];
    shape(_rank, new_d);
    delete[] new_d;
    return *this;
  }

  u8 rank() const { return _rank; }
  const usz *dimensions() const { return _dims; }

  /** @brief Increments reference count. */
  void retain() {
    if (_fragmentCount > 0) {
      for (usz i = 0; i < _fragmentCount; ++i) {
        if (_fragments[i].block)
          _fragments[i].block->useCount++;
      }
    } else if (block) {
      block->useCount++;
    }
  }

  /** @brief Decrements reference count and destroys internal storage if zero. */
  void destroy() {
    if (_fragmentCount > 0) {
      for (usz i = 0; i < _fragmentCount; ++i) {
        if (_fragments[i].block) {
#if defined(__GNUC__) || defined(__clang__)
          if (__atomic_fetch_sub(&_fragments[i].block->useCount, 1, __ATOMIC_ACQ_REL) == 1) {
#else
          if (--_fragments[i].block->useCount == 0) {
#endif
            Block::destroy(_fragments[i].block);
          }
        }
      }
      delete[] _fragments;
      _fragments = nullptr;
      _fragmentCount = 0;
      _fragmentCap = 0;
    }
    if (block) {
#if defined(__GNUC__) || defined(__clang__)
      if (__atomic_fetch_sub(&block->useCount, 1, __ATOMIC_ACQ_REL) == 1) {
#else
      if (--block->useCount == 0) {
#endif
        Block::destroy(block);
      }
      block = nullptr;
    }
    _data = nullptr;
    _length = 0;
    offset = 0;
  }

  /** @brief Clears the array. */
  void clear() { destroy(); }

  /** @brief Ensures capacity for at least cap elements without altering length. */
  bool allocate(usz cap) {
    if (_fragmentCount > 0) {
      Fragment &last = _fragments[_fragmentCount - 1];
      if (last.block && cap <= last.block->capacity)
        return true;
      Block *nb = Block::allocate(cap);
      if (!nb)
        return false;
      if (last.block) {
        T *src = last.data;
        T *dst = nb->get_data();
        usz toCopy = last.length < cap ? last.length : cap;
        bool canMove = (last.block->useCount == 1);
        for (usz i = 0; i < toCopy; ++i) {
          ArrayDetail::copyOrMove(&dst[i], src[i], canMove);
        }
        nb->_length = toCopy;
        if (--last.block->useCount == 0)
          Block::destroy(last.block);
      }
      T *dst = nb->get_data();
      new (&dst[nb->_length]) T();
      last.block = nb;
      last.data = nb->get_data();
      return true;
    }
    if (block && cap <= block->capacity)
      return true;
    Block *nb = Block::allocate(cap);
    if (!nb)
      return false;
    if (block) {
      T *src = _data;
      T *dst = nb->get_data();
      usz toCopy = _length < cap ? _length : cap;
      bool canMove = (block->useCount == 1);
      for (usz i = 0; i < toCopy; i++) {
        ArrayDetail::copyOrMove(&dst[i], src[i], canMove);
      }
      nb->_length = toCopy;
      destroy();
    }
    T *dst = nb->get_data();
    new (&dst[nb->_length]) T();
    block = nb;
    _data = block->get_data();
    _length = block->_length;
    return true;
  }

  T *data() {
    if (_fragmentCount > 0)
      flatten();
    return _data;
  }
  const T *data() const {
    if (_fragmentCount > 0)
      const_cast<Array<T>*>(this)->flatten();
    return _data;
  }

  usz size() const {
    if (_fragmentCount > 0) {
      const Fragment &last = _fragments[_fragmentCount - 1];
      return last.offset + last.length;
    }
    return _length;
  }
  usz length() const { return size(); }
  bool isEmpty() const { return size() == 0; }
  void remove(usz idx) { splice(idx, 1); }

  usz fragmentCount() const {
    return _fragmentCount > 0 ? _fragmentCount : (block ? 1 : 0);
  }

  T &operator[](usz idx) {
    if (_fragmentCount == 0) {
      if (_data && idx < _length) {
        return _data[idx];
      }
    }
    return accessSparse(idx);
  }

  const T &operator[](usz idx) const {
    if (_fragmentCount == 0) {
      if (_data && idx < _length) {
        return _data[idx];
      }
      static T dummy{};
      return dummy;
    }
    for (usz k = 0; k < _fragmentCount; ++k) {
      if (_fragments[k].has(idx))
        return _fragments[k].data[idx - _fragments[k].offset];
    }
    static T dummy{};
    return dummy;
  }

  bool has(usz idx) const {
    if (_fragmentCount > 0) {
      for (usz k = 0; k < _fragmentCount; ++k) {
        if (_fragments[k].has(idx))
          return true;
      }
      return false;
    }
    if (idx < offset)
      return false;
    return (idx - offset) < _length;
  }

  long long find(const T &val) const { return indexOf(val); }

  long long indexOf(const T &val) const {
    if (_fragmentCount > 0) {
      for (usz k = 0; k < _fragmentCount; ++k) {
        const Fragment &f = _fragments[k];
        for (usz i = 0; i < f.length; ++i) {
          if (Equal<T>::eq(f.data[i], val))
            return (long long)(f.offset + i);
        }
      }
      return -1;
    }
    for (usz i = 0; i < _length; ++i) {
      if (Equal<T>::eq(_data[i], val))
        return (long long)(offset + i);
    }
    return -1;
  }

  void push(const T &val) {
    if (_fragmentCount > 0) {
      pushFragmentElement(_fragmentCount - 1, val);
    } else {
      pushDense(val);
    }
  }

  void pushEach(const T *vals, usz count) {
    if (!vals || count == 0) return;
    if (_fragmentCount > 0) {
      for (usz i = 0; i < count; ++i)
        push(vals[i]);
      return;
    }
    if (!block) {
      usz cap = count < KSEE_ARRAY_MIN_CAP ? KSEE_ARRAY_MIN_CAP : count;
      block = Block::allocate(cap);
      _data = block->get_data();
      _length = 0;
      offset = 0;
      new (&_data[0]) T();
    }
    if (block->useCount > 1 || _data != block->get_data() ||
        (_data + _length) != (block->get_data() + block->_length) ||
        (block->_length + count > block->capacity)) {
      usz old_s = _length;
      usz new_cap = old_s + count;
      if (block && block->capacity < (1ULL << 30) && new_cap < block->capacity * 2) new_cap = block->capacity * 2;
      if (new_cap < KSEE_ARRAY_MIN_CAP) new_cap = KSEE_ARRAY_MIN_CAP;
      Block *nb = Block::allocate(new_cap);
      T *dst = nb->get_data();
      bool canMove = (block && block->useCount == 1);
      for (usz i = 0; i < old_s; ++i) {
        ArrayDetail::copyOrMove(&dst[i], _data[i], canMove);
      }
      for (usz i = 0; i < count; ++i)
        new (&dst[old_s + i]) T(vals[i]);
      nb->_length = old_s + count;
      new (&dst[nb->_length]) T();
      destroy();
      block = nb;
      _data = nb->get_data();
      _length = nb->_length;
      return;
    }
    for (usz i = 0; i < count; ++i) {
      new (&_data[_length + i]) T(vals[i]);
    }
    _length += count;
    block->_length = _length;
    new (&_data[_length]) T();
  }

  void set(const T *vals, usz count) {
    destroy();
    if (count > 0 && vals)
      pushEach(vals, count);
  }

  T pop() {
    if (size() == 0)
      return T();
    if (_fragmentCount > 0) {
      Fragment &last = _fragments[_fragmentCount - 1];
      T ret = Ksee::Move(last.data[last.length - 1]);
      last.data[last.length - 1].~T();
      last.length--;
      last.block->_length--;
      if (last.length == 0) {
        if (last.block) {
          if (--last.block->useCount == 0)
            Block::destroy(last.block);
        }
        _fragmentCount--;
        if (_fragmentCount == 0) {
          delete[] _fragments;
          _fragments = nullptr;
          _fragmentCap = 0;
        }
      }
      return ret;
    }
    if (block->useCount > 1 || _data != block->get_data()) {
      usz old_s = _length;
      T ret = _data[old_s - 1];
      Block *nb = Block::allocate(old_s - 1);
      T *dst = nb->get_data();
      bool canMove = (block && block->useCount == 1);
      for (usz i = 0; i < old_s - 1; ++i) {
        ArrayDetail::copyOrMove(&dst[i], _data[i], canMove);
      }
      nb->_length = old_s - 1;
      new (&dst[nb->_length]) T();
      destroy();
      block = nb;
      _data = nb->get_data();
      _length = nb->_length;
      return ret;
    }
    T ret = Ksee::Move(_data[_length - 1]);
    _data[_length - 1].~T();
    _length--;
    block->_length--;
    new (&_data[_length]) T();
    return ret;
  }

  void unshift(const T &val) {
    if (_fragmentCount > 0) {
      Fragment &f = _fragments[0];
      if (f.offset > 0) {
        pushFragmentElement(0, val);
        f.offset--;
      } else {
        pushFragmentElement(0, val);
      }
      for (usz i = 1; i < _fragmentCount; ++i)
        _fragments[i].offset++;
      return;
    }
    if (block && (block->useCount > 1 || _data != block->get_data())) {
      usz old_s = _length;
      Block *nb = Block::allocate(old_s + 1);
      T *dst = nb->get_data();
      new (&dst[0]) T(val);
      bool canMove = (block->useCount == 1);
      for (usz i = 0; i < old_s; ++i) {
        ArrayDetail::copyOrMove(&dst[i + 1], _data[i], canMove);
      }
      nb->_length = old_s + 1;
      new (&dst[nb->_length]) T();
      destroy();
      block = nb;
      _data = nb->get_data();
      _length = nb->_length;
      return;
    }
    if (!block) {
      push(val);
      return;
    }
    if (block->_length < block->capacity) {
      _data[_length].~T();
      for (usz i = _length; i > 0; --i) {
        new (&_data[i]) T(Ksee::Move(_data[i - 1]));
        _data[i - 1].~T();
      }
      new (&_data[0]) T(val);
      block->_length++;
      _length++;
      new (&_data[_length]) T();
    } else {
      usz new_cap = block->capacity * 2;
      Block *old = block;
      Block *nb = Block::allocate(new_cap);
      T *dst = nb->get_data();
      T *src = _data;
      new (&dst[0]) T(val);
      for (usz i = 0; i < _length; ++i)
        new (&dst[i + 1]) T(Ksee::Move(src[i]));
      nb->_length = _length + 1;
      new (&dst[nb->_length]) T();
      Block::destroy(old);
      block = nb;
      _data = nb->get_data();
      _length = nb->_length;
    }
  }

  T shift() {
    if (size() == 0)
      return T();
    if (_fragmentCount > 0) {
      Fragment &f = _fragments[0];
      T ret = Ksee::Move(f.data[0]);
      f.data[0].~T();
      f.data++;
      f.length--;
      f.offset++;
      if (f.length == 0) {
        if (f.block) {
          if (--f.block->useCount == 0)
            Block::destroy(f.block);
        }
        for (usz i = 0; i < _fragmentCount - 1; ++i)
          _fragments[i] = _fragments[i + 1];
        _fragmentCount--;
        if (_fragmentCount == 0) {
          delete[] _fragments;
          _fragments = nullptr;
          _fragmentCap = 0;
        }
      }
      for (usz i = 0; i < _fragmentCount; ++i) {
        if (_fragments[i].offset > 0)
          _fragments[i].offset--;
      }
      return ret;
    }
    if (block->useCount > 1) {
      usz old_s = _length;
      T ret = _data[0];
      Block *nb = Block::allocate(old_s - 1);
      T *dst = nb->get_data();
      bool canMove = (block->useCount == 1);
      for (usz i = 1; i < old_s; ++i) {
        ArrayDetail::copyOrMove(&dst[i - 1], _data[i], canMove);
      }
      nb->_length = old_s - 1;
      new (&dst[nb->_length]) T();
      destroy();
      block = nb;
      _data = nb->get_data();
      _length = nb->_length;
      return ret;
    }
    T ret = Ksee::Move(_data[0]);
    _data[0].~T();
    new (&_data[0]) T();
    _data++;
    _length--;
    offset++;
    return ret;
  }

  /** @brief Forces a fragment boundary at the specified index. */
  void break_at(usz at) {
    if (_fragmentCount == 0) {
      if (at > offset && at < offset + _length) {
        sparsify();
      } else {
        return;
      }
    }
    for (long long i = 0; i < (long long)_fragmentCount; ++i) {
      Fragment &f = _fragments[i];
      usz start = f.offset;
      usz end = start + f.length;

      if (at > start && at < end) {
        usz rel = at - start;
        usz suff_len = f.length - rel;

        Fragment suff;
        suff.block = Block::allocate(suff_len < KSEE_ARRAY_MIN_CAP ? KSEE_ARRAY_MIN_CAP : suff_len);
        suff.data = suff.block->get_data();
        for (usz k = 0; k < suff_len; ++k)
          new (&suff.data[k]) T(Ksee::Move(f.data[rel + k]));
        suff.length = suff_len;
        suff.block->_length = suff_len;
        new (&suff.data[suff_len]) T();
        suff.offset = at;

        for (usz k = rel; k < f.length; ++k)
          f.data[k].~T();
        f.length = rel;
        f.block->_length = rel;
        new (&f.data[rel]) T();

        insertFragment((usz)i + 1, suff);
        return;
      }
    }
  }

  Array begin(usz start) const {
    if (start >= size())
      return Array();
    if (_fragmentCount == 0) {
      Array sub;
      sub.block = block;
      if (block)
        block->useCount++;
      sub._data = _data + start;
      sub._length = _length - start;
      sub.offset = offset + start;
      return sub;
    }
    Array copy;
    usz len = size() - start;
    copy.allocate(len);
    for (usz i = 0; i < len; ++i)
      copy.push((*this)[start + i]);
    copy.offset = offset + start;
    return copy;
  }

  T *begin() { return data(); }
  const T *begin() const { return data(); }
  T *end() { return data() ? data() + size() : nullptr; }
  const T *end() const { return data() ? data() + size() : nullptr; }

  Array begin(usz start, usz end) const {
    if (start >= size())
      return Array();
    usz avail = size() - start;
    usz len = (end - start);
    if (len > avail)
      len = avail;
    Array copy;
    copy.allocate(len);
    for (usz i = 0; i < len; ++i)
      copy.push((*this)[start + i]);
    copy.offset = 0;
    return copy;
  }

  template <int Rank> struct ViewProxy {
    T *data_ptr;
    const usz *strides_ptr;
    template <int R = Rank, typename Ksee::EnableIf<(R > 1), int>::Type = 0>
    ViewProxy<Rank - 1> operator[](usz i) const {
      return ViewProxy<Rank - 1>{data_ptr + i * strides_ptr[0],
                                 strides_ptr + 1};
    }
    template <int R = Rank, typename Ksee::EnableIf<(R == 1), int>::Type = 0>
    T &operator[](usz i) const {
      return data_ptr[i * strides_ptr[0]];
    }
  };

  template <int Rank> struct ViewContainer {
    Array<T> *arr;
    usz dims[Rank];
    usz strides[Rank];
    template <typename S = String> S serialize() const {
      S s;
      s.pushVarLong(0);
      s.pushVarLong((long long)Rank);
      for (int i = 0; i < Rank; ++i)
        s.pushVarLong((long long)dims[i]);
      s.pushVarLong((long long)arr->offset);
      for (usz i = 0; i < arr->size(); ++i)
        s += Ksee::serialize<T>((*arr)[i]);
      return s;
    }
    auto operator[](usz i) -> decltype(arr->operator[](i * strides[0])) {
      if constexpr (Rank > 1)
        return ViewProxy<Rank - 1>{arr->data() + i * strides[0], strides + 1};
      else
        return (*arr)[i * strides[0]];
    }
  };

  template <typename... Args>
  auto view(Args... args) -> ViewContainer<sizeof...(Args)> {
    constexpr int Rank = sizeof...(Args);
    ViewContainer<Rank> v;
    v.arr = this;
    usz d[] = {(usz)args...};
    usz current = 1;
    for (int i = Rank - 1; i >= 0; --i) {
      v.dims[i] = d[i];
      v.strides[i] = current;
      current *= d[i];
    }
    return v;
  }

  template <typename S = String> S serialize() const {
    S s;
    if (offset == 0 && _rank <= 1) {
      s.pushVarLong((long long)size() + 1);
    } else {
      s.pushVarLong(0);
      s.pushVarLong((long long)_rank);
      for (u8 i = 0; i < _rank; ++i)
        s.pushVarLong((long long)(_dims ? _dims[i] : size()));
      s.pushVarLong((long long)offset);
    }
    for (usz i = 0; i < size(); ++i)
      s += Ksee::serialize<T>((*this)[i]);
    return s;
  }

  template <typename S = String> static Array<T> deserialize(const S &s) {
    usz at = 0;
    return deserialize(s, at);
  }

  template <typename S = String>
  static Array<T> deserialize(const S &s, usz &at) {
    Array<T> res;
    auto headerRes = s.peekVarLong(at);
    if (headerRes.error)
      return res;
    at += headerRes.bytes;
    if (headerRes.value > 0) {
      usz size = (usz)(headerRes.value - 1);
      res.allocate(size);
      for (usz i = 0; i < size; ++i)
        res.push(Ksee::deserialize<T, S>(s, at));
    } else {
      auto rankRes = s.peekVarLong(at);
      if (rankRes.error)
        return res;
      at += rankRes.bytes;
      u8 rank = (u8)rankRes.value;
      res._rank = rank;
      res._dims = new usz[rank];
      usz total = 1;
      for (u8 i = 0; i < rank; i++) {
        auto d = s.peekVarLong(at);
        at += d.bytes;
        res._dims[i] = (usz)d.value;
        total *= (usz)d.value;
      }
      auto offsetRes = s.peekVarLong(at);
      if (offsetRes.error)
        return res;
      at += offsetRes.bytes;
      res.allocate(total);
      res.offset = (usz)offsetRes.value;
      for (usz i = 0; i < total; ++i)
        res.push(Ksee::deserialize<T, S>(s, at));
    }
    return res;
  }

  /**
   * @brief Removes a range of elements and collapses the gap.
   */
  void splice(usz start, usz length) {
    if (length == 0)
      return;
    if (_fragmentCount == 0) {
      if (start >= _length)
        return;
      if (start + length > _length)
        length = _length - start;

      if (block && block->useCount > 1) {
        allocate(_length);
      }

      T *ptr = _data;
      for (usz i = start + length; i < _length; ++i) {
        ptr[i - length] = Ksee::Move(ptr[i]);
      }
      for (usz i = _length - length; i <= _length; ++i) {
        ptr[i].~T();
      }
      _length -= length;
      if (block)
        block->_length = _length;
      new (&ptr[_length]) T();
      return;
    }

    usz end = start + length;
    usz newCount = 0;
    Fragment *newFrags = new Fragment[_fragmentCap];

    for (usz i = 0; i < _fragmentCount; ++i) {
      Fragment &f = _fragments[i];
      usz f_start = f.offset;
      usz f_end = f_start + f.length;

      if (f_end <= start) {
        newFrags[newCount++] = f;
      } else if (f_start >= end) {
        f.offset -= length;
        newFrags[newCount++] = f;
      } else {
        if (f_start < start) {
          usz p1_len = start - f_start;
          Fragment p1 = createChunk(f_start, p1_len);
          p1.length = p1_len;
          p1.block->_length = p1_len;
          for (usz k = 0; k < p1_len; ++k)
            new (&p1.data[k]) T(Ksee::Move(f.data[k]));
          newFrags[newCount++] = p1;
        }
        if (f_end > end) {
          usz p2_len = f_end - end;
          Fragment p2 = createChunk(start, p2_len);
          p2.length = p2_len;
          p2.block->_length = p2_len;
          for (usz k = 0; k < p2_len; ++k)
            new (&p2.data[k]) T(Ksee::Move(f.data[(end - f_start) + k]));
          newFrags[newCount++] = p2;
        }
        if (f.block) {
          if (--f.block->useCount == 0)
            Block::destroy(f.block);
        }
      }
    }
    delete[] _fragments;
    _fragments = newFrags;
    _fragmentCount = newCount;
  }

  IMemoryDevice *getDevice() const { return block ? block->device : nullptr; }

  void *deviceView(i32 type = 0) const {
    if (_fragmentCount > 0)
      const_cast<Array<T>*>(this)->flatten();
    if (!block || !block->device)
      return nullptr;
    return block->device->view(block->deviceHandle, type);
  }

  void wrapDevice(IMemoryDevice *dev, void *handle, usz count) {
    destroy();
    block = Block::wrapDevice(dev, handle, count);
    _data = nullptr;
    _length = count;
    offset = 0;
  }

  Array<T> to() const {
    if (_fragmentCount > 0)
      const_cast<Array<T>*>(this)->flatten();
    if (!block)
      return Array<T>();
    if (!block->device) {
      Array<T> res;
      res.allocate(size());
      if (_length > 0) {
        for (usz i = 0; i < _length; ++i)
          res.push(_data[i]);
      }
      res.offset = offset;
      res._rank = _rank;
      if (_dims) {
        res._dims = new usz[_rank];
        for (u8 i = 0; i < _rank; i++)
          res._dims[i] = _dims[i];
      }
      return res;
    }
    Array<T> cpu;
    usz count = block->_length;
    if (count == 0)
      return cpu;
    cpu.allocate(count);
    for (usz i = 0; i < count; ++i)
      cpu.push(T());
    block->device->download(block->deviceHandle, cpu.data(), count * sizeof(T));
    cpu.offset = offset;
    cpu._rank = _rank;
    if (_dims) {
      cpu._dims = new usz[_rank];
      for (u8 i = 0; i < _rank; i++)
        cpu._dims[i] = _dims[i];
    }
    return cpu;
  }

  Array<T> to(IMemoryDevice *dev) const {
    if (!dev)
      return to();
    Array<T> src = to();
    usz count = src.size();
    if (count == 0)
      return Array<T>();
    usz byteSize = count * sizeof(T);
    Array<T> result;
    result.destroy();
    result.block = Block::allocateDevice(dev, byteSize);
    result._data = nullptr;
    result._length = count;
    result.offset = src.offset;
    result._rank = src._rank;
    if (src._dims) {
      result._dims = new usz[src._rank];
      for (u8 i = 0; i < src._rank; i++)
        result._dims[i] = src._dims[i];
    }
    dev->upload(result.block->deviceHandle, src.data(), byteSize);
    return result;
  }

  bool includes(const T& val) const {
    if (_fragmentCount > 0) {
      for (usz k = 0; k < _fragmentCount; ++k) {
        const Fragment &f = _fragments[k];
        for (usz i = 0; i < f.length; ++i) {
          if (Equal<T>::eq(f.data[i], val))
            return true;
        }
      }
      return false;
    }
    for (usz i = 0; i < _length; ++i) {
      if (Equal<T>::eq(_data[i], val))
        return true;
    }
    return false;
  }

  Array<T> intersect(const Array<T>& o) const {
    Array<T> res;
    for (usz i = 0; i < size(); ++i) {
      T val = ((Array<T>*)this)->operator[](i);
      if (o.includes(val) && !res.includes(val)) {
        res.push(val);
      }
    }
    return res;
  }

  Array<T> uni(const Array<T>& o) const {
    Array<T> res;
    for (usz i = 0; i < size(); ++i) {
      T val = ((Array<T>*)this)->operator[](i);
      if (!res.includes(val)) res.push(val);
    }
    for (usz i = 0; i < o.size(); ++i) {
      T val = ((Array<T>&)o)[i];
      if (!res.includes(val)) res.push(val);
    }
    return res;
  }

  Array<T> difference(const Array<T>& o) const {
    Array<T> res;
    for (usz i = 0; i < size(); ++i) {
      T val = ((Array<T>*)this)->operator[](i);
      if (!o.includes(val) && !res.includes(val)) {
        res.push(val);
      }
    }
    return res;
  }

  Array<T> operator&(const Array<T>& o) const { return intersect(o); }
  Array<T> operator|(const Array<T>& o) const { return uni(o); }
  Array<T> operator-(const Array<T>& o) const { return difference(o); }

  bool equals(const Array<T> &other) const {
    if (this == &other) return true;
    usz n = size();
    if (n != other.size()) return false;
    if (n == 0) return true;

    if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T> || std::is_pointer_v<T>) {
      if (_fragmentCount == 0 && other._fragmentCount == 0) {
        return memcmp(_data, other._data, n * sizeof(T)) == 0;
      }
    }

    for (usz i = 0; i < n; ++i) {
      if (!((*this)[i] == other[i])) {
        return false;
      }
    }
    return true;
  }

  bool operator==(const Array<T> &other) const { return equals(other); }
  bool operator!=(const Array<T> &other) const { return !equals(other); }

  bool constantTimeEquals(const Array<T> &other) const {
    usz n = size();
    usz otherN = other.size();
    usz maxLen = n > otherN ? n : otherN;
    volatile u8 diff = (n == otherN) ? 0 : 1;

    for (usz i = 0; i < maxLen; ++i) {
      usz idxA = i < n ? i : 0;
      usz idxB = i < otherN ? i : 0;
      if constexpr (std::is_integral_v<T>) {
        diff |= (u8)((*this)[idxA] ^ other[idxB]);
      } else {
        diff |= ((*this)[idxA] == other[idxB]) ? 0 : 1;
      }
    }
    return diff == 0;
  }
};

#define XI_INLINE_ARRAY_BIN_OP(op)                                             \
  template <typename T>                                                        \
  Array<T> operator op(const Array<T> &a,                                      \
                       const Array<T> &b) {                                    \
    usz n = a.size() < b.size() ? a.size() : b.size();                         \
    Array<T> res;                                                              \
    res.allocate(n);                                                           \
    const T *d_a = a.data();                                                   \
    const T *d_b = b.data();                                                   \
    for (usz i = 0; i < n; ++i)                                                \
      res.push(d_a[i] op d_b[i]);                                              \
    return res;                                                                \
  }                                                                            \
  template <typename T>                                                        \
  Array<T> operator op(const Array<T> &a, const T &b) {                        \
    usz n = a.size();                                                          \
    Array<T> res;                                                              \
    res.allocate(n);                                                           \
    const T *d_a = a.data();                                                   \
    for (usz i = 0; i < n; ++i)                                                \
      res.push(d_a[i] op b);                                                   \
    return res;                                                                \
  }                                                                            \
  template <typename T>                                                        \
  Array<T> operator op(const T &a, const Array<T> &b) {                        \
    usz n = b.size();                                                          \
    Array<T> res;                                                              \
    res.allocate(n);                                                           \
    const T *d_b = b.data();                                                   \
    for (usz i = 0; i < n; ++i)                                                \
      res.push(a op d_b[i]);                                                   \
    return res;                                                                \
  }

XI_INLINE_ARRAY_BIN_OP(+)
XI_INLINE_ARRAY_BIN_OP(-)
XI_INLINE_ARRAY_BIN_OP(*)
XI_INLINE_ARRAY_BIN_OP(/)

#if defined(KSEE_HAS_STD_INITIALIZER_LIST)
template <typename T>
Array(std::initializer_list<T>) -> Array<T>;
#endif

template <typename First, typename... Rest>
Array(First, Rest...) -> Array<typename Ksee::Decay<First>::Type>;

} // namespace Ksee

#endif // KSEE_ARRAY_HPP
