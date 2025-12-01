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
    u64 hash = Hash( HashDJB2(key) );
    return hash;
}

inline
u64 HashStringValue(const char *key) {
    return HashStringValue( StrL(key) );
}



// TODO: also be a GPA?
// TODO: if max-len key (200 chars?), storage can be an array with
//      slot_size = sz(hdr) + 200 + sz(T), and with some tricks we can
//      also do un-ordered delete for removal, compressing it always


struct DictKeyVal {
    DictKeyVal *chain;  // collision chaining
    DictKeyVal *nxt;    // walk to next
    DictKeyVal *prv;    // walk to prev
    Str key;            // the full key
    void *val;          // the value
    u32 sz_val;
};


struct Dict {
    // fixed-size values dict with string keys
    u32 nslots;
    u32 sz_val;
    MArena *a_storage;
    List<u64> slots;
    DictKeyVal *head;
    DictKeyVal *tail;
    u32 ncollisions;
    bool debug_print;
};

Dict InitDict(u32 nslots = 256, u32 sz_val = 0) {
    Dict dct = {};
    dct.nslots = nslots;
    dct.sz_val = sz_val;

    MArena a = ArenaCreate();
    dct.a_storage = (MArena *) ArenaPush(&a, &a, sizeof(MArena));
    dct.slots = InitList<u64>(dct.a_storage, nslots);
    dct.slots.len = nslots;

    return dct;
}


// TODO: how do we remove chained values?


u64 DictStoragePush(Dict *dct, Str key, void *val, u32 sz_val, u64 slot_ptr) {
    DictKeyVal *hdr = NULL;
    DictKeyVal *collider = NULL;

    // collision or value reset
    bool is_collision = false;
    bool is_resetval = false;
    if (slot_ptr != 0) {
        assert(dct->head != NULL);
        assert(dct->tail != NULL);

        is_collision = true;
        collider = (DictKeyVal*) slot_ptr;
        while (true) {
            if (StrEqual(key, collider->key)) {
                hdr = collider;
                is_resetval = true;
                is_collision = false;
            }
            if (collider->chain == NULL) {
                break;
            }
            else {
                collider = collider->chain;
            }
        }
        dct->ncollisions += is_collision;
    }
    if (hdr == NULL) {
        assert(is_resetval == false);

        hdr = (DictKeyVal*) ArenaAlloc(dct->a_storage, sizeof(DictKeyVal));
    }

    // fix the linked list
    if (is_resetval) {
        // don't change anything
    }
    else if (dct->head == NULL) {
        assert(dct->tail == NULL);
        assert(is_resetval == false);
        assert(is_collision == false);

        dct->head = hdr;
        dct->tail = hdr;
    }
    else {
        assert(dct->tail != NULL);
        assert(dct->tail->nxt == NULL || is_resetval);

        dct->tail->nxt = hdr;
        hdr->prv = dct->tail;
        dct->tail = hdr;
    }

    // save key and value
    if (is_resetval) {
        assert(sz_val == collider->sz_val && "demand the same size for reset-vals");
        memcpy(collider->val, val, sz_val);
    }
    else {
        hdr->sz_val = sz_val;
        hdr->key.len = key.len;
        hdr->key.str = (char*) ArenaPush(dct->a_storage, key.str, key.len);
        hdr->val = ArenaPush(dct->a_storage, val, sz_val);
    }

    // collision chain
    if (is_collision) {
        collider->chain = hdr;
    }

    // return
    if (slot_ptr == 0) {
        return (u64) hdr;
    }
    else {
        return slot_ptr;
    }
}
void DictStorageWalk(Dict *dct) {
    DictKeyVal *kv = dct->head;
    while (kv != NULL) {
        printf("%s : %u\n", StrZ(kv->key), *((u32*) kv->val));

        kv = kv->nxt;
    }
}

