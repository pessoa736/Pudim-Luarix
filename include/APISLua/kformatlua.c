#include "kformatlua.h"
#include "kfs.h"
#include "kheap.h"

/*
 * PLF (Pudim Lua Format) — text-based serialization for Lua tables.
 *
 * Format:
 *   PLF:1\n
 *   s:key=string value\n
 *   n:key=42\n
 *   b:key=true\n
 *   s:parent.child=nested value\n
 *   s:arr.1=first element\n
 *
 * Type prefixes: s=string, n=number, b=boolean
 * Nested tables use dotted paths. Integer path segments become array indices.
 * String values escape: \n → \\n, \r → \\r, \\ → \\\\
 */

#define KFORMAT_MAX_ENCODE_SIZE 65536u
#define KFORMAT_MAX_KEY_LEN     256u
#define KFORMAT_MAX_VALUE_LEN   2048u
#define KFORMAT_MAX_DEPTH       8u
#define KFORMAT_HEADER          "PLF:1\n"
#define KFORMAT_HEADER_LEN      6u

/* ---- Buffer writer ---- */

typedef struct {
    char* buf;
    unsigned int pos;
    unsigned int cap;
} kformat_buf_t;

static int kformat_buf_putc(kformat_buf_t* b, char c) {
    if (b->pos + 1 >= b->cap) {
        return 0;
    }

    b->buf[b->pos++] = c;
    b->buf[b->pos] = 0;
    return 1;
}

static int kformat_buf_puts(kformat_buf_t* b, const char* s) {
    if (!s) {
        return 1;
    }

    while (*s) {
        if (b->pos + 1 >= b->cap) {
            return 0;
        }

        b->buf[b->pos++] = *s++;
    }

    b->buf[b->pos] = 0;
    return 1;
}

static void kformat_buf_puts_escaped(kformat_buf_t* b, const char* s) {
    if (!s) {
        return;
    }

    while (*s && b->pos + 2 < b->cap) {
        if (*s == '\n') {
            b->buf[b->pos++] = '\\';
            b->buf[b->pos++] = 'n';
        } else if (*s == '\r') {
            b->buf[b->pos++] = '\\';
            b->buf[b->pos++] = 'r';
        } else if (*s == '\\') {
            b->buf[b->pos++] = '\\';
            b->buf[b->pos++] = '\\';
        } else {
            b->buf[b->pos++] = *s;
        }

        s++;
    }

    b->buf[b->pos] = 0;
}

/* ---- Integer to string ---- */

static void kformat_int_to_str(lua_Integer value, char* out, unsigned int outsize) {
    lua_Unsigned magnitude;
    char tmp[32];
    unsigned int n = 0;
    unsigned int p = 0;

    if (!out || outsize < 2) {
        return;
    }

    if (value < 0) {
        magnitude = (lua_Unsigned)(-(value + 1)) + 1u;
        if (p + 1 < outsize) {
            out[p++] = '-';
        }
    } else {
        magnitude = (lua_Unsigned)value;
    }

    if (magnitude == 0) {
        out[p++] = '0';
        out[p] = 0;
        return;
    }

    while (magnitude > 0 && n < sizeof(tmp)) {
        tmp[n++] = (char)('0' + (magnitude % 10u));
        magnitude /= 10u;
    }

    while (n > 0 && p + 1 < outsize) {
        out[p++] = tmp[--n];
    }

    out[p] = 0;
}

/* ---- Key path builder ---- */

static int kformat_build_key(lua_State* L, int key_idx, const char* prefix,
                              char* out, unsigned int outsize) {
    unsigned int pos = 0;
    const char* src;
    char numbuf[32];

    if (!out || outsize < 2) {
        return 0;
    }

    if (prefix && prefix[0]) {
        src = prefix;
        while (*src && pos + 1 < outsize) {
            out[pos++] = *src++;
        }

        if (pos + 1 < outsize) {
            out[pos++] = '.';
        }
    }

    if (lua_type(L, key_idx) == LUA_TSTRING) {
        src = lua_tostring(L, key_idx);
    } else if (lua_type(L, key_idx) == LUA_TNUMBER && lua_isinteger(L, key_idx)) {
        kformat_int_to_str(lua_tointeger(L, key_idx), numbuf, sizeof(numbuf));
        src = numbuf;
    } else {
        out[pos] = 0;
        return 0;
    }

    while (*src && pos + 1 < outsize) {
        out[pos++] = *src++;
    }

    out[pos] = 0;
    return pos > 0;
}

