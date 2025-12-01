#include <cstdio>
#include <cassert>
#include "src/baselayer.h"


struct PoolTestEntity {
    f32 a;
    f32 b;
    f32 c;
};


void TestStringBasics() {
    InitBaselayer();

    MArena arena = ArenaCreate();
    MArena *a = &arena;

    printf("StrLiteral\n");
    Str s1 = StrL("hello");
    Str s2 = StrL("hello_noteq");
    Str s3 = StrL("hello");

    StrPrint("StrPrint - ", s1, "\n");

    printf("StrEqual - ");
    printf("should be (0 1 0): %d %d %d\n", StrEqual(s1, s2), StrEqual(s1, s3), StrEqual(s2, s3));

    printf("StrCat - ");
    StrPrint(StrCat(s1, s3));
    printf("\n");

    printf("StrSplit / StrLstPrint - ");
    Str to_split = StrL("...Hello.I..Have.Been.Split..");
    printf("splitting: ");
    StrPrint(to_split);
    printf(" into: ");
    StrLst *lst = StrSplit(to_split, '.');
    StrLstPrint(lst);
    printf("\n");

    printf("StrJoin - ");
    Str join = StrJoin(a, lst);
    StrPrint(join);
    printf("\n");
    join = StrJoinInsertChar(a, lst, '/');
    StrPrint(join);
    printf("\n");

    printf("CLAInit - ");
    if (CLAContainsArg("--test", g_argc, g_argv)) {
        printf("OK");
    }
    printf("\n");
}


void TestMemoryPool() {
    printf("\nTestMemoryPool\n");

    u32 pool_size = 1001;
    u32 test_num_partial = 666;
    MPool pool = PoolCreate(sizeof(PoolTestEntity), pool_size);
    PoolTestEntity *e;
    PoolTestEntity *elements[1001];

    // populate to full
    printf("populating to max ...\n");
    for (u32 i = 0; i < pool_size; ++i) {
        e = (PoolTestEntity *) PoolAlloc(&pool);
        elements[i] = e;
        assert(e != NULL);
    }
    e = (PoolTestEntity *) PoolAlloc(&pool);
    assert(e == NULL);
    assert(pool.occupancy == pool_size);

    // depopulate to zero
    printf("de-populating to zero ...\n");
    for (u32 i = 0; i < pool_size; ++i) {
        e = elements[i];
        bool was_freed = PoolFree(&pool, e);
        assert(was_freed == true);

        bool was_freed_twice = PoolFree(&pool, e, false);
        assert(was_freed_twice == false);
    }
    assert(pool.occupancy == 0);

    // twice-free another element
    e = elements[test_num_partial];
    bool twice2 = PoolFree(&pool, e, false);
    assert(twice2 == false);

    // re-populate to some %
    printf("re-populating to partial ...\n");
    for (u32 i = 0; i < test_num_partial; ++i) {
        e = (PoolTestEntity *) PoolAlloc(&pool);
        elements[i] = e;
        assert(e != NULL);
    }
    assert(pool.occupancy == test_num_partial);
}


void _PrintFlexArrayU32(u32 *arr) {
    for (u32 i = 0; i < lst_len(arr); ++i) {
        printf("%u ", arr[i]);
    }
}
void TestSorting() {
    printf("\nTestSorting\n");

    MContext *ctx = GetContext();
    RandInit();

    u32 cnt_a = 100;
    u32 cnt_b = 300;
    u32 min_a = 11;
    u32 max_a = 44;
    u32 min_b = 22;
    u32 max_b = 66;

    u32 *arr_a = NULL;
    for (u32 i = 0; i < cnt_a; ++i) {
        lst_push(arr_a, RandMinMaxI(min_a, max_a));
    }
    assert(cnt_a == lst_len(arr_a) && "another flexi buffer test");

    u32 *arr_b = NULL;
    for (u32 i = 0; i < cnt_b; ++i) {
        lst_push(arr_b, RandMinMaxI(min_b, max_b));
    }
    assert(cnt_b == lst_len(arr_b) && "another flexi buffer test");

    // print array a
    printf("\narray a:\n");
    _PrintFlexArrayU32(arr_a);
    printf("\n\n");

    // sort array a
    SortBubbleU32({ arr_a, lst_len(arr_a) });
    _PrintFlexArrayU32(arr_a);
    printf("\n\n");

    // print array b
    printf("array b:\n");
    _PrintFlexArrayU32(arr_b);
    printf("\n\n");

    // sort array b
    SortBubbleU32({ arr_b, lst_len(arr_b) });
    _PrintFlexArrayU32(arr_b);
    printf("\n\n");

    //
    printf("intersection:\n");
    List<u32> intersection = SetIntersectionU32(ctx->a_pers, { arr_a, lst_len(arr_a) }, { arr_b, lst_len(arr_b) });
    for (u32 i = 0; i < intersection.len; ++i) {
        printf("%u ", intersection.lst[i]);
    }
    printf("\n\n");
}


