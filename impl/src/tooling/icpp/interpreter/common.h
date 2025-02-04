//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

#include <assert.h>
#include <setjmp.h>

#include <cstdlib>
#include <cstdint>
#include <math.h>

#include <optional>
#include <string>

#include <vector>
#include <list>
#include <map>

#include "../../api_parse/decls.h"

////////////////////////////////
//Various sizes
#define BSQ_MAX_STACK 2048

////////////////////////////////
//Asserts

#define BSQ_INTERNAL_ASSERT(C) if(!(C)) { assert(false); }

#ifdef BSQ_DEBUG_BUILD
#define HANDLE_BSQ_ABORT(MSG, F, L, C) { printf("\"%s\" in %s on line %i\n", MSG, F, (int)L); fflush(stdout); longjmp(Evaluator::g_entrybuff, C); }
#else
#define HANDLE_BSQ_ABORT() { printf("ABORT\n"); longjmp(Evaluator::g_entrybuff, 5); }
#endif

#ifdef BSQ_DEBUG_BUILD
#define BSQ_LANGUAGE_ASSERT(C, F, L, MSG) if(!(C)) HANDLE_BSQ_ABORT(MSG, (F)->c_str(), L, 2);
#else
#define BSQ_LANGUAGE_ASSERT(C, F, L, MSG) if(!(C)) HANDLE_BSQ_ABORT();
#endif

#ifdef BSQ_DEBUG_BUILD
#define BSQ_LANGUAGE_ABORT(MSG, F, L) HANDLE_BSQ_ABORT(MSG, (F)->c_str(), L, 3)
#else
#define BSQ_LANGUAGE_ABORT(MSG, F, L) HANDLE_BSQ_ABORT()
#endif

////////////////////////////////
//Memory allocator

#if defined(MEM_STATS)
#define ENABLE_MEM_STATS
#define MEM_STATS_OP(X) X
#define MEM_STATS_ARG(X) X
#else
#define MEM_STATS_OP(X)
#define MEM_STATS_ARG(X)
#endif

//Program should not contain any allocations larger than this in a single block 
#define BSQ_ALLOC_MAX_BLOCK_SIZE 65536

//Min and max bump allocator size
#define BSQ_MIN_NURSERY_SIZE 1048576
#define BSQ_MAX_NURSERY_SIZE 16777216

//Allocation routines
#ifdef _WIN32
#define BSQ_STACK_SPACE_ALLOC(SIZE) ((SIZE) == 0 ? nullptr : _alloca(SIZE))
#else
#define BSQ_STACK_SPACE_ALLOC(SIZE) ((SIZE) == 0 ? nullptr : alloca(SIZE))
#endif

#define BSQ_BUMP_SPACE_ALLOC(SIZE) zxalloc(SIZE)
#define BSQ_BUMP_SPACE_RELEASE(M) xfree(M)

#define BSQ_FREE_LIST_ALLOC_SMALL(SIZE) xalloc(SIZE)
#define BSQ_FREE_LIST_RELEASE(SIZE, M) xfree(M)

#define GC_REF_LIST_BLOCK_SIZE_DEFAULT 256

//Header word layout
//high [RC - 40 bits] [MARK - 1 bit] [YOUNG - 1 bit] [UNUSED - 2 bits] [TYPEID - 20 bits]

#define GC_MARK_BIT 0x800000
#define GC_YOUNG_BIT 0x400000
#define GC_BUMP_SPACE_BIT 0x200000
#define GC_EAGER_RC_BIT 0x100000
#define GC_RC_MASK 0xFFFFFFFFFF000000
#define GC_TYPE_ID_MASK 0x1FFFFF
#define GC_REACHABLE_MASK (GC_RC_MASK | GC_MARK_BIT)

typedef uint64_t GC_META_DATA_WORD;
#define GC_GET_META_DATA_ADDR(M) ((GC_META_DATA_WORD*)((uint8_t*)M - sizeof(GC_META_DATA_WORD)))
#define GC_LOAD_META_DATA_WORD(ADDR) (*((GC_META_DATA_WORD*)ADDR))
#define GC_SET_META_DATA_WORD(ADDR, W) (*((GC_META_DATA_WORD*)ADDR) = W)