/* ---- Encoder ---- */

static int kformat_encode_table(lua_State* L, int tab_idx, kformat_buf_t* buf,
                                 const char* prefix, unsigned int depth) {
    int abs_idx;

    if (depth > KFORMAT_MAX_DEPTH) {
        return 0;
    }

    abs_idx = lua_absindex(L, tab_idx);
    lua_pushnil(L);

    while (lua_next(L, abs_idx) != 0) {
        char fullkey[KFORMAT_MAX_KEY_LEN];
        int vtype = lua_type(L, -1);

        if (!kformat_build_key(L, -2, prefix, fullkey, sizeof(fullkey))) {
            lua_pop(L, 1);
            continue;
        }

        switch (vtype) {
            case LUA_TSTRING: {
                const char* val = lua_tostring(L, -1);
                kformat_buf_puts(buf, "s:");
                kformat_buf_puts(buf, fullkey);
                kformat_buf_putc(buf, '=');
                kformat_buf_puts_escaped(buf, val);
                kformat_buf_putc(buf, '\n');
                break;
            }

            case LUA_TNUMBER: {
                kformat_buf_puts(buf, "n:");
                kformat_buf_puts(buf, fullkey);
                kformat_buf_putc(buf, '=');

                if (lua_isinteger(L, -1)) {
                    char numbuf[32];
                    kformat_int_to_str(lua_tointeger(L, -1), numbuf, sizeof(numbuf));
                    kformat_buf_puts(buf, numbuf);
                } else {
                    /* Push copy to avoid corrupting table iteration key */
                    lua_pushvalue(L, -1);
                    kformat_buf_puts(buf, lua_tostring(L, -1));
                    lua_pop(L, 1);
                }

                kformat_buf_putc(buf, '\n');
                break;
            }

            case LUA_TBOOLEAN: {
                kformat_buf_puts(buf, "b:");
                kformat_buf_puts(buf, fullkey);
                kformat_buf_putc(buf, '=');
                kformat_buf_puts(buf, lua_toboolean(L, -1) ? "true" : "false");
                kformat_buf_putc(buf, '\n');
                break;
            }

            case LUA_TTABLE: {
                kformat_encode_table(L, -1, buf, fullkey, depth + 1);
                break;
            }

            default:
                break;
        }

        lua_pop(L, 1); /* pop value, keep key for lua_next */
    }

    return 1;
}

/* format.encode(table) → string  |  nil, error */
static int kformatlua_encode(lua_State* L) {
    kformat_buf_t buf;

    if (!lua_istable(L, 1)) {
        lua_pushnil(L);
        lua_pushstring(L, "expected table");
        return 2;
    }

    buf.buf = (char*)kmalloc(KFORMAT_MAX_ENCODE_SIZE);
    if (!buf.buf) {
        lua_pushnil(L);
        lua_pushstring(L, "out of memory");
        return 2;
    }

    buf.pos = 0;
    buf.cap = KFORMAT_MAX_ENCODE_SIZE;
    buf.buf[0] = 0;

    kformat_buf_puts(&buf, KFORMAT_HEADER);
    kformat_encode_table(L, 1, &buf, NULL, 0);

    lua_pushstring(L, buf.buf);
    kfree(buf.buf);
    return 1;
}

/* ---- Decoder helpers ---- */

static int kformat_is_int_segment(const char* s, unsigned int len, lua_Integer* out) {
    lua_Integer val = 0;
    unsigned int i = 0;

    if (!s || len == 0) {
        return 0;
    }

    while (i < len) {
        if (s[i] < '0' || s[i] > '9') {
            return 0;
        }

        val = val * 10 + (s[i] - '0');
        i++;
    }

    *out = val;
    return 1;
}

static void kformat_unescape(const char* src, unsigned int srclen,
                               char* dst, unsigned int dstsize) {
    unsigned int si = 0;
    unsigned int di = 0;

    if (!dst || dstsize == 0) {
        return;
    }

    if (!src) {
        dst[0] = 0;
        return;
    }

    while (si < srclen && di + 1 < dstsize) {
        if (src[si] == '\\' && si + 1 < srclen) {
            si++;
            if (src[si] == 'n') {
                dst[di++] = '\n';
            } else if (src[si] == 'r') {
                dst[di++] = '\r';
            } else if (src[si] == '\\') {
                dst[di++] = '\\';
            } else {
                /* Unknown escape: keep both chars */
                if (di + 2 < dstsize) {
                    dst[di++] = '\\';
                    dst[di++] = src[si];
                }
            }
            si++;
        } else {
            dst[di++] = src[si++];
        }
    }

    dst[di] = 0;
}

