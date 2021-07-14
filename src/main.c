#include <lua.h>
#include <lauxlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <math.h>
#include <stdlib.h>

#if LUA_VERSION_NUM < 502
#define luaL_newlib(L, l) (lua_newtable(L), luaL_register(L, NULL, l))
#endif

static bool initialized = false;

static int g_datacenter_id = 0;
static int g_node_id = 0;
static long g_last_timestamp = -1;
static int g_sequence = 0;

// 2014-10-20T15:00:00.000Z
#define SNOWFLAKE_EPOC 1413817200000L

#define NODE_ID_BITS 5
#define DATACENTER_ID_BITS 5
#define SEQUENCE_BITS 12
#define DELTA_OFFSET 1

struct conf {
    long snowflake_epoc;
    int node_id_bits;
    int datacenter_id_bits;
    int sequence_bits;
    int node_id_shift;
    int datacenter_id_shift;
    int timestamp_shift;
    int sequence_mask;
    int delta_offset;
} conf;

static long get_timestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    switch (conf.delta_offset)
    {
    case 1000:
        return tv.tv_sec;
    case 100:
        return tv.tv_sec * 10 + tv.tv_usec / 100000;
    case 10:
        return tv.tv_sec * 100 + tv.tv_usec / 10000;
    case 1:
        return tv.tv_sec * 1000 + tv.tv_usec / 1000;
    }
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static long get_til_next_millis(long last_timestamp) {
    long ts = get_timestamp();
    while (ts < last_timestamp) {
        ts = get_timestamp();
    }

    return ts;
}

static int luasnowflake_init(lua_State *L) {
    conf.snowflake_epoc = luaL_optlong(L, 3, SNOWFLAKE_EPOC);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (conf.snowflake_epoc < 0 || conf.snowflake_epoc > tv.tv_sec * 1000) {
        char buffer[32];
        snprintf(buffer, 32, "%ld", tv.tv_sec * 1000);
        return luaL_error(L, "snowflake_epoc must be an long n where 1 ≤ n ≤ %s", buffer);
    }

    conf.node_id_bits = luaL_optint(L, 4, NODE_ID_BITS);
    if (conf.node_id_bits < 1) {
        return luaL_error(L, "node_id_bits must be an integer n where ≥ 1");
    }

    conf.datacenter_id_bits = luaL_optint(L, 5, DATACENTER_ID_BITS);
    if (conf.datacenter_id_bits < 1) {
        return luaL_error(L, "datacenter_id_bits must be an integer n where ≥ 1");
    }

    conf.sequence_bits = luaL_optint(L, 6, SEQUENCE_BITS);
    if (conf.sequence_bits < 1) {
        return luaL_error(L, "sequence_bits must be an integer n where ≥ 1");
    }

    conf.delta_offset = luaL_optint(L, 7, DELTA_OFFSET);
    switch (conf.delta_offset)
    {
    case 1000:
    case 100:
    case 10:
    case 1:
        break;
    default:
        return luaL_error(L, "does not support delta_offset: %d", conf.delta_offset);
    }

    conf.snowflake_epoc = conf.snowflake_epoc / conf.delta_offset;

    g_datacenter_id = luaL_checkint(L, 1);
    int max_datacenter_id_bits = (1 << conf.datacenter_id_bits) - 1;
    if (g_datacenter_id < 0x00 || g_datacenter_id > max_datacenter_id_bits) {
        return luaL_error(L, "datacenter_id must be an integer n, where 0 ≤ n ≤ %d", max_datacenter_id_bits);
    }

    g_node_id = luaL_checkint(L, 2);
    int max_node_id_bits = (1 << conf.node_id_bits) -1;
    if (g_node_id < 0x00 || g_node_id > max_node_id_bits) {
        return luaL_error(L, "node_id must be an integer n where 0 ≤ n ≤ %d", max_node_id_bits) ;
    }

    conf.node_id_shift = conf.sequence_bits;
    conf.datacenter_id_shift = (conf.node_id_shift + conf.node_id_bits);
    conf.timestamp_shift = (conf.datacenter_id_shift + conf.datacenter_id_bits);
    conf.sequence_mask = (0xffffffff ^ (0xffffffff << conf.sequence_bits));

    initialized = true;

    return 1;
}

static int luasnowflake_next_id(lua_State *L) {
    if (!initialized) {
        return luaL_error(L, "snowflake.init must be called first");
    }

    long ts = get_timestamp();

    if (g_last_timestamp == ts) {
        g_sequence = (g_sequence + 1) & conf.sequence_mask;
        if (g_sequence == 0) {
            ts = get_til_next_millis(g_last_timestamp);
        }
    } else {
        g_sequence = 0;
    }

    g_last_timestamp = ts;
    ts = ((ts - conf.snowflake_epoc) << conf.timestamp_shift)
            | (g_datacenter_id << conf.datacenter_id_shift)
            | (g_node_id << conf.node_id_shift)
            | g_sequence;

    char buffer[32];
    snprintf(buffer, 32, "%ld", ts);
    lua_pushstring(L, buffer);

    return 1;
}

static int bit_split(lua_State *L) {
    const char* snow_id_str = luaL_checkstring(L, 1);
    long int snow_id = atol(snow_id_str);
    long sequence = snow_id & ((long)1 << conf.sequence_bits) - 1;
    long data_machine_and = ((long)1 << (conf.node_id_bits + conf.datacenter_id_bits)) - 1;
    long data_machine_id = (snow_id >> conf.sequence_bits) & data_machine_and;
    int max = conf.sequence_bits + conf.node_id_bits + conf.datacenter_id_bits;
    long delta_offset_and = ((long)1 << (64 - max)) - 1;
    long delta_offset = (snow_id >> max) & delta_offset_and;
    char buffer[32];
    snprintf(buffer, 32, "%ld, %ld, %ld", data_machine_id, delta_offset, sequence);
    lua_pushstring(L, buffer);
    return 1;
}

static int getmillisecond(lua_State *L) {
    lua_pushnumber(L, get_timestamp());
    return 1;
}

static const struct luaL_Reg luasnowflake_lib[] = {
    {"init", luasnowflake_init},
    {"next_id", luasnowflake_next_id},
    {"getmillisecond", getmillisecond},
    {"bit_split", bit_split},
    {NULL, NULL},
};

LUALIB_API int luaopen_snowflake(lua_State *const L) {
    luaL_newlib(L, luasnowflake_lib);

    return 1;
}