void DictPut(Dict *dct, Str key, void *val, u32 sz = 0) {
    assert(sz != 0 || dct->sz_val != 0);
    if (sz == 0) {
        sz = dct->sz_val;
    }

    u32 slot = HashStringValue(key) % dct->slots.len;
    if (dct->debug_print) {
        printf("slot: %u\n", slot);
    }

    u64 ptr = dct->slots.lst[slot];
    ptr = DictStoragePush(dct, key, val, sz, ptr);
    dct->slots.lst[slot] = ptr;
}
inline
void DictPut(Dict *dct, char *key, void *val, u32 sz = 0) {
    return DictPut(dct, Str { key, (u32) strlen(key) }, val, sz);
}
inline
void DictPut(Dict *dct, const char *key, void *val, u32 sz = 0) {
    return DictPut(dct, Str { (char*) key, (u32) strlen( (char*) key) }, val, sz);
}

void *DictGet(Dict *dct, Str key) {
    u32 slot = HashStringValue(key) % dct->slots.len;
    DictKeyVal *kv = (DictKeyVal *) dct->slots.lst[slot];

    while (kv) {
        if (StrEqual(key, kv->key)) {
            return kv->val;
        }
        kv = kv->chain;
    }
    return NULL;
}
inline
void *DictGet(Dict *dct, char *key) {
    return DictGet(dct, Str { key, (u32) strlen(key) } );
}
inline
void *DictGet(Dict *dct, const char *key) {
    return DictGet(dct, Str { (char*) key, (u32) strlen((char*) key) } );
}


//
//  Pointer-map
//
//  (You need to supply a 64b key)
//


// TODO: enable a key-iteration option


struct HashMapKeyVal {
    u64 key;
    u64 val;
    HashMapKeyVal *chain;
};
struct HashMap {
    List<HashMapKeyVal> slots;
    List<HashMapKeyVal> colls;
    u32 ncollisions;
    u32 nresets;
    u32 noccupants;

    bool IsInitialized() {
        return slots.len > 0;
    }
    void PrintOccupance() {
        printf("noccupants: %u, nresets: %u\n", noccupants, nresets);
    }
};
HashMap InitMap(MArena *a_dest, u32 nslots = 1000) {
    HashMap map = {};
    map.slots = InitList<HashMapKeyVal>(a_dest, nslots);
    map.slots.len = nslots;
    map.colls = InitList<HashMapKeyVal>(a_dest, nslots);

    return map;
}
void MapClear(HashMap *map) {
    u32 nslots = map->slots.len;
    _memzero(map->colls.lst, sizeof(HashMapKeyVal) * map->slots.len);
    _memzero(map->slots.lst, sizeof(HashMapKeyVal) * map->slots.len);
    map->ncollisions = 0;
    map->noccupants = 0;
    map->nresets = 0;
    map->colls.len = 0;
    map->slots.len = nslots;
}

struct MapIter {
    s32 slot_idx;
    s32 coll_idx;

    s32 occ_slots_cnt = 0;
    s32 occ_colliders_cnt = 0;
};

u64 MapNextVal(HashMap *map, MapIter *iter) {
    while (iter->slot_idx < (s32) map->slots.len) {
        HashMapKeyVal kv = map->slots.lst[iter->slot_idx++];

        if (kv.val) {
            iter->occ_slots_cnt++;

            return kv.val;
        }

    }
    while (iter->coll_idx < (s32) map->colls.len) {
        HashMapKeyVal kv = map->colls.lst[iter->coll_idx++];
        if (kv.val) {
            iter->occ_colliders_cnt++;

            return kv.val;
        }
    }

    return 0;
}

