#ifndef __HASH_H__
#define __HASH_H__


//
// hash map


u32 Hash32(u32 x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x);
    return x;
}
u64 Hash64(u64 x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x);
    return x;
}
#ifdef __arm__
    #warning "__arm__ detected: u64 typedef'd to 32bit"
    #define Hash Hash32

    // TODO: 32bit version for HashDJB2 below
#else
    #define Hash Hash64
#endif


inline
u64 HashDJB2(Str skey) {
    // djb2 - see http://www.cse.yorku.ca/~oz/hash.html
    u64 hash = 5381;

    s32 c;
    u32 i = 0;
    while (i < skey.len) {
        c = skey.str[i];
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        i++;
    }
    return hash;
}

inline
u64 HashStringValue(Str key) {
    u64 hash = HashDJB2(key);
    return hash;
}

inline
u64 HashStringValue(const char *key) {
    u64 hash = HashDJB2( StrL(key) );
    return hash;
}


//
//  Hash map for pointers
//


struct KeyVal {
    u64 key;
    u64 val;
    s64 next;
};

struct HashMap {
    Array<KeyVal> slots;
    u32 collisions;
    u32 load;
    u32 overflows;

    void Print() {
        printf("load: %u, collisions: %u, overflows: %u\n", load, collisions, overflows);
    }
    void PrintElements() {
        for (s32 i = 0; i < slots.len; ++i) {
            KeyVal kv = slots.arr[i];
            printf("%d: key: %lu, val: %lu, next: %ld\n", i, kv.key, kv.val, kv.next);
        }
    }
};

HashMap InitMap(MArena *a_dest, u32 nslots = 1023) {
    HashMap map = {};
    map.slots = InitArray<KeyVal>(a_dest, nslots);
    map.slots.len = nslots;
    return map;
}

void MapClear(HashMap *map) {
    memset(map->slots.arr, 0, sizeof(KeyVal) * map->slots.len);
    map->collisions = 0;
    map->load = 0;
    map->overflows = 0;
}

struct MapIter {
    s32 slot_idx;
    s32 occ_slots_cnt;
};

s64 MapPut(HashMap *map, u64 key, u64 val) {
    assert(key != 0);

    u64 len = (u64) map->slots.len;

    // full-guard
    if (map->load == len) {
        map->overflows++;
        return -1;
    }

    KeyVal *slot0 = map->slots.arr + (key % len);
    KeyVal *slot;

    if (slot0->next || slot0->key) {
        map->collisions++;

        // find the end of the collision chain - or overwrite at matching key within the chain
        while (slot0->next && slot0->key) {

            if (slot0->key == key) {
                // overwrite
                slot0->val = val;

                return slot - map->slots.arr;
            }

            slot0 = slot0 + slot0->next;
        }
        slot = slot0;

        // fint an empty slot
        while (slot->key != 0) {
            slot++;

            // wrap-around
            if (slot == map->slots.arr + len) {
                slot = map->slots.arr;
            }
        }

        // add to the collision chain
        if (slot0 != slot) {
            slot0->next = slot - slot0;
        }

        // sanity check pointer are in range
        assert(slot >= map->slots.arr);
        assert(slot < map->slots.arr + len);
        assert(slot0 >= map->slots.arr);
        assert(slot0 < map->slots.arr + len);
    }
    else {
        slot = slot0;
    }

    slot->key = key;
    slot->val = val;
    map->load++;

    return slot - map->slots.arr;
}

u64 MapGet(HashMap *map, u64 key) {
    if (key == 0) {
        return 0;
    }
    u64 len = (u64) map->slots.len;

    // check the base slot
    KeyVal *slot = map->slots.arr + (key % len);

    if (slot->key == key) {
        return slot->val;
    }

    // iterate the collision chain
    s64 key_stop = slot->key;
    while (slot->next) {
        slot = slot + slot->next;

        if (slot->key == key) {
            return slot->val;
        }
        else if (slot->key == key_stop) {
            break;
        }
    }

    // no takers
    return 0;
}

s64 MapGetIndex(HashMap *map, u64 key, s64 *prev_idx) {
    assert(prev_idx);
    *prev_idx = -1;

    u64 len = (u64) map->slots.len;

    // check the base slot
    KeyVal *slot = map->slots.arr + (key % len);
    if (slot->key == key) {
        return slot - map->slots.arr;
    }

    // iterate the collision chain
    s64 key_stop = slot->key;
    while (slot->next) {
        *prev_idx = slot - map->slots.arr;
        slot = slot + slot->next;

        if (slot->key == key) {
            return slot - map->slots.arr;
        }
        else if (slot->key == key_stop) {
            break;
        }
    }

    // no get
    *prev_idx = -1;
    return -1;
}

s64 MapRemove(HashMap *map, u64 key) {
    u64 len = (u64) map->slots.len;
    s64 prev_idx;
    s64 remove_idx = MapGetIndex(map, key, &prev_idx);

    if (remove_idx == -1) {
        return -1;
    }

    KeyVal *remove = map->slots.arr + remove_idx;
    assert(remove->key == key);

    if (prev_idx >= 0) {
        KeyVal *prev = map->slots.arr + prev_idx;
        assert(prev->next + prev_idx == remove - map->slots.arr);

        if (prev->key == 0 && prev->val == 0) {
            assert(prev->next);
        }
        if (remove->next) {
            prev->next += remove->next;
        }
        else {
            prev->next = 0;
        }
    }

    remove->key = 0;
    remove->val = 0;
    map->load--;

    return remove_idx;
}

