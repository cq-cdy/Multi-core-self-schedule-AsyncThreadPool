//
// Created by 10447 on 2023-09-06.
//

#ifndef THREAD_POOL_MYALLOCATOR_H
#define THREAD_POOL_MYALLOCATOR_H
#include <cstddef>
#include <cstring>
#include <mutex>
#include <string>
template <int __inst>
class __malloc_alloc_template {
private:
    static void* _S_oom_malloc(size_t);
    static void* _S_oom_realloc(void*, size_t);

#ifndef __STL_STATIC_TEMPLATE_MEMBER_BUG
    static void (*__malloc_alloc_oom_handler)();
#endif
public:
    static void* allocate(size_t __n) {
        void* __result = malloc(__n);
        if (nullptr == __result) __result = _S_oom_malloc(__n);
        return __result;
    }

    static void deallocate(void* __p, size_t /* __n */) { free(__p); }

    static void* reallocate(void* __p, size_t /* old_sz */, size_t __new_sz) {
        void* __result = realloc(__p, __new_sz);
        if (nullptr == __result) __result = _S_oom_realloc(__p, __new_sz);
        return __result;
    }

    static void (*__set_malloc_handler(void (*__f)()))() {
        void (*__old)() = __malloc_alloc_oom_handler;
        __malloc_alloc_oom_handler = __f;
        return (__old);
    }
};
#ifndef __STL_STATIC_TEMPLATE_MEMBER_BUG
template <int __inst>
void (* __malloc_alloc_template<__inst>::__malloc_alloc_oom_handler)() = nullptr;
#endif
template <int __inst>
void* __malloc_alloc_template<__inst>::_S_oom_malloc(size_t __n) {
    void (*__my_malloc_handler)();
    void* __result;

    for (;;) {
        __my_malloc_handler = __malloc_alloc_oom_handler;
        if (nullptr == __my_malloc_handler) {
            //__THROW_BAD_ALLOC;
            throw std::bad_alloc();
        }
        (*__my_malloc_handler)();
        __result = malloc(__n);
        if (__result) return (__result);
    }
}

template <int __inst>
void* __malloc_alloc_template<__inst>::_S_oom_realloc(void* __p, size_t __n) {
    void (*__my_malloc_handler)();
    void* __result;

    for (;;) {
        __my_malloc_handler = __malloc_alloc_oom_handler;
        if (nullptr == __my_malloc_handler) {
            //__THROW_BAD_ALLOC;
            throw std::bad_alloc();
        }
        (*__my_malloc_handler)();
        __result = realloc(__p, __n);
        if (__result) return (__result);
    }
}
typedef __malloc_alloc_template<0> malloc_alloc;
template <class _T>
class Myallocator {
public:
    using value_type = _T;
    constexpr Myallocator() noexcept {}
    constexpr Myallocator(const Myallocator& other) noexcept = default;
    template <class _Othrer>
    constexpr Myallocator(const Myallocator<_Othrer>& other) noexcept {}
    _T* allocate(size_t __n) {
        void* __ret =nullptr;
        __n *=sizeof(_T);
        if (__n > (size_t)_MAX_BYTES) {
            __ret = malloc_alloc::allocate(__n);
        } else {
            _Obj* volatile* __my_free_list =
                    _S_free_list + _S_freelist_index(__n);
            std::lock_guard<std::mutex> lock(mtx);
            _Obj* __result = *__my_free_list;
            if (__result == nullptr)
                __ret = _S_refill(_S_round_up(__n));
            else {
                *__my_free_list = __result->_M_free_list_link;
                __ret = __result;
            }
        }

        return (_T*)__ret;
    }
    void deallocate(_T* __p, size_t __n) {
        if (__n > (size_t)_MAX_BYTES)
            malloc_alloc::deallocate(__p, __n);
        else {
            _Obj* volatile* __my_free_list =
                    _S_free_list + _S_freelist_index(__n);
            _Obj* __q = (_Obj*)__p;
            std::lock_guard<std::mutex> lock(mtx);
            __q->_M_free_list_link = *__my_free_list;
            *__my_free_list = __q;
        }
    }

    void* reallocate(void* __p, size_t __old_sz, size_t __new_sz) {
        void* __result;
        size_t __copy_sz;
        if (__old_sz > (size_t)_MAX_BYTES && __new_sz > (size_t)_MAX_BYTES) {
            return (realloc(__p, __new_sz));
        }
        if (_S_round_up(__old_sz) == _S_round_up(__new_sz)) return (__p);
        __result = allocate(__new_sz);
        __copy_sz = __new_sz > __old_sz ? __old_sz : __new_sz;
        memcpy(__result, __p, __copy_sz);
        deallocate(__p, __old_sz);
        return (__result);
    }