/* Push a typed value on the Lua stack */
static void kformat_push_typed_value(lua_State* L, char type,
                                      const char* raw, unsigned int raw_len) {
    switch (type) {
        case 's': {
            char* unesc = (char*)kmalloc(raw_len + 1);
            if (!unesc) {
                lua_pushstring(L, "");
                break;
            }

            kformat_unescape(raw, raw_len, unesc, raw_len + 1);
            lua_pushstring(L, unesc);
            kfree(unesc);
            break;
        }

        case 'n': {
            /* Copy to null-terminated buffer for lua_stringtonumber */
            char numbuf[64];
            unsigned int i;

            if (raw_len >= sizeof(numbuf)) {
                raw_len = sizeof(numbuf) - 1;
            }

            for (i = 0; i < raw_len; i++) {
                numbuf[i] = raw[i];
            }
            numbuf[raw_len] = 0;

            if (lua_stringtonumber(L, numbuf) == 0) {
                lua_pushinteger(L, 0);
            }
            break;
        }

        case 'b': {
            lua_pushboolean(L, raw_len >= 4 && raw[0] == 't' &&
                            raw[1] == 'r' && raw[2] == 'u' && raw[3] == 'e');
            break;
        }

        default:
            lua_pushnil(L);
            break;
    }
}

/*
 * Navigate a dotted key path, creating intermediate tables as needed,
 * then push the typed value and set it at the leaf.
 */
static void kformat_set_nested(lua_State* L, int table_idx, const char* path,
                                char type, const char* raw_value, unsigned int raw_len) {
    int abs_tab = lua_absindex(L, table_idx);
    unsigned int pi = 0;
    unsigned int pathlen = 0;
    int last_dot = -1;
    unsigned int j;
    lua_Integer ikey;

    while (path[pathlen]) {
        pathlen++;
    }

    for (j = 0; j < pathlen; j++) {
        if (path[j] == '.') {
            last_dot = (int)j;
        }
    }

    /* No dots: set directly on table */
    if (last_dot < 0) {
        kformat_push_typed_value(L, type, raw_value, raw_len);

        if (kformat_is_int_segment(path, pathlen, &ikey)) {
            lua_rawseti(L, abs_tab, ikey);
        } else {
            lua_setfield(L, abs_tab, path);
        }
        return;
    }

    /* Navigate through intermediate tables */
    lua_pushvalue(L, abs_tab);
    pi = 0;

    while ((int)pi <= last_dot) {
        char segment[KFORMAT_MAX_KEY_LEN];
        unsigned int si = 0;

        while ((int)pi < last_dot + 1 && path[pi] != '.' &&
               si + 1 < sizeof(segment)) {
            segment[si++] = path[pi++];
        }
        segment[si] = 0;

        if (path[pi] == '.') {
            pi++;
        }

        /* Get or create subtable */
        if (kformat_is_int_segment(segment, si, &ikey)) {
            lua_rawgeti(L, -1, ikey);
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                lua_newtable(L);
                lua_pushvalue(L, -1);
                lua_rawseti(L, -3, ikey);
            }
        } else {
            lua_getfield(L, -1, segment);
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                lua_newtable(L);
                lua_pushvalue(L, -1);
                lua_setfield(L, -3, segment);
            }
        }

        /* Remove parent ref, keep subtable */
        lua_remove(L, -2);
    }

    /* Set leaf value */
    {
        const char* leaf = &path[last_dot + 1];
        unsigned int leaf_len = pathlen - (unsigned int)(last_dot + 1);

        kformat_push_typed_value(L, type, raw_value, raw_len);

        if (kformat_is_int_segment(leaf, leaf_len, &ikey)) {
            lua_rawseti(L, -2, ikey);
        } else {
            lua_setfield(L, -2, leaf);
        }
    }

    lua_pop(L, 1); /* pop working table */
}

