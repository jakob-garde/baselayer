#ifndef __STRING_H__
#define __STRING_H__

#include <cstdio>
#include <cassert>
#include <stdarg.h>


//
//  Init


static MArena _g_a_strings;
static MArena *g_a_strings;
static MArena _g_a_string_interns;
static MArena *g_a_string_interns;

static MArena *g_a_strings_cached;
static MArena *g_a_string_interns_cached;

void StrSetArenas(MArena *a_tmp, MArena *a_intern) {
    g_a_strings_cached = g_a_strings;
    g_a_string_interns_cached = g_a_string_interns;

    g_a_strings = a_tmp;
    g_a_string_interns = a_intern;
}
void StrPopArenas() {
    g_a_strings = g_a_strings_cached;
    g_a_string_interns = g_a_string_interns_cached;
}

void StrInit() {
    if (g_a_strings == NULL) {
        _g_a_strings = ArenaCreate();
        g_a_strings = &_g_a_strings;
    }
    if (g_a_string_interns == NULL) {
        _g_a_string_interns = ArenaCreate();
        g_a_string_interns = &_g_a_strings;
    }
}

MArena *StrGetTmpArena() {
    return g_a_strings;
}

MArena *StrGetInternArena() {
    return g_a_string_interns;
}


//
//  Str


struct Str {
    char *str;
    u32 len;
};


inline
Str StrAlloc(MArena *a_dest, u32 len) {
    char *buff = (char*) ArenaAlloc(a_dest, len);
    return Str { buff, len };
}

inline
Str StrAlloc(u32 len) {
    char *buff = (char*) ArenaAlloc(g_a_strings, len);
    return Str { buff, len };
}

Str StrIntern(Str s) { // NOTE: not an actual interna, it just pushes the string to a the current lifetime arena
    Str s_dest = {};
    if (s.len) {
        s_dest = StrAlloc(g_a_string_interns, s.len);
        memcpy(s_dest.str, s.str, s.len);
    }
    return s_dest;
}

Str StrPush(MArena *a_dest, Str s) {
    Str s_dest = {};
    if (s.len) {
        s_dest = StrAlloc(a_dest, s.len);
        memcpy(s_dest.str, s.str, s.len);
    }
    return s_dest;
}

char *StrZ(Str s, bool check_unsafe = false) {
    if (check_unsafe && s.str[s.len] == '\0') {
        return s.str;
    }
    else {
        char * zt = (char*) ArenaAlloc(g_a_strings, s.len + 1);
        memcpy(zt, s.str, s.len);
        zt[s.len] = 0;
        return zt;
    }
}

inline
char *StrZU(Str s) {
    return StrZ(s, true);
}

inline
Str StrL(const char *str) { // StrLiteral
    return Str { (char*) str, (u32) strlen((char*) str) };
}

Str StrL(char *str) {
    Str s;
    s.len = 0;
    while (*(str + s.len) != '\0') {
        ++s.len;
    }
    s.str = (char*) ArenaAlloc(g_a_strings, s.len);
    memcpy(s.str, str, s.len);

    return s;
}


//
//  String API


inline
void StrPrint(Str s) {
    printf("%.*s", s.len, s.str);
}

inline
void StrPrint(const char *aff, Str s, const char *suf) {
    printf("%s%.*s%s", aff, s.len, s.str, suf);
}

inline
void StrPrint(Str *s) {
    printf("%.*s", s->len, s->str);
}

Str StrSPrint(const char *format, s32 cnt, ...) {
    ArenaEnsureSpace(g_a_strings, KILOBYTE);
    Str s = {};
    s.str = (char*) ArenaAlloc(g_a_strings, 0);

    va_list args;
    va_start(args, cnt);

    s.len = vsnprintf(s.str, KILOBYTE, format, args);
    ArenaAlloc(g_a_strings, s.len, false);

    va_end(args);

    return s;
}

bool StrEqual(Str a, Str b) {
    u32 i = 0;
    u32 len = MinU32(a.len, b.len);
    while (i < len) {
        if (a.str[i] != b.str[i]) {
            return false;
        }
        ++i;
    }

    return a.len == b.len;
}

inline
bool StrEqual(Str a, const char *b) {
    return StrEqual(a, StrL(b));
}

