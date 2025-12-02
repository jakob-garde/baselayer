#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <cstddef>
#include <cstdlib>
#include <cassert>


//
// platform dependent:


u64 MemoryProtect(void *from, u64 amount);
void *MemoryReserve(u64 amount);
s32 MemoryUnmap(void *at, u64 amount_reserved);


//
// Memory Arena allocator


struct MArena {
    u8 *mem;
    u64 mapped;
    u64 committed;
    u64 used;
    u64 fixed_size;
};

#define ARENA_RESERVE_SIZE GIGABYTE
#define ARENA_COMMIT_CHUNK SIXTEEN_KB

MArena ArenaCreate(u64 fixed_size = 0) {
    MArena a = {};
    a.used = 0;

    if (fixed_size > 0) {
        a.mem = (u8*) MemoryReserve(fixed_size);
        a.mapped = fixed_size;
        MemoryProtect(a.mem, fixed_size);
        a.committed = fixed_size;
    }
    else {
        a.mem = (u8*) MemoryReserve(ARENA_RESERVE_SIZE);
        a.mapped = ARENA_RESERVE_SIZE;
        MemoryProtect(a.mem, ARENA_COMMIT_CHUNK);
        a.committed = ARENA_COMMIT_CHUNK;
    }

    return a;
}

void ArenaDestroy(MArena *a) {
    MemoryUnmap(a, a->committed);
    *a = {};
}

inline
void *ArenaAlloc(MArena *a, u64 len, bool zerod = true) {
    if (a->fixed_size) {
        assert(a->fixed_size == a->committed && "ArenaAlloc: fixed_size misconfigured");
        assert(a->fixed_size <= a->used + len && "ArenaAlloc: fixed_size exceeded");
    }

    else if (a->committed < a->used + len) {
        assert(a->committed >= a->used);
        u64 diff = a->committed - a->used;
        u64 amount = ( (len - diff) / ARENA_COMMIT_CHUNK + 1) * ARENA_COMMIT_CHUNK;
        MemoryProtect(a->mem + a->committed, amount );
        a->committed += amount;
    }

    void *result = a->mem + a->used;
    a->used += len;
    if (zerod) {
        _memzero(result, len);
    }

    return result;
}

inline
void ArenaRelease(MArena *a, u64 len) {
    assert(len <= a->used);

    a->used -= len;
}

inline
void *ArenaPush(MArena *a, void *data, u32 len) {
    void *dest = ArenaAlloc(a, len);
    memcpy(dest, data, len);
    return dest;
}

void ArenaPrint(MArena *a) {
    printf("Arena mapped/committed/used: %lu %lu %lu\n", a->mapped, a->committed, a->used);
}

void ArenaClear(MArena *a) {
    a->used = 0;
}

void ArenaEnsureSpace(MArena *a, s32 space_needed) {
    u64 used = a->used;
    u64 diff = a->committed - a->used;

    if (diff < space_needed) {
        // expand committed space without marking it as used
        ArenaAlloc(a, space_needed);
        a->used = used;
    }
}


//
//  Pool allocator / slot based allocation impl. using a free-list to track vacant slots.
//
//  NOTE: Pool indices, if used, are counted from 1, and the value 0 is reserved as the NULL equivalent.


struct MPoolBlockHdr {
    MPoolBlockHdr *next;
    u64 lock;
};

struct MPool {
    u8 *mem;
    u32 block_size;
    u32 nblocks;
    u32 occupancy;
    u64 lock;
    MPoolBlockHdr free_list;
};


#define MPOOL_MIN_BLOCK_SIZE 64


MPool PoolCreate(u32 block_size_min, u32 nblocks) {
    assert(nblocks > 1);

    MPool p = {};
    p.block_size = MPOOL_MIN_BLOCK_SIZE * (block_size_min / MPOOL_MIN_BLOCK_SIZE + 1);
    p.nblocks = nblocks;
    p.lock = (u64) &p; // this number is a lifetime constant

    p.mem = (u8*) MemoryReserve(p.block_size * p.nblocks);
    MemoryProtect(p.mem, p.block_size * p.nblocks);

    MPoolBlockHdr *freeblck = &p.free_list;
    for (u32 i = 0; i < nblocks; ++i) {
        freeblck->next = (MPoolBlockHdr*) (p.mem + i * p.block_size);
        freeblck->lock = p.lock;
        freeblck = freeblck->next;
    }
    freeblck->next = NULL;

    return p;
}

