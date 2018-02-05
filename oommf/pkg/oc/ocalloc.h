/* FILE: ocalloc.h                   -*-Mode: c++-*-
 *
 *      Custom memory allocation
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2015/03/09 04:51:08 $
 * Last modified by: $Author: donahue $
 *
 */

#ifndef _OCALLOC
#define _OCALLOC

#include <new>
#include <vector>

/* End includes */     /* Optional directive to pimake */


// Template wrapper for std::vector using above aligned allocator.  The
// generic code assumes the template class T has an Alignment() static member
// that returns the alignment requirement for the class (should be a
// power of 2).  Provided also are overrides for some base variable
// types that fall back to std::vector with the standard allocator.
// These are provided so generic code can transparently use
// Oc_AlignedVector with base types.
template <class T> size_t Oc_Alignment() { return T::Alignment(); }
template <> inline size_t Oc_Alignment<float>()        { return 8; }
template <> inline size_t Oc_Alignment<double>()       { return 8; }
template <> inline size_t Oc_Alignment<long double>()  { return 8; }

/*
 * Class to supply aligned memory allocations to C++ standard containers
 * (std::vector, etc.).  Parameter ALIGN should be a power-of-two.  There is
 * sufficient memory allocated in front of the returned pointer to store
 * a pointer to the start of the system-provided memory block.
 *    Much of the boilerplate below can be dropped if C++11
 * std::allocator_traits is available.
 * 
 */
template <class T>
class Oc_AlignedAlloc {
 public:
  typedef T value_type;
  typedef T& reference;
  typedef const T& const_reference;
  typedef T* pointer;
  typedef const T* const_pointer;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  template <class U> struct rebind {
    typedef Oc_AlignedAlloc<U> other;
  };

  pointer address(reference ref) { return &ref; }
  const_pointer address(const_reference ref) const { return &ref; }
  size_type max_size() const throw() { return size_type(-1)/sizeof(T) - 2; }

  bool operator==(const Oc_AlignedAlloc&) { return true; }
  bool operator!=(const Oc_AlignedAlloc&) { return false; }

  Oc_AlignedAlloc() noexcept {}
  template <class U>
  Oc_AlignedAlloc(const Oc_AlignedAlloc<U>&) noexcept {}

  void construct(pointer ptr,const_reference val) {
    new(ptr) value_type(val); // Placement new
  }
  void destroy(pointer ptr) {
    value_type obj = *ptr;
    obj.~value_type(); // Run T destructor w/o releasing memory
  }

  T* allocate(size_t N) {
    size_t alignment = Oc_Alignment<T>();
    if(alignment<sizeof(void*)) alignment = sizeof(void*); // Insure
    /// requested alignment satisfies alignment requirements for void*.

    size_t datasize = N * sizeof(T);
    size_t blocksize = datasize + alignment + sizeof(void*) - 1;
    void* sysptr = operator new(blocksize);
    size_t offset = ((reinterpret_cast<size_t>(sysptr) + sizeof(void*) - 1)
                     & ~(alignment-1)) + alignment;
    T* ptr = reinterpret_cast<T*>(offset);
    *(reinterpret_cast<void**>(offset)-1) = sysptr;

    /// If the pointer returned by "operator new" is known to be
    /// sufficiently aligned for void* (likely to be true), then
    /// blocksize can be reduced to datasize + alignment, and the offset
    /// computation is simplified to
    ///    size_t offset
    ///     = (reinterpret_cast<size_t>(sysptr) & ~(alignment-1)) + alignment

    return ptr;
  }

  void deallocate(T* ptr, size_t /* N */) {
    operator delete(*(reinterpret_cast<void**>(ptr)-1));
  }

};


// If the C++ compiler supports template aliases (aka type alias), then
// the following is valid:
//
//  template <class T>
//     using Oc_AlignedVector = std::vector<T,Oc_AlignedAlloc<T> >;
//
// Then
//
//  Oc_AlignedVector<Nb_Xpfloat> foo;
//
// is equivalent to
//
//  std::vector<Nb_Xpfloat,Oc_AlignedAlloc<Nb_Xpfloat> > foo;
//
// Type alias support first appears in g++ 4.7, Visual C++ 12.0, and
// Clang 3.0.

template <class T>
using Oc_AlignedVector = std::vector<T,Oc_AlignedAlloc<T> >;

#endif // _OCALLOC