/* format.decode(string) → table  |  nil, error */
static int kformatlua_decode(lua_State* L) {
    const char* data;
    unsigned int dlen = 0;
    unsigned int pos;
    int tab_idx;

    if (!lua_isstring(L, 1)) {
        lua_pushnil(L);
        lua_pushstring(L, "expected string");
        return 2;
    }

    data = lua_tostring(L, 1);
    if (!data) {
        lua_pushnil(L);
        lua_pushstring(L, "null string");
        return 2;
    }

    while (data[dlen]) {
        dlen++;
    }

    /* Check header */
    if (dlen < KFORMAT_HEADER_LEN ||
        data[0] != 'P' || data[1] != 'L' || data[2] != 'F' ||
        data[3] != ':' || data[4] != '1' || data[5] != '\n') {
        lua_pushnil(L);
        lua_pushstring(L, "invalid PLF header");
        return 2;
    }

    pos = KFORMAT_HEADER_LEN;
    lua_newtable(L);
    tab_idx = lua_gettop(L);

    while (pos < dlen) {
        unsigned int line_start = pos;
        unsigned int line_end;
        char type_char;
        unsigned int key_start;
        unsigned int key_end;
        unsigned int val_start;
        unsigned int val_len;
        char key[KFORMAT_MAX_KEY_LEN];
        unsigned int klen;

        /* Find end of line */
        while (pos < dlen && data[pos] != '\n') {
            pos++;
        }
        line_end = pos;
        if (pos < dlen) {
            pos++;
        }

        /* Minimum line: "t:k=v" = 5 chars */
        if (line_end - line_start < 5) {
            continue;
        }

        /* Parse type */
        type_char = data[line_start];
        if (data[line_start + 1] != ':') {
            continue;
        }
        if (type_char != 's' && type_char != 'n' && type_char != 'b') {
            continue;
        }

        /* Parse key (until '=') */
        key_start = line_start + 2;
        key_end = key_start;
        while (key_end < line_end && data[key_end] != '=') {
            key_end++;
        }
        if (key_end >= line_end) {
            continue;
        }

        klen = key_end - key_start;
        if (klen == 0 || klen >= KFORMAT_MAX_KEY_LEN) {
            continue;
        }

        {
            unsigned int ki;
            for (ki = 0; ki < klen; ki++) {
                key[ki] = data[key_start + ki];
            }
            key[klen] = 0;
        }

        /* Value starts after '=' */
        val_start = key_end + 1;
        val_len = line_end - val_start;

        kformat_set_nested(L, tab_idx, key, type_char,
                            &data[val_start], val_len);
    }

    return 1;
}

/* format.is_valid(string) → boolean */
static int kformatlua_is_valid(lua_State* L) {
    const char* data;

    if (!lua_isstring(L, 1)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    data = lua_tostring(L, 1);
    lua_pushboolean(L, data &&
                    data[0] == 'P' && data[1] == 'L' && data[2] == 'F' &&
                    data[3] == ':' && data[4] == '1' && data[5] == '\n');
    return 1;
}

/* format.disk_init() → boolean  (formats disk with empty PLFS) */
static int kformatlua_disk_init(lua_State* L) {
    (void)L;
    kfs_clear();
    lua_pushboolean(L, kfs_persist_save());
    return 1;
}

/* format.disk_info() → table */
static int kformatlua_disk_info(lua_State* L) {
    lua_createtable(L, 0, 4);

    lua_pushstring(L, "PLFS");
    lua_setfield(L, -2, "magic");

    lua_pushinteger(L, 1);
    lua_setfield(L, -2, "version");

    lua_pushinteger(L, (lua_Integer)kfs_count());
    lua_setfield(L, -2, "files");

    lua_pushboolean(L, kfs_persist_available());
    lua_setfield(L, -2, "available");

    return 1;
}

/* ---- Registration ---- */

int kformatlua_register(lua_State* L) {
    if (!L) {
        return 0;
    }

    lua_createtable(L, 0, 7);

    lua_pushcfunction(L, kformatlua_encode);
    lua_setfield(L, -2, "encode");

    lua_pushcfunction(L, kformatlua_decode);
    lua_setfield(L, -2, "decode");

    lua_pushcfunction(L, kformatlua_is_valid);
    lua_setfield(L, -2, "is_valid");

    lua_pushcfunction(L, kformatlua_disk_init);
    lua_setfield(L, -2, "disk_init");

    lua_pushcfunction(L, kformatlua_disk_info);
    lua_setfield(L, -2, "disk_info");

    /* Constants */
    lua_pushstring(L, "PLF:1\n");
    lua_setfield(L, -2, "HEADER");

    lua_pushinteger(L, 1);
    lua_setfield(L, -2, "VERSION");

    lua_setglobal(L, "format");

    return 1;
}