bool MapPut(HashMap *map, u64 key, u64 val) {
    assert(key != 0 && "null ptr can not be used as a key");

    u32 slot = Hash(key) % map->slots.len;
    HashMapKeyVal *kv_slot = map->slots.lst + slot;

    if (kv_slot->key == 0 || kv_slot->key == key) {
        if (kv_slot->key == key) {
            map->noccupants--;
            map->nresets++;
        }
        // new slot or reset value
        HashMapKeyVal kv;
        kv.key = key;
        kv.val = val;
        kv.chain = kv_slot->chain;
        map->slots.lst[slot] = kv;
    }
    else {
        // collision
        map->ncollisions++;

        HashMapKeyVal *collider = kv_slot;
        while (collider->chain != NULL) {
            collider = collider->chain;
        }
        if (map->colls.len == map->slots.len) {
            // no more space
            return false;
        }

        // put a new collider onto the list
        HashMapKeyVal kv_new = {};
        kv_new.key = key;
        kv_new.val = val;

        collider->chain = map->colls.Add(kv_new);
    }
    map->noccupants++;
    return true;
}
inline
bool MapPut(HashMap *map, void *key, void *val) {
    return MapPut(map, (u64) key, (u64) val);
}

inline
bool MapPut(HashMap *map, u64 key, void *val) {
    return MapPut(map, key, (u64) val);
}

inline
bool MapPut(HashMap *map, Str skey, void *val) {
    return MapPut(map, HashStringValue(skey), (u64) val);
}

u64 MapGet(HashMap *map, u64 key) {

    HashMapKeyVal kv_slot = {};
    if (map->slots.len) {

        u32 slot = Hash(key) % map->slots.len;
        kv_slot = map->slots.lst[slot];
    }

    if (kv_slot.key == key) {
        return kv_slot.val;
    }
    else {
        HashMapKeyVal *kv = &kv_slot;
        while (kv->chain != NULL) {
            kv = kv->chain;
            if (kv->key == key) {
                return kv->val;
            }
        }
    }

    return 0;
}
inline
u64 MapGet(HashMap *map, Str skey) {
    return MapGet(map, HashStringValue(skey));
}

bool MapRemove(HashMap *map, u64 key, void *val) {
    u32 slot = Hash(key) % map->slots.len;
    HashMapKeyVal kv_slot = map->slots.lst[slot];

    if (kv_slot.key == key) {
        map->slots.lst[slot] = {};
        return true;
    }
    else {
        HashMapKeyVal *kv_prev = map->slots.lst + slot;
        HashMapKeyVal *kv = kv_slot.chain;
        while (kv != NULL) {
            if (kv->key == key) {
                kv_prev->chain = kv->chain;
                *kv = {};
                return true;
            }
            else {
                kv_prev = kv;
                kv = kv->chain;
            }
        }
    }

    return false;
}



//
// random


// TODO: reference ReadCPUTimer()


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
#define McRandom() Kiss_Random(g_kiss_randstate)
u32 RandInit(u32 seed = 0) {
    if (seed == 0) {
        seed = (u32) Hash(ReadSystemTimerMySec());
    }
    Kiss_SRandom(g_kiss_randstate, seed);
    Kiss_Random(g_kiss_randstate); // flush the first one

    return seed;
}

f64 Rand01() {
    f64 randnum = (f64) McRandom();
    randnum /= (f64) ULONG_MAX + 1;
    return randnum;
}
f32 Rand01_f32() {
    f32 randnum = (f32) McRandom();
    randnum /= (f32) ULONG_MAX + 1;
    return randnum;
}
f32 RandPM1_f32() {
    f32 randnum = (f32) McRandom();
    randnum /= ((f32) ULONG_MAX + 1) / 2;
    randnum -= 1;
    return randnum;
}
int RandMinMaxI(int min, int max) {
    assert(max > min);
    return McRandom() % (max - min + 1) + min;
}
u32 RandMinMaxU(u32 min, u32 max) {
    assert(max > min);
    return McRandom() % (max - min + 1) + min;
}
u32 RandMinU16(u32 min) {
    return RandMinMaxU(min, (u16) -1);
}
f32 RandMinMaxI_f32(int min, int max) {
    assert(max > min);
    return (f32) (McRandom() % (max - min + 1) + min);
}
int RandDice(u32 max) {
    assert(max > 0);
    return McRandom() % max + 1;
}
int RandIntMax(u32 max) {
    assert(max > 0);
    return McRandom() % max + 1;
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