MPool PoolCreate(MArena *a_dest, u32 block_size_min, u32 nblocks) {
    assert(nblocks > 1);

    MPool p = {};
    p.block_size = MPOOL_MIN_BLOCK_SIZE * (block_size_min / MPOOL_MIN_BLOCK_SIZE + 1);
    p.nblocks = nblocks;
    p.lock = (u64) &p; // this "magic" number is a lifetime constant, checked at allocation time
    p.mem = (u8*) ArenaAlloc(a_dest, p.block_size * p.nblocks);

    MPoolBlockHdr *freeblck = &p.free_list;
    for (u32 i = 0; i < nblocks; ++i) {
        freeblck->next = (MPoolBlockHdr*) (p.mem + i * p.block_size);
        freeblck->lock = p.lock;
        freeblck = freeblck->next;
    }
    freeblck->next = NULL;

    return p;
}

void *PoolAlloc(MPool *p) {
    if (p->free_list.next == NULL) {
        return NULL;
    }
    void *retval = p->free_list.next;
    p->free_list.next = p->free_list.next->next;
    _memzero(retval, p->block_size);

    ++p->occupancy;
    return retval;
}

bool PoolCheckAddress(MPool *p, void *ptr) {
    if (ptr == NULL) {
        return false;
    }
    bool b1 = (ptr >= (void*) p->mem); // check lower bound
    if (b1 == false) {
        return false;
    }
    u64 offset = (u8*) ptr -  p->mem;
    bool b2 = (offset % p->block_size == 0); // check alignment
    bool b3 = (offset < p->block_size * p->nblocks); // check upper bound

    return b2 && b3;
}

bool PoolFree(MPool *p, void *element, bool enable_strict_mode = true) {
    assert(PoolCheckAddress(p, element) && "input address aligned and in range");
    MPoolBlockHdr *e = (MPoolBlockHdr*) element;

    bool address_aligned = PoolCheckAddress(p, e);
    bool next_address_matches = PoolCheckAddress(p, e->next);
    bool key_matches = (e->lock == p->lock);

    if (key_matches && (next_address_matches || e->next == NULL)) {
        if (enable_strict_mode) {
            assert(1 == 0 && "Attempt to free an un-allocated block");
        }
        else {
            return false;
        }
    }

    else if (address_aligned == false) {
        if (enable_strict_mode) {
            assert(1 == 0 && "Attempt to free a non-pool address");
        }
        else {
            return false;
        }
    }

    else {
        e->next = p->free_list.next;
        e->lock = p->lock;

        p->free_list.next = e;
        --p->occupancy;
    }
    return true;
}

inline
u32 PoolPtr2Idx(MPool *p, void *ptr) {
    PoolCheckAddress(p, ptr);
    if (ptr == NULL) {
        return 0;
    }
    u32 idx = (u32) ( ((u8*) ptr - (u8*) p->mem) / p->block_size );
    return idx;
}

inline
void *PoolIdx2Ptr(MPool *p, u32 idx) {
    assert(idx < p->nblocks);

    if (idx == 0) {
        return NULL;
    }
    void *ptr = (u8*) p->mem + idx * p->block_size;
    return ptr;
}

u32 PoolAllocIdx(MPool *p) {
    void *element = PoolAlloc(p);
    if (element == NULL) {
        return 0;
    }

    u32 idx = (u32) ((u8*) element - (u8*) p->mem) / p->block_size;
    assert(idx < p->nblocks && "block index must be positive and less and the number of blocks");

    return idx;
}

bool PoolFreeIdx(MPool *p, u32 idx) {
    void * ptr = PoolIdx2Ptr(p, idx);
    return PoolFree(p, ptr);
}


//
// Templated memory pool wrapper
//


template<typename T>
struct MPoolT {
    MPool _p;

    T *Alloc() {
        return (T*) PoolAlloc(&this->_p);
    }
    void Free(T* el) {
        PoolFree(&this->_p, el);
    }
};

template<class T>
MPoolT<T> PoolCreate(u32 nblocks) {
    MPool pool_inner = PoolCreate(sizeof(T), nblocks);
    MPoolT<T> pool;
    pool._p = pool_inner;
    return pool;
}


//
//  List & Array


template<typename T>
struct List {
    T *lst = NULL;
    u32 len = 0;

    inline
    void Add(T *element) {
        lst[len++] = *element;
    }
    inline
    T *Add(T element) {
        lst[len++] = element;
        return LastPtr();
    }
    inline
    T *AddUnique(T element) {
        for (u32 i = 0; i < len; ++i) {
            if (lst[i] == element) {
                return NULL;
            }
        }
        return Add(element);
    }
    inline
    void Push(T element) {
        lst[len++] = element;
    }
    inline
    T Pop() {
        return lst[--len];
    }
    inline
    T Last() {
        assert(len > 0);
        return lst[len - 1];
    }
    inline
    T *LastPtr() {
        assert(len > 0);
        return lst + len - 1;
    }
    inline
    T First() {
        assert(len > 0);
        return lst[0];
    }
    inline
    void Delete(u32 idx) {
        // unordered delete / swap-dec
        T swap = Last();
        len--;
        lst[len] = lst[idx];
        lst[idx] = swap;
    }
};