bool StrContainsChar(Str s, char c) {
    for (u32 i = 0; i < s.len; ++i) {
        if (c == s.str[i]) {
            return true;
        }
    }
    return false;
}

Str StrCat(Str a, Str b) {
    Str cat;
    cat.len = a.len + b.len;
    cat.str = (char*) ArenaAlloc(g_a_strings, cat.len);
    memcpy(cat.str, a.str, a.len);
    memcpy(cat.str + a.len, b.str, b.len);

    return cat;
}

Str StrCat(Str a, const char *b) {
    return StrCat(a, StrL(b));
}

Str StrTrim(Str s, char t) {
    if (s.len && s.str[0] == t) {
            s.str++;
            s.len -= 1;
    }
    if (s.len && (s.str[s.len-1] == t)) {
        s.len -= 1;
    }
    return s;
}

void StrCopy(Str src, Str dest) {
    assert(src.str && dest.str);

    for (s32 i = 0; i < MinS32( src.len, dest.len ); ++i) {
        dest.str[i] = src.str[i];
    }
}

Str StrInsertReplace(Str src, Str amend, Str at) {
    Str before = src;
    before.len = (u32) (at.str - src.str);

    Str after = {};
    after.len = src.len - before.len - at.len;
    after.str = (src.str + src.len) - after.len;

    s32 len = src.len - at.len + amend.len;
    Str result = StrAlloc(len);
    result = StrCat(before, amend);
    result = StrCat(result, after);

    return result;
}


//
//  StrLst -> linked string list


struct StrLst {
    char *str;
    u32 len;
    StrLst *next;
    StrLst *first;

    Str GetStr() {
        return Str { str, len };
    }
    void SetStr(char * s) {
        str = s;
        len = (u32) strlen(s);
    }
};


void StrLstSetToFirst(StrLst **lst) {
    if ((*lst)->first) {
        *lst = (*lst)->first;
    }
}

u32 StrListLen(StrLst *lst, u32 limit = -1) {
    if (lst == NULL) {
        return 0;
    }
    StrLstSetToFirst(&lst);

    u32 cnt = 0;
    while (lst && cnt < limit) {
        cnt++;
        lst = lst->next;
    }
    return cnt;
}

void StrLstPrint(StrLst *lst, const char *sep = "\n") {
    while (lst != NULL) {
        StrPrint(lst->GetStr()); // TODO: fix
        printf("%s", sep);
        lst = lst->next;
    }
}

StrLst *_StrLstAllocNext() {
    MArena *a = g_a_strings;

    static StrLst def;
    StrLst *lst = (StrLst*) ArenaPush(a, &def, sizeof(StrLst));
    lst->str = (char*) ArenaAlloc(a, 0);
    return lst;
}

StrLst *StrSplit(Str base, char split) {
    MArena *a = g_a_strings;

    StrLst *next = _StrLstAllocNext();
    StrLst *first = next;
    StrLst *node = next;

    u32 i = 0;
    u32 j = 0;
    while (i < base.len) {
        // seek
        j = 0;
        while (i + j < base.len && base.str[i + j] != split) {
            j++;
        }

        // copy
        if (j > 0) {
            if (node->len > 0) {
                next = _StrLstAllocNext();
                node->next = next;
                node->first = first;
                node = next;
            }

            node->len = j;
            ArenaAlloc(a, j);
            memcpy(node->str, base.str + i, j);
        }

        // iter
        i += j + 1;
    }
    return first;
}