#define GC_EXTRACT_RC(W) (W & GC_RC_MASK)
#define GC_EXTRACT_TYPEID(W) (W & GC_TYPE_ID_MASK)

#define GC_RC_ZERO ((GC_META_DATA_WORD)0x0)
#define GC_RC_ONE ((GC_META_DATA_WORD)0x1000000)

#define GC_TEST_IS_UNREACHABLE(W) ((W & GC_REACHABLE_MASK) == 0x0)
#define GC_TEST_IS_ZERO_RC(W) ((W & GC_RC_MASK) == GC_RC_ZERO)
#define GC_TEST_IS_YOUNG(W) (W & GC_YOUNG_BIT)

#define GC_CLEAR_MARK_BIT(W) (W & (GC_RC_MASK | GC_YOUNG_BIT | GC_TYPE_ID_MASK))
#define GC_SET_MARK_BIT(W) (W | GC_MARK_BIT)

#define GC_INC_RC(W) (W + GC_RC_ONE)
#define GC_DEC_RC(W) (W - GC_RC_ONE)

#define GC_INIT_BUMP_SPACE_ALLOC(ADDR, TID) GC_SET_META_DATA_WORD(ADDR, GC_YOUNG_BIT | TID)

#define GC_INIT_OLD_RC_ROOT_REF(ADDR, W) GC_SET_META_DATA_WORD(ADDR, GC_RC_ZERO | GC_MARK_BIT | GC_EXTRACT_TYPEID(W))
#define GC_INIT_OLD_RC_HEAP_REF(ADDR, W) GC_SET_META_DATA_WORD(ADDR, GC_RC_ONE | GC_EXTRACT_TYPEID(W))

//Access type info + special forwarding pointer mark
#define GET_TYPE_META_DATA_FROM_WORD(W) (*(BSQType::g_typetable + GC_EXTRACT_TYPEID(W)))
#define GET_TYPE_META_DATA_FROM_ADDR(ADDR) GET_TYPE_META_DATA_FROM_WORD(GC_LOAD_META_DATA_WORD(ADDR))
#define GET_TYPE_META_DATA(M) GET_TYPE_META_DATA_FROM_ADDR(GC_GET_META_DATA_ADDR(M))
#define GET_TYPE_META_DATA_AS(T, M) (dynamic_cast<const T*>(GET_TYPE_META_DATA(M)))

#define GC_SET_TYPE_META_DATA_FORWARD_SENTINAL(ADDR) *(ADDR) = 0
#define GC_IS_TYPE_META_DATA_FORWARD_SENTINAL(W) ((W) == 0)
#define GC_GET_FORWARD_PTR(M) *((void**)M)
#define GC_SET_FORWARD_PTR(M, P) *((void**)M) = (void*)P

//Misc operations
#define COMPUTE_REAL_BYTES(M) (GET_TYPE_META_DATA(M)->allocinfo.heapsize + sizeof(GC_META_DATA_WORD))

#define GC_MEM_COPY(DST, SRC, BYTES) std::copy((uint8_t*)SRC, ((uint8_t*)SRC) + (BYTES), (uint8_t*)DST)
#define GC_MEM_ZERO(DST, BYTES) std::fill((uint8_t*)DST, ((uint8_t*)DST) + (BYTES), (uint8_t)0)

////////////////////////////////
//Storage location ops

//Generic pointer to a storage location that holds a value
typedef void* StorageLocationPtr;

#define IS_INLINE_STRING(S) (*(((uint8_t*)(S)) + 15) != 0)
#define IS_INLINE_BIGNUM(N) false

#define SLPTR_LOAD_CONTENTS_AS(T, L) (*((T*)L))
#define SLPTR_STORE_CONTENTS_AS(T, L, V) *((T*)L) = V

#define SLPTR_LOAD_CONTENTS_AS_GENERIC_HEAPOBJ(L) (*((void**)L))
#define SLPTR_STORE_CONTENTS_AS_GENERIC_HEAPOBJ(L, V) *((void**)L) = V

#define SLPTR_LOAD_UNION_INLINE_TYPE(L) (*((const BSQType**)L))
#define SLPTR_LOAD_UNION_INLINE_TYPE_AS(T, L) ((const T*)(SLPTR_LOAD_UNION_INLINE_TYPE(L)))
#define SLPTR_LOAD_UNION_INLINE_DATAPTR(L) ((void*)(((uint8_t*)L) + sizeof(const BSQType*)))