template<class T>
List<T> InitList(MArena *a, u32 count, bool zerod = true) {
    List<T> _lst = {};
    _lst.len = 0;
    _lst.lst = (T*) ArenaAlloc(a, sizeof(T) * count, zerod);
    return _lst;
}

template<class T>
void ArenaShedTail(MArena *a, List<T> lst, u32 diff_T) {
    assert(a->used >= diff_T * sizeof(T));
    assert(a->mem + a->used == (u8*) (lst.lst + lst.len + diff_T));

    a->mem -= diff_T * sizeof(T);
}

template<class T>
List<T> ListCopy(MArena *a_dest, List<T> src) {
    List<T> dest = InitList<T>(a_dest, src.len);
    dest.len = src.len;
    memcpy(dest.lst, src.lst, sizeof(T) * src.len);
    return dest;
}


//
//  Fixed-size arrays with overflow checks


// TODO: override [] to be able to do the overflow check while using that operator


template<typename T>
struct Array {
    T *arr = NULL;
    u32 len = 0;
    u32 max = 0;

    inline
    void Add(T *element) {
        assert(len < max);

        arr[len++] = *element;
    }
    inline
    T *Add(T element) {
        assert(len < max);

        arr[len++] = element;
        return LastPtr();
    }
    inline
    T *AddUnique(T element) {
        assert(len < max);

        for (u32 i = 0; i < len; ++i) {
            if (arr[i] == element) {
                return NULL;
            }
        }
        return Add(element);
    }
    inline
    void Push(T element) {
        assert(len < max);

        arr[len++] = element;
    }
    inline
    T Pop() {
        if (len) {
            return arr[--len];
        }
        else {
            return {};
        }
    }
    inline
    T Last() {
        if (len) {
            return arr[len - 1];
        }
        else {
            return {};
        }
    }
    inline
    T *LastPtr() {
        if (len) {
            return arr + len - 1;
        }
        else {
            return NULL;
        }
    }
    inline
    T First() {
        if (len) {
            return arr[0];
        }
        else {
            return {};
        }
    }
    inline
    void Delete(u32 idx) {
        assert(idx < len);

        T swap = Last();
        len--;
        arr[len] = arr[idx];
        arr[idx] = swap;
    }
};

template<class T>
Array<T> InitArray(MArena *a, u32 max_len) {
    Array<T> _arr = {};
    _arr.len = 0;
    _arr.max = max_len;
    _arr.arr = (T*) ArenaAlloc(a, sizeof(T) * max_len);
    return _arr;
}


//
//  Stack


template<typename T>
struct Stack {
    T *lst = NULL;
    u32 len = 0;
    u32 cap = 0;

    inline
    void Push(T element) {
        lst[len++] = element;
    }
    inline
    T Pop() {
        if (len) {
            return lst[--len];
        }
        else {
            T zero = {};
            return zero;
        }
    }
};

template<class T>
Stack<T> InitStack(MArena *a, u32 cap) {
    Stack<T> stc;
    stc.lst = (T*) ArenaAlloc(a, sizeof(T) * cap, true);
    stc.len = 0;
    stc.cap = cap;
    return stc;
}

template<class T>
Stack<T> InitStackStatic(T *mem, u32 cap) {
    Stack<T> stc;
    stc.len = 0;
    stc.cap = cap;
    stc.lst = (T*) mem;
    return stc;
}


//
// Self-expanding array


// eXpanding list:
//
// - pass by reference, constructor is there
// - memory managed by internal arena
// - array subscripting, accessed by Lst(), can be used up to Len(), done by accessing a pointer member
/*
    ListX<u32> lst;
*/


template<typename T>
struct ListX {
    MArena _arena;

    inline
    void _XPand() {
        ArenaAlloc(&this->_arena, ARENA_COMMIT_CHUNK);
    }
    void Init(u32 initial_cap = 0) {
        this->_arena = ArenaCreate();
        if (Cap() == 0) {
            _XPand();
        }
        while (Cap() < initial_cap) {
            _XPand();
        }
    }
    inline
    T *Lst() {
        return (T*) _arena.mem;
    }
    inline
    u32 Len() {
        assert(this->_arena.used % sizeof(T) == 0);
        u32 len = (u32) this->_arena.used / sizeof(T);
        return len;
    }
    void SetLen(u32 len) {
        Init(len);
        _arena.used = len * sizeof(T);
    }
    inline
    u32 Cap() {
        u32 capacity = (u32) this->_arena.committed / sizeof(T);
        return capacity;
    }
    inline
    void Add(T element) {
        if (sizeof(T) > _arena.committed - _arena.used) {
            _XPand();
        }
        Lst()[Len()] = element;
        this->_arena.used += sizeof(T);
    }
    inline
    T *GetPtr(u32 idx) {
        return (T*) _arena.mem + idx;
    }
    inline
    T Get(u32 idx) {
        return *((T*) _arena.mem + idx);
    }
    inline
    void Set(u32 idx, T element) {
        if (idx < Len()) {
            Lst()[idx] = element;
        }
    }
};