void TestStringHelpers() {
    printf("\nTestStringHelpers\n");

    printf("\nlook for .h hiles in ./:\n");
    StrLst *fs = GetFiles((char*) ".", "h");
    StrLstPrint(fs);

    printf("\nlook for .cmake file in .:\n");
    StrLst *fs2 = GetFiles((char*) ".", "cmake");
    StrLstPrint(fs2);
}


void TestDict() {
    printf("\nTestDict\n");
    InitBaselayer();
    
    u32 nslots = 17;
    u32 sz_val = sizeof(u32);
    Dict dct = InitDict(nslots, sz_val);
    dct.debug_print = true;
    RandInit();

    printf("\n| put values:\n");
    u32 val;
    val = RandIntMax(UINT32_MAX);
    printf("hest : %u in ", val);
    DictPut(&dct, "hest", &val);
    val = RandIntMax(UINT32_MAX);
    printf("melon : %u in ", val);
    DictPut(&dct, "melon", &val);
    val = RandIntMax(UINT32_MAX);
    printf("møg : %u in ", val);
    DictPut(&dct, "møg", &val);
    val = RandIntMax(UINT32_MAX);
    printf("blad : %u in ", val);
    DictPut(&dct, "blad", &val);
    val = RandIntMax(UINT32_MAX);
    printf("appelsin : %u in ", val);
    DictPut(&dct, "appelsin", &val);
    val = RandIntMax(UINT32_MAX);
    printf("æble : %u in ", val);
    DictPut(&dct, "pære", &val);
    val = RandIntMax(UINT32_MAX);
    printf("æble : %u in ", val);
    DictPut(&dct, "æble", &val);
    val = RandIntMax(UINT32_MAX);
    printf("bold : %u in ", val);
    DictPut(&dct, "bold", &val);
    val = RandIntMax(UINT32_MAX);
    printf("sol : %u in ", val);
    DictPut(&dct, "sol", &val);
    val = RandIntMax(UINT32_MAX);
    printf("måne : %u in ", val);
    DictPut(&dct, "måne", &val);
    val = RandIntMax(UINT32_MAX);
    printf("blad : %u in ", val);
    DictPut(&dct, "blad", &val);
    val = RandIntMax(UINT32_MAX);
    printf("gulerod : %u in ", val);
    DictPut(&dct, "gulerod", &val);
    val = RandIntMax(UINT32_MAX);
    printf("banan : %u in ", val);
    DictPut(&dct, "banan", &val);

    printf("\ncollisions: %u\n", dct.ncollisions);
    printf("\n| walk the key-vals:\n");
    DictStorageWalk(&dct);

    // get values 

    printf("\n| getting a few values:\n");
    u32 *ptr_val;
    ptr_val = (u32*) DictGet(&dct, "gulerod");
    printf("gulerod get : %u\n", *ptr_val);
    ptr_val = (u32*) DictGet(&dct, "melon");
    printf("melon get : %u\n", *ptr_val);
    ptr_val = (u32*) DictGet(&dct, "hest");
    printf("hest get : %u\n", *ptr_val);
}


void TestPointerHashMap() {
    printf("\nTestPointerHashMap\n");
    MContext *ctx = GetContext(1024 * 1024);
    RandInit();

    u32 nslots = 12;
    HashMap _map = InitMap(ctx->a_life, nslots);
    HashMap *map = &_map;

    u32 nputs = 20;
    static u64 keys[20];
    for (u32 i = 0; i < nputs; ++i) {
        // create some random 64b values
        u64 key_high = RandMinMaxU(1, UINT32_MAX - 1);
        u64 key_low = RandMinMaxU(1, UINT32_MAX - 1);
        u64 val_high = RandMinMaxU(1, UINT32_MAX - 1);
        u64 val_low = RandMinMaxU(1, UINT32_MAX - 1);
        u64 key = (key_high << 32) + key_low;
        keys[i] = key;
        u64 val = (val_high << 32) + val_low;
        printf("MapPut() key: %lu, val: %lu\n", key, val);

        // enter into the map
        MapPut(map, key, val);

    }
    printf("collisions: %u, resets: %u\n\n", map->ncollisions, map->nresets);


    for (u32 i = 0; i < nputs; ++i) {
        u64 key = keys[i];
        u64 val = MapGet(map, key);

        printf("MapGet() key: %lu, val: %lu\n", key, val);
    }
}