    void construct(_T* __p, const _T& val) { new (__p) _T(val); }
    void destroy(_T* __p) { __p->~_T(); }

private:
    enum { _ALIGN = 8 };
    enum { _MAX_BYTES = 128 };
    enum { _NFREELISTS = 16 };

    union _Obj {  // 每一个chunk块的头信息
        union _Obj* _M_free_list_link;
        char _M_client_data[1]; /* The client sees this.        */
    };
    static _Obj* volatile _S_free_list[_NFREELISTS];
    static size_t _S_round_up(size_t __bytes) {
        return (((__bytes) + (size_t)_ALIGN - 1) & ~((size_t)_ALIGN - 1));
    }
    static size_t _S_freelist_index(size_t __bytes) {
        return (((__bytes) + (size_t)_ALIGN - 1) / (size_t)_ALIGN - 1);
    }
    static char* _S_start_free;
    static char* _S_end_free;
    static size_t _S_heap_size;
    static std::mutex mtx;
    static void* _S_refill(size_t __n) {
        int __nobjs = 20;
        char* __chunk = _S_chunk_alloc(__n, __nobjs);
        _Obj* volatile* __my_free_list;
        _Obj* __result;
        _Obj* __current_obj;
        _Obj* __next_obj;
        int __i;

        if (1 == __nobjs) return (__chunk);
        __my_free_list = _S_free_list + _S_freelist_index(__n);

        /* Build free list in chunk */
        __result = (_Obj*)__chunk;
        *__my_free_list = __next_obj = (_Obj*)(__chunk + __n);
        for (__i = 1;; __i++) {
            __current_obj = __next_obj;
            __next_obj = (_Obj*)((char*)__next_obj + __n);
            if (__nobjs - 1 == __i) {
                __current_obj->_M_free_list_link = 0;
                break;
            } else {
                __current_obj->_M_free_list_link = __next_obj;
            }
        }
        return (__result);
    }
    static char* _S_chunk_alloc(size_t __size, int& __nobjs) {
        char* __result;
        size_t __total_bytes = __size * __nobjs;
        size_t __bytes_left = _S_end_free - _S_start_free;

        if (__bytes_left >= __total_bytes) {
            __result = _S_start_free;
            _S_start_free += __total_bytes;
            return (__result);
        } else if (__bytes_left >= __size) {
            __nobjs = (int)(__bytes_left / __size);
            __total_bytes = __size * __nobjs;
            __result = _S_start_free;
            _S_start_free += __total_bytes;
            return (__result);
        } else {
            size_t __bytes_to_get =
                    2 * __total_bytes + _S_round_up(_S_heap_size >> 4);
            if (__bytes_left > 0) {
                _Obj* volatile* __my_free_list =
                        _S_free_list + _S_freelist_index(__bytes_left);

                ((_Obj*)_S_start_free)->_M_free_list_link = *__my_free_list;
                *__my_free_list = (_Obj*)_S_start_free;
            }
            _S_start_free = (char*)malloc(__bytes_to_get);
            if (nullptr == _S_start_free) {
                size_t __i;
                _Obj* volatile* __my_free_list;
                _Obj* __p;

                for (__i = __size; __i <= (size_t)_MAX_BYTES;
                     __i += (size_t)_ALIGN) {
                    __my_free_list = _S_free_list + _S_freelist_index(__i);
                    __p = *__my_free_list;
                    if (nullptr != __p) {
                        *__my_free_list = __p->_M_free_list_link;
                        _S_start_free = (char*)__p;
                        _S_end_free = _S_start_free + __i;
                        return (_S_chunk_alloc(__size, __nobjs));
                    }
                }
                _S_end_free = 0;  // In case of exception.
                _S_start_free = (char*)malloc_alloc::allocate(__bytes_to_get);

            }
            _S_heap_size += __bytes_to_get;
            _S_end_free = _S_start_free + __bytes_to_get;
            return (_S_chunk_alloc(__size, __nobjs));
        }
    }
};

template <typename _T>
char* Myallocator<_T>::_S_start_free = nullptr;

template <typename _T>
char* Myallocator<_T>::_S_end_free = nullptr;

template <typename _T>
size_t Myallocator<_T>::_S_heap_size = 0;

template <typename _T>
typename Myallocator<_T>::_Obj* volatile Myallocator<
        _T>::_S_free_list[Myallocator<_T>::_NFREELISTS] = {
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

template <typename _T>
std::mutex Myallocator<_T>::mtx;
#endif //THREAD_POOL_MYALLOCATOR_H