//
// Stretchy buffer
//
// Subscripting on the native C pointer. (E.g. using ptr[])
// The pointer is associated with a len and a capacity/max_len stored before the actual array.
// e.g.:
/*
    s32 *lst = NULL;
    lst_push(lst, 42);
    lst_push(lst, -15);

    for (int i = 0; i < lst_len(lst); ++i) {
        printf("%d\n", lst[i]);
    }
*/
// TODO: allow the virtual memory -style of growing, for pointer stability (and maybe performance)
// TODO: adopt "double internal storage space" convention


struct LstHdr {
    u32 len;
    u32 cap;
    u8 list[];
};

#define lst__hdr(lst)     ( lst ? ((LstHdr*) ( (u8*) lst - sizeof(LstHdr) )) : 0 )
#define lst__fits(lst, n) ( lst ? lst__cap(lst) >= lst_len(lst) + n : 0 )
#define lst__fit(lst, n)  ( lst__fits(lst, n) ? 0 : lst = lst__grow(lst, n) )
#define lst__cap(lst)     ( lst ? *((u32*)lst__hdr(lst) + 1) : 0 )

#define lst_len(lst)      ( lst ? *((u32*)lst__hdr(lst)) : 0 )
#define lst_push(lst, e)  ( lst__fit(lst, 1), lst[lst_len(lst)] = e, lst__hdr(lst)->len++ )
#define lst_free(lst)     ( free(lst__hdr(lst)) )
#define lst_print(lst)    ( lst ? printf("len: %u, cap: %u\n", lst__hdr(lst)->len, lst__hdr(lst)->cap) : 0 )

template<class T>
T *lst__grow(T *lst, u32 add_len) {

    u32 new_cap = MaxU32(2 * lst__cap(lst), MaxU32(add_len, 16));
    u32 new_size = new_cap * sizeof(T) + offsetof(LstHdr, list);

    LstHdr *new_hdr = NULL;
    if (lst == NULL) {
        new_hdr = (LstHdr*) malloc(new_size);
        new_hdr->len = 0;
    }
    else {
        new_hdr = (LstHdr*) realloc(lst__hdr(lst), new_size);
    }
    new_hdr->cap = new_cap;
    return (T*) new_hdr->list;
}


//
//  Sort
//


void SortBubbleU32(List<u32> arr) {
    u32 a;
    u32 b;
    for (u32 i = 0; i < arr.len; ++i) {
        for (u32 j = 0; j < arr.len - 1; ++j) {
            a = arr.lst[j];
            b = arr.lst[j+1];

            if (a > b) {
                arr.lst[j] = b;
                arr.lst[j+1] = a;
            }
        }
    }
}

List<u32> SetIntersectionU32(MArena *a_dest, List<u32> arr_a, List<u32> arr_b) {
    List<u32> result = InitList<u32>(a_dest, 0);
    if (arr_a.len == 0 || arr_b.len == 0) {
        return result;
    }
    SortBubbleU32(arr_a);
    SortBubbleU32(arr_b);

    u32 min = MaxU32(arr_a.First(), arr_b.First());
    u32 max = MinU32(arr_a.Last(), arr_b.Last());
    u32 i = 0;
    u32 j = 0;
    u32 i_max = arr_a.len - 1;
    u32 j_max = arr_b.len - 1;

    // fast-forward both arrays to min value
    u32 a = arr_a.First();
    u32 b = arr_b.First();
    while (a < min) {
        a = arr_a.lst[++i];
    }
    while (b < min) {
        b = arr_b.lst[++j];
    }

    // do the intersection work until we reach max value
    while (i <= i_max && j <= j_max && a <= max && b <= max) {
        if (a == b) {
            if (result.len == 0 || (result.len > 0 && a != result.Last())) {
                ArenaAlloc(a_dest, sizeof(u32));
                result.Add(a);
            }
            a = arr_a.lst[++i];
            b = arr_b.lst[++j];
        }
        else if (a < b) {
            while (a < b && i < i_max) {
                a = arr_a.lst[++i];
            }
        }
        else if (b < a) {
            while (b < a && j < j_max) {
                b = arr_b.lst[++j];
            }
        }
    }
    return result;
}


#endif