// wrappers
inline
s64 MapPut(HashMap *map, void *key, void *val) {
    return MapPut(map, (u64) key, (u64) val);
}
inline
s64 MapPut(HashMap *map, u64 key, void *val) {
    return MapPut(map, key, (u64) val);
}
inline
s64 MapPut(HashMap *map, Str skey, void *val) {
    return MapPut(map, HashStringValue(skey), (u64) val);
}
inline
u64 MapGet(HashMap *map, Str skey) {
    return MapGet(map, HashStringValue(skey));
}
inline
s64 MapRemove(HashMap *map, Str skey) {
    return MapRemove(map, HashStringValue(skey));
}


//
// random


#ifndef ULONG_MAX
#  define ULONG_MAX ( (u64) 0xffffffffffffffffUL )
#endif

void Kiss_SRandom(u64 state[7], u64 seed) {
    if (seed == 0) seed = 1;
    state[0] = seed | 1; // x
    state[1] = seed | 2; // y
    state[2] = seed | 4; // z
    state[3] = seed | 8; // w
    state[4] = 0;        // carry
}
u64 Kiss_Random(u64 state[7]) {
    state[0] = state[0] * 69069 + 1;
    state[1] ^= state[1] << 13;
    state[1] ^= state[1] >> 17;
    state[1] ^= state[1] << 5;
    state[5] = (state[2] >> 2) + (state[3] >> 3) + (state[4] >> 2);
    state[6] = state[3] + state[3] + state[2] + state[4];
    state[2] = state[3];
    state[3] = state[6];
    state[4] = state[5] >> 30;
    return state[0] + state[1] + state[3];
}
u64 g_kiss_randstate[7];

u32 RandInit(u32 seed = 0) {
    if (seed == 0) {
        seed = (u32) Hash(ReadSystemTimerMySec());
    }
    Kiss_SRandom(g_kiss_randstate, seed);
    Kiss_Random(g_kiss_randstate); // flush the first one

    return seed;
}

u64 RandMinMax64(u64 min, u64 max) {
    assert(max > min);

    return Kiss_Random(g_kiss_randstate) % (max - min + 1) + min;
}


f64 Rand01() {
    f64 randnum = (f64) Kiss_Random(g_kiss_randstate);
    randnum /= (f64) ULONG_MAX + 1;
    return randnum;
}
f32 Rand01_f32() {
    f32 randnum = (f32) Kiss_Random(g_kiss_randstate);
    randnum /= (f32) ULONG_MAX + 1;
    return randnum;
}
f32 RandPM1_f32() {
    f32 randnum = (f32) Kiss_Random(g_kiss_randstate);
    randnum /= ((f32) ULONG_MAX + 1) / 2;
    randnum -= 1;
    return randnum;
}
int RandMinMaxI(int min, int max) {
    assert(max > min);
    return Kiss_Random(g_kiss_randstate) % (max - min + 1) + min;
}
u32 RandMinMaxU(u32 min, u32 max) {
    assert(max > min);
    return Kiss_Random(g_kiss_randstate) % (max - min + 1) + min;
}
u32 RandMinU16(u32 min) {
    return RandMinMaxU(min, (u16) -1);
}
f32 RandMinMaxI_f32(int min, int max) {
    assert(max > min);
    return (f32) (Kiss_Random(g_kiss_randstate) % (max - min + 1) + min);
}
int RandDice(u32 max) {
    assert(max > 0);
    return Kiss_Random(g_kiss_randstate) % max + 1;
}
int RandIntMax(u32 max) {
    assert(max > 0);
    return Kiss_Random(g_kiss_randstate) % max + 1;
}

void PrintHex(u8* data, u32 len) {
    const char *nibble_to_hex = "0123456789ABCDEF";

    if (data) {
        for (u32 i = 0; i < len; ++i) {
            u8 byte = data[i];
            char a = nibble_to_hex[byte >> 4];
            char b = nibble_to_hex[byte & 0x0F];
            printf("%c%c ", a, b);

            if (i % 4 == 3 || (i == len + 1)) {
                printf("\n");
            }
        }
    }
}
void WriteRandomHexStr(char* dest, int nhexchars, bool put_newline_and_nullchar = true) {
    RandInit();

    // TODO: make use of the cool "nibble_to_hex" technique (see PrintHex)

    for (int i = 0; i < nhexchars ; i++) {
        switch (RandMinMaxI(0, 15)) {
            case 0: { *dest = '0'; break; }
            case 1: { *dest = '1'; break; }
            case 2: { *dest = '2'; break; }
            case 3: { *dest = '3'; break; }
            case 4: { *dest = '4'; break; }
            case 5: { *dest = '5'; break; }
            case 6: { *dest = '6'; break; }
            case 7: { *dest = '7'; break; }
            case 8: { *dest = '8'; break; }
            case 9: { *dest = '9'; break; }
            case 10: { *dest = 'a'; break; }
            case 11: { *dest = 'b'; break; }
            case 12: { *dest = 'c'; break; }
            case 13: { *dest = 'd'; break; }
            case 14: { *dest = 'e'; break; }
            case 15: { *dest = 'f'; break; }
            default: { assert(1==0); break; }
        };
        dest++;
    }

    if (put_newline_and_nullchar) {
        *dest = '\n';
        dest++;
        *dest = '\0';
    }
}


#endif