struct TestWidget {
    TestWidget *next;
    TestWidget *first;
    TestWidget *parent;
    u64 key;
};
void TestPoolAllocatorAgain()  {
    printf("\nTestPoolAllocatorAgain\n");

    MContext *ctx = InitBaselayer();

    u32 pool_sz = 1000;
    u32 fill_sz = 800;
    u32 rm_sz = 500;
    u32 refill_sz = 500;

    printf("\n");
    printf("create                %u\n", pool_sz);
    MPoolT<TestWidget> _p = PoolCreate<TestWidget>(pool_sz);
    MPoolT<TestWidget> *p = &_p;

    List<TestWidget*> wgts = InitList<TestWidget*>(ctx->a_tmp, pool_sz);

    // fill
    printf("fill                  %u\n", fill_sz);
    for (u32 i = 0; i < fill_sz; ++i) {
        TestWidget *w = p->Alloc();
        wgts.Add(w);
    }

    u32 repeat_cnt = 10; // 1000
    for (u32 rep = 0; rep < repeat_cnt; ++rep) {
        // remove
        printf("remove random allocs  %u\n", rm_sz);
        for (u32 i = 0; i < rm_sz; ++i) {
            u32 r = RandMinMaxI(0, wgts.len-1);

            p->Free(wgts.lst[r]);
            wgts.Delete(r);
        }

        // re-fill
        printf("re-fill               %u\n", refill_sz);
        for (u32 i = 0; i < refill_sz; ++i) {
            TestWidget *w = p->Alloc();

            assert(w != NULL);

            wgts.Add(w);
        }
    }

    // empty
    for (u32 i = 0; i < wgts.len; ++i) {
        p->Free(wgts.lst[i]);
    }
    assert(p->_p.occupancy == 0);
    printf("emptying              %u\n", wgts.len);
}


void TestStrBuffer() {
    printf("\nTestStrBuffer\n");
    StrInit();

    StrBuff buff = StrBuffInit();
    StrBuffNewLine(&buff);

    s32 total_printed = 0;
    s32 iters = 2 * 10;

    for (s32 i = 0; i < iters; ++i) {
        total_printed += StrBuffPrint1K(&buff, "%d_%u_%s ->\n", 3, i, i*2, "some_text");
    }

    // test StrBuffPut
    Str put_str = { (char*) "an string appended at the end of the buffer", 0 };
    put_str.len = (u32) strlen(put_str.str);
    StrBuffAppend(&buff, put_str);
    StrBuffNewLine(&buff);


    StrPrint(buff.GetStr());
    printf("\n");
    printf("chars printed: %d, buff_len: %d\n", total_printed, buff.len);
    ArenaPrint(&buff.a);
    printf("\n");
}


void TestHashString() {
    printf("TestHashStrings\n\n");

    u64 val;
    val = HashStringValue("PSDbefore_guides_blitarea");
    printf("%lu\n", val);
    val = HashStringValue("l_mon_source_blitarea");
    printf("%lu\n", val);
    val = HashStringValue("PSDbefore_curve_blitarea");
    printf("%lu\n", val);
    val = HashStringValue("PSDafter_curve_blitarea");
    printf("%lu\n", val);
    val = HashStringValue("ydist_fluxpos_blitarea");
    printf("%lu\n", val);
    val = HashStringValue("PSD_fluxpos_blitarea");
    printf("%lu\n", val);
    val = HashStringValue("xdist_flux_pos_blitarea");
    printf("%lu\n", val);
    val = HashStringValue("PSD_fluxposB_blitarea");
    printf("%lu\n", val);
    val = HashStringValue("lambda_in_blitarea");
    printf("%lu\n", val);
    val = HashStringValue("PSD_sample_blitarea");
    printf("%lu\n", val);
    val = HashStringValue("lambda_sample_blitarea");
    printf("%lu\n", val);
    val = HashStringValue("Detector_blitarea");
    printf("%lu\n", val);
}

void Test() {
    printf("Running baselayer tests ...\n\n");

    TestStringBasics();
    TestSorting();
    TestStringHelpers();
    TestDict();
    TestPointerHashMap();
    TestMemoryPool();
    TestPoolAllocatorAgain();
    TestStrBuffer();
    TestHashString();
}