#define SLPTR_STORE_UNION_INLINE_TYPE(T, L) *((const BSQType**)L) = T

#define SLPTR_LOAD_HEAP_TYPE(L) GET_TYPE_META_DATA(*((void**)L))
#define SLPTR_LOAD_HEAP_TYPE_AS(T, L) ((const T*)SLPTR_LOAD_HEAP_TYPE(*((void**)L)))

#define SLPTR_LOAD_HEAP_DATAPTR(L) (*((void**)L))

#define SLPTR_INDEX_DATAPTR(SL, I) ((void*)(((uint8_t*)SL) + I))

#define BSQ_MEM_ZERO(TRGTL, SIZE) GC_MEM_ZERO(TRGTL, SIZE)
#define BSQ_MEM_COPY(TRGTL, SRCL, SIZE) GC_MEM_COPY(TRGTL, SRCL, SIZE)

////////////////////////////////
//Type and GC interaction decls

class BSQType;

enum class BSQTypeLayoutKind : uint32_t
{
    Invalid = 0x0,
    Empty,
    Register,
    Struct,
    BoxedStruct,
    String,
    BigNum,
    Ref,
    UnionRef,
    UnionInline,
    UnionUniversal
};

#define PTR_FIELD_MASK_NOP '1'
#define PTR_FIELD_MASK_PTR '2'
#define PTR_FIELD_MASK_STRING '3'
#define PTR_FIELD_MASK_BIGNUM '4'
#define PTR_FIELD_MASK_UNION '5'
#define PTR_FIELD_MASK_END (char)0

typedef const char* RefMask;

typedef void (*GCProcessOperatorFP)(const BSQType*, void**);

enum DisplayMode
{
    Standard = 0x0,
    CmdDebug = 0x1,
    CmdDebugLeaf = 0x2 
};

////////////////
#define PROCESS_DISPLAY_MODE(BTYPE, MODE, OBJ) {\
    if(MODE & DisplayMode::CmdDebugLeaf)\
    {\
        if(BTYPE->tkind == BSQTypeLayoutKind::Ref)\
        {\
            return Allocator::registerDbgObjID(BTYPE, SLPTR_LOAD_CONTENTS_AS_GENERIC_HEAPOBJ(OBJ));\
        }\
    }\
    if(MODE & DisplayMode::CmdDebug)\
    {\
        MODE = (DisplayMode)(MODE | DisplayMode::CmdDebugLeaf);\
    }\
}
////////////////

typedef std::string (*DisplayFP)(const BSQType*, StorageLocationPtr, DisplayMode);

struct BSQTypeSizeInfo
{
    const uint64_t heapsize;   //number of bytes needed to represent the data (no type ptr) when storing in the heap
    const uint64_t inlinedatasize; //number of bytes needed in storage location for this (includes type tag for inline union -- is the size of a pointer for ref -- and word size for BSQBool)
    const uint64_t assigndatasize; //number of bytes needed to copy when assigning this to a location -- 1 for BSQBool -- others should be same as inlined size

    const RefMask heapmask; //The mask to used to traverse this object during gc (if it is heap allocated) -- null if this is a leaf object -- partial if tailing scalars
    const RefMask inlinedmask; //The mask used to traverse this object as part of inline storage (on stack or inline in an object) -- must cover full size of data
};

#define UNION_UNIVERSAL_CONTENT_SIZE 32
#define UNION_UNIVERSAL_SIZE 40
#define UNION_UNIVERSAL_MASK "51111"

struct GCFunctorSet
{
    GCProcessOperatorFP fpProcessObjRoot;
    GCProcessOperatorFP fpProcessObjHeap;
    GCProcessOperatorFP fpDecObj;
    GCProcessOperatorFP fpClearObj;
    GCProcessOperatorFP fpMakeImmortal;
};

typedef int (*KeyCmpFP)(const BSQType* btype, StorageLocationPtr, StorageLocationPtr);
constexpr KeyCmpFP EMPTY_KEY_CMP = nullptr;