StrLst *StrSplitSpacesKeepQuoted(Str base) {
    MArena *a = g_a_strings;

    char space = ' ';
    char quote = '"';

    u32 qcnt = 0;
    bool e1 = false;
    bool e2 = false;
    for (u32 i = 0; i < base.len; ++i) {
        if (base.str[i] == quote) {
            qcnt++;

            // check whether the quotation thing is padded with space on left/right side (uneven / even)
            bool qts_are_wrapped_by_spaces =
                (qcnt % 2 == 1) && (i == 0 || base.str[i - 1] == ' ') ||
                (qcnt % 2 == 0) && (i == base.len - 1 || base.str[i + 1] == ' ');

            if (!qts_are_wrapped_by_spaces) {
                e1 = true;
            }
        }
    }
    e2 = qcnt % 2 == 1;

    bool debug_print = false;
    if (e1 || e2) {
        if (debug_print) printf("FAIL: qcnt: %u\n", qcnt);
        if (debug_print) printf("FAIL: fail %d %d\n", e1, e2);
        return NULL;
    }
    else {
        if (debug_print) printf("able to collapse\n");
    }

    char split = space;
    StrLst *next = _StrLstAllocNext();
    StrLst *first = next;
    StrLst *node = next;

    u32 i = 0;
    u32 j = 0;
    while (i < base.len) {
        // seek
        j = 0;
        while (i + j < base.len && (base.str[i + j] != split) ) {
            char c = base.str[i + j];

            if (c == quote) {
                i++;
                if (split == space) {
                    split = quote;
                }
                else {
                    split = space;
                }
            }

            j++;
        }

        // copy
        if (j > 0) {
            if (node->len > 0) {
                next = _StrLstAllocNext();
                node->next = next;
                node->first = first;
                node = next;
            }

            node->len = j;
            ArenaAlloc(a, j);
            memcpy(node->str, base.str + i, j);
        }

        // iter
        split = space;
        i += j + 1;
    }
    return first;
}

Str StrJoin(MArena *a, StrLst *strs) {
    // TODO: we should just ArenaAlloc() every time there is a new string

    u32 amount_needed = 0;
    while (strs != NULL) {
        amount_needed += strs->len;
        strs = strs->next;
    }

    Str join;
    join.str = (char*) ArenaAlloc(a, amount_needed);
    join.len = 0;
    while (strs != NULL) {
        memcpy(join.str + join.len, strs->str, strs->len);
        join.len += strs->len;
        strs = strs->next;
    }

    return join;
}

Str StrJoinInsertChar(MArena *a, StrLst *strs, char insert) {
    // TODO: we should just ArenaAlloc() every time there is a new string

    u32 amount_needed = 0;
    while (strs != NULL) {
        amount_needed += strs->len;

        strs = strs->next;
        if (strs != NULL) {
            amount_needed++;
        }
    }

    Str join;
    join.str = (char*) ArenaAlloc(a, amount_needed);
    join.len = 0;
    while (strs != NULL) {
        memcpy(join.str + join.len, strs->str, strs->len);
        join.len += strs->len;
        strs = strs->next;

        if (strs != NULL) {
            join.str[join.len] = insert;
            ++join.len;
        }
    }

    return join;
}


//
//  StrLst: string list builder functions


StrLst *StrLstPush(char *str, StrLst *after = NULL) {
    MArena *a = g_a_strings;

    StrLst _ = {};
    StrLst *lst = (StrLst*) ArenaPush(a, &_, sizeof(StrLst));
    lst->len = (u32) strlen(str);
    lst->str = (char*) ArenaAlloc(a, lst->len + 1);
    lst->str[lst->len] = 0;

    memcpy(lst->str, str, lst->len);

    if (after != NULL) {
        assert(after->first && "ensure first is set");

        after->next = lst;
        lst->first = after->first;
    }
    else {
        lst->first = lst;
    }
    return lst;
}

StrLst *StrLstPush(const char *str, StrLst *after = NULL) {
    return StrLstPush((char*) str, after);
}

StrLst *StrLstPush(Str str, StrLst *after = NULL) {
    return StrLstPush(StrZ(str), after);
}

Str StrLstNext(StrLst **lst) {
    Str s = (*lst)->GetStr();
    *lst = (*lst)->next;
    return s;
}

void StrLstPrint(StrLst lst) {
    StrLst *iter = &lst;
    do {
        StrPrint(iter->GetStr());
        printf("\n");
    }
    while ((iter = iter->next) != NULL);
}

StrLst *StrLstPop(StrLst *pop, StrLst *prev) {
    if (pop == NULL) {
        return NULL;
    }
    else if (pop->first == NULL && pop->next == NULL) {
        return NULL;
    }

    // pop is first
    else if (pop == pop->first) { 
        assert(prev == NULL);

        StrLst *newfirst = pop->next;
        StrLst *iter = newfirst;
        while (iter) {
            iter->first = newfirst;
            iter = iter->next;
        }
        pop->first = newfirst; // this line is just a safeguard in case someone uses first on the item
        return newfirst;
    }

    else if (prev != NULL) {
        assert(prev->next == pop);

        // pop is in the middle
        if (pop->next) {
            prev->next = pop->next;
        }

        // pop is the last
        else {
            prev->next = NULL;
        }
    }

    return pop->first;
}


//
//  Path / filename helpers


Str StrBasename(char *path) {
    // TODO: re-impl. using StrLst navigation, e.g. StrLstLast(); StrLstLen(); StrLstAtIdx(len - 2);
    //      To robustly find the basename.

    StrLst *before_ext = StrSplit(StrL(path), '.');
    StrLst *iter = before_ext;
    while (iter->next) {
        iter = iter->next;
        if (iter->next == NULL) {
            break;
        }
        else {
            before_ext = iter;
        }
    }

    StrLst* slashes = StrSplit(before_ext->GetStr(), '/');
    while (slashes->next) {
        slashes = slashes->next;
    }

    return slashes->GetStr();
}

inline
Str StrBasename(Str path) {
    return StrBasename( StrZ(path) );
}

Str StrExtension(Str path) {
    assert(g_a_strings != NULL && "init strings first");

    Str result { NULL, 0 };
    StrLst *lst = StrSplit(path, '.');
    bool has_extension = (lst->next != NULL);

    while (lst->next != NULL) {
        lst = lst->next;
    }

    if (has_extension) {
        result = lst->GetStr();
    }
    else {
        result = {};
    }

    return result;
}

Str StrExtension(char *path) {
    return StrExtension(StrL(path));
}

Str StrExtension(const char *path) {
    return StrExtension(StrL(path));
}

Str StrDirPath(Str path) {
    assert(g_a_strings != NULL && "init strings first");

    // with no extension, we assume dir
    if (StrExtension(path).len == 0) {
        return path;
    }

    StrLst *slash = StrSplit(path, '/');
    u32 len = StrListLen(slash);

    Str cat = StrL("");
    for (u32 i = 0; i < len - 1; ++i) {
        cat = StrCat(cat, slash->GetStr());
        cat = StrCat(cat, StrL("/"));
        slash = slash->next;
    }
    return cat;
}

Str StrPathBuild(Str dirname, Str basename, Str ext) {
    dirname = StrTrim(dirname, '/');
    basename = StrTrim(basename, '/');

    Str path = dirname;
    if (dirname.len) {
       path = StrCat(dirname, StrL("/"));
    }
    path = StrCat(path, basename);
    path = StrCat(path, StrL("."));
    path = StrCat(path, ext);

    return path;
}

Str StrPathJoin(Str path_1, Str path_2) {
    Str path = StrCat(path_1, StrL("/"));
    path = StrCat(path, path_2);
    return path;
}


//
//  StrBuff


struct StrBuff {
    char *str;
    u32 len;
    MArena a;

    Str GetStr() {
        return Str { str, len };
    }
};

StrBuff StrBuffInit() {
    StrBuff buff = {};
    buff.a = ArenaCreate();
    buff.str = (char*) ArenaAlloc(&buff.a, 0);
    return buff;
}

s32 StrBuffPrint1K(StrBuff *buff, const char *format, s32 cnt, ...) {
    ArenaEnsureSpace(&buff->a, KILOBYTE);

    va_list args;
    va_start(args, cnt);

    s32 len = vsnprintf(buff->str + buff->len, KILOBYTE, format, args);
    buff->len += len;
    buff->a.used += len;

    va_end(args);
    return len;
}

s32 StrBuffAppend(StrBuff *buff, Str put) {
    ArenaEnsureSpace(&buff->a, put.len);

    memcpy(buff->str + buff->len, put.str, put.len);
    buff->len += put.len;
    buff->a.used += put.len;

    return put.len;
}

s32 StrBuffAppendConst(StrBuff *buff, const char* text) {
    u32 len = (u32) strlen(text);
    StrBuffAppend(buff, Str { (char*) text, len } );
    return (s32) len;
}

s32 StrBuffNewLine(StrBuff *buff) {
    return StrBuffPrint1K(buff, "\n", 1);
}

void StrBuffClear(StrBuff *buff) {
    ArenaClear(&buff->a);
    buff->str = (char*) ArenaAlloc(&buff->a, 0);
    buff->len = 0;
}


#endif