typedef uint32_t BSQTypeID;
typedef uint32_t BSQTupleIndex;
typedef uint32_t BSQRecordPropertyID;
typedef uint32_t BSQFieldID;
typedef uint32_t BSQInvokeID;
typedef uint32_t BSQVirtualInvokeID;
typedef uint32_t BSQConstantID;

#define BSQ_TYPE_ID_NONE 0
#define BSQ_TYPE_ID_NOTHING 1
#define BSQ_TYPE_ID_BOOL 2
#define BSQ_TYPE_ID_NAT 3
#define BSQ_TYPE_ID_INT 4
#define BSQ_TYPE_ID_BIGNAT 5
#define BSQ_TYPE_ID_BIGINT 6
#define BSQ_TYPE_ID_FLOAT 7
#define BSQ_TYPE_ID_DECIMAL 8
#define BSQ_TYPE_ID_RATIONAL 9
#define BSQ_TYPE_ID_STRING 10
#define BSQ_TYPE_ID_BYTEBUFFER_LEAF 11
#define BSQ_TYPE_ID_BYTEBUFFER_NODE 12
#define BSQ_TYPE_ID_BYTEBUFFER 13
#define BSQ_TYPE_ID_DATETIME 14
#define BSQ_TYPE_ID_TICKTIME 15
#define BSQ_TYPE_ID_LOGICALTIME 16
#define BSQ_TYPE_ID_UUID 17
#define BSQ_TYPE_ID_CONTENTHASH 18
#define BSQ_TYPE_ID_REGEX 19

#define BSQ_TYPE_ID_STRINGREPR_K16 20
#define BSQ_TYPE_ID_STRINGREPR_K32 21
#define BSQ_TYPE_ID_STRINGREPR_K64 22
#define BSQ_TYPE_ID_STRINGREPR_K96 23
#define BSQ_TYPE_ID_STRINGREPR_K128 24

#define BSQ_TYPE_ID_STRINGREPR_TREE 25

enum class BSQPrimitiveImplTag
{
    Invalid = 0x0,

    validator_accepts,

    number_nattoint,
    number_inttonat,
    number_nattobignat,
    number_inttobigint,
    number_bignattonat,
    number_biginttoint,
    number_bignattobigint,
    number_biginttobignat,
    number_bignattofloat,
    number_bignattodecimal,
    number_bignattorational,
    number_biginttofloat,
    number_biginttodecimal,
    number_biginttorational,
    number_floattobigint,
    number_decimaltobigint,
    number_rationaltobigint,
    number_floattodecimal,
    number_floattorational,
    number_decimaltofloat,
    number_decimaltorational,
    number_rationaltofloat,
    number_rationaltodecimal,
    float_floor,
    decimal_floor,
    float_ceil,
    decimal_ceil,
    float_truncate,
    decimal_truncate,

    float_power,
    decimal_power,

    string_empty,
    string_append,
    s_strconcat_ne,
    s_strjoin_ne,

    bytebuffer_getformat,
    bytebuffer_getcompression,

    datetime_create,

    s_list_build_k,
    s_list_size_ne,
    s_list_set_ne,
    s_list_push_back_ne,
    s_list_push_front_ne,
    s_list_remove_ne,
    s_list_pop_back_ne,
    s_list_pop_front_ne,
    s_list_reduce_ne,
    s_list_reduce_idx_ne,
    s_list_transduce_ne,
    s_list_transduce_idx_ne,
    s_list_range_ne,
    s_list_fill_ne,
    s_list_reverse_ne,
    s_list_append_ne,
    s_list_slice_start,
    s_list_slice_end,
    s_list_safe_get,
    s_list_safe_back,
    s_list_safe_front,
    s_list_find_pred_ne,
    s_list_find_pred_idx_ne,
    s_list_find_pred_last_ne,
    s_list_find_pred_last_idx_ne,
    s_list_filter_pred_ne,
    s_list_filter_pred_idx_ne,
    s_list_map_ne,
    s_list_map_idx_ne,
    s_list_map_sync_ne,
    s_list_sort_ne,
    s_list_unique_from_sorted_ne,

    s_map_build_k,
    s_map_size_ne,
    s_map_has_ne,
    s_map_find_ne,
    s_map_union_ne,
    s_map_submap_ne,
    s_map_remap_ne,
    s_map_add_ne,
    s_map_set_ne,
    s_map_remove_ne
};
