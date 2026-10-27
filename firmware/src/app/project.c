/* SPDX-License-Identifier: GPL-3.0-only */
/* DUB-1 project: one flash object (OBJ_PROJ). A project of another format, or none, gives the
 * defaults. Part of the unity build, after app.h.
 *
 * The pieces (their recipes are the engine's, en_piece): each keeps its own settings as you leave it;
 * Tune and MIDI out are global. */
project_t proj;

const char *piece_name(int i) { return en_piece_name(i); }

static int global_par(int p) { return p == P_TUNE || p == P_MIDI; }

/* (1.3.0) a piece never changed takes today's recipe on an upgrade: one that matches one of its own recipes as they
 * have been (recipe_prints.h, from git: a hash over their values, Tune and MIDI out left out), on the scale its mode
 * gives, was left as it came, so it gets today's sound; one you changed keeps your settings */
#include "recipe_prints.h"
static uint32_t print_of(const int16_t *par, int n)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < n; i++) {
        uint16_t u = (uint16_t)par[i];
        if (global_par(i))
            continue;
        h = (h ^ (u & 0xFFu)) * 16777619u;
        h = (h ^ (u >> 8)) * 16777619u;
    }
    return h;
}
static int untouched_raw(const int16_t *par, int n, uint16_t pool, int piece)   /* as it was saved, n of them */
{
    int k;
    if ((unsigned)par[P_MODE] >= EN_NMODES || pool != EN_MODE_POOL[par[P_MODE]])
        return 0;                                /* (its scale changed by hand) */
    for (k = 0; k < RECIPE_PRINTS; k++)
        if (RECIPE_PRINT[k].n == n && RECIPE_PRINT[k].h[piece] == print_of(par, n))
            return 1;
    return 0;
}
static int untouched_now(const slot_t *sl, int piece)   /* as read: an old recipe's first ones, today's after them */
{
    int16_t rec[P_NPARAMS];
    uint16_t pool;
    int k, p;
    en_piece(piece, rec, &pool);
    if (sl->pool != pool)
        return 0;
    for (k = 0; k < RECIPE_PRINTS; k++) {
        const int nk = RECIPE_PRINT[k].n;
        if (nk > P_NPARAMS || RECIPE_PRINT[k].h[piece] != print_of(sl->par, nk))
            continue;
        for (p = nk; p < P_NPARAMS && (global_par(p) || sl->par[p] == rec[p]); p++)
            ;
        if (p == P_NPARAMS)
            return 1;
    }
    return 0;
}

static void recipe(int i, slot_t *s)
{
    en_piece(i, s->par, &s->pool);
    s->pings_ok = 0;
    s->screen = 0;
}

/* the projects before 1.3.0's noise (formats 8 to 10) had the parameters up to P_NCOL */
#define NPAR10 P_NCOL
_Static_assert(NPAR10 == 106, "formats 8-10 had 106 parameters");
/* DUB-1 1.0.0's project (format 8): the same, without the pings; read, and kept */
typedef struct {
    int16_t par[NPAR10];
    uint16_t pool;
} slot8_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR10];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot8_t slot[NPIECES];
    uint8_t rsv[16];
} project8_t;
/* DUB-1 1.1.0 to 1.3.0's chords (formats 9 and 10): today's, without the noise's five */
typedef struct {
    int16_t par[NPAR10];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot10_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR10];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot10_t slot[NPIECES];
    uint8_t rsv[16];
} project10_t;
/* DUB-1 1.3.0's noise and ladder (formats 11 and 12): today's, without Throws, Ghost, Notes and Hats level */
#define NPAR12 P_CTHROW
_Static_assert(NPAR12 == 111, "formats 11-12 had 111 parameters");
typedef struct {
    int16_t par[NPAR12];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot12_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR12];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot12_t slot[NPIECES];
    uint8_t rsv[16];
} project12_t;
/* DUB-1 1.3.0's pages (format 13): today's, without the chord's Ghost, the kick's density, the echo's Blur, Tone and
 * Width, the noise's Decay */
#define NPAR13 P_CGHOST
_Static_assert(NPAR13 == 115, "format 13 had 115 parameters");
typedef struct {
    int16_t par[NPAR13];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot13_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR13];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot13_t slot[NPIECES];
    uint8_t rsv[16];
} project13_t;
/* DUB-1 1.3.0's echo and density (format 14): today's, without the Noise page's second and the hats' length */
#define NPAR14 P_NWIDTH
_Static_assert(NPAR14 == 121, "format 14 had 121 parameters");
typedef struct {
    int16_t par[NPAR14];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot14_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR14];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot14_t slot[NPIECES];
    uint8_t rsv[16];
} project14_t;
/* DUB-1 1.3.0's Noise space (format 15): today's, without the noise's LFO */
#define NPAR15 P_NLFO
_Static_assert(NPAR15 == 125, "format 15 had 125 parameters");
typedef struct {
    int16_t par[NPAR15];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot15_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR15];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot15_t slot[NPIECES];
    uint8_t rsv[16];
} project15_t;
/* DUB-1 1.3.0's noise LFO (format 16): today's, without the kick's rumble */
#define NPAR16 P_RUMBLE
_Static_assert(NPAR16 == 126, "format 16 had 126 parameters");
typedef struct {
    int16_t par[NPAR16];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot16_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR16];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot16_t slot[NPIECES];
    uint8_t rsv[16];
} project16_t;
_Static_assert(sizeof(project_t) <= 3840u, "the project must fit one storage sector's payload");

static void take(const slot_t *s)                /* a piece's settings into play, the globals kept */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        if (!global_par(p))
            proj.par[p] = s->par[p];
    proj.pool = s->pool;
    proj.screen = (uint8_t)(s->screen % 3u);      /* (each piece its own view) */
}

static void keep(void)                           /* the piece playing, into its slot (with its ping loops) */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        proj.slot[proj.piece].par[p] = proj.par[p];
    proj.slot[proj.piece].pool = proj.pool;
    proj.slot[proj.piece].screen = proj.screen;
    en_pings_get(proj.slot[proj.piece].pings);
    proj.slot[proj.piece].pings_ok = 1;
}

void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    for (i = 0; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = en_param_info(i)->def;
    take(&proj.slot[0]);
    proj.leds = 1;
}

static int pool_ok(uint32_t p)                   /* the root and at least two more notes */
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return (p & 1u) && !(p >> 12) && n >= 3;
}

static int pars_ok(const int16_t *par)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        if (par[i] < p->lo || par[i] > p->hi)
            return 0;
    }
    return 1;
}

static void pars_clamp(int16_t *par)            /* a value out of its (now narrower) range: to its end, not the whole
                                                  * project thrown away (1.3.0's one-sided knobs) */
{
    int i;
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        if (par[i] < p->lo)
            par[i] = p->lo;
        if (par[i] > p->hi)
            par[i] = p->hi;
    }
}
static int project_valid(void)                  /* an older format: the recipes again */
{
    int i;
    if (proj.magic != PROJ_MAGIC || !pool_ok(proj.pool) || !pars_ok(proj.par))
        return 0;
    if (proj.format != PROJ_FORMAT || proj.piece >= NPIECES)
        return 0;
    for (i = 0; i < NPIECES; i++)
        if (!pool_ok(proj.slot[i].pool) || !pars_ok(proj.slot[i].par))
            return 0;
    return 1;
}

static void from8(const project8_t *o)          /* 1.0.0's project: everything kept, the pings grown afresh */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
    }
}
static void from10(const project10_t *o)        /* 1.1.0 to 1.3.0's chords: everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from12(const project12_t *o)        /* 1.3.0's noise and ladder (11, 12): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from13(const project13_t *o)        /* 1.3.0's pages (13): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from14(const project14_t *o)        /* 1.3.0's echo and density (14): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from15(const project15_t *o)        /* 1.3.0's Noise space (15): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from16(const project16_t *o)        /* 1.3.0's noise LFO (16): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void groove11(int16_t *par)              /* before 12: the groove a pattern (0..4: Drone, Offbeat, Dotted,
                                                 * Roller, Sync), now the knob's (Drone, Offbeat, Dotted, Sync, Roller) */
{
    static const int8_t TO[5] = {0, 1, 2, 4, 3};
    par[P_GROOVE] = TO[(unsigned)par[P_GROOVE] < 5u ? par[P_GROOVE] : 1];
    par[P_BNOTES] = 100;
}
static void groove12(int16_t *par)              /* 12: the groove a ladder (0 Drone .. 8 Roller, each step a pattern
                                                 * and how many of its notes), now a pattern and Notes */
{
    static const int8_t PAT[9] = {0, 1, 1, 2, 2, 3, 3, 4, 4};   /* (Drone, Thin off, Offbeat, Thin dot, Dotted, Thin
                                                                 * sync, Sync, Thin roll, Roller: the knob's patterns) */
    static const int8_t DENS[9] = {100, 55, 100, 60, 100, 65, 100, 60, 100};
    unsigned g = (unsigned)par[P_GROOVE] < 9u ? (unsigned)par[P_GROOVE] : 2u;
    par[P_GROOVE] = PAT[g];
    par[P_BNOTES] = DENS[g];
}
static void newpar(int from)                     /* the parameters a format lacked (from on), from the recipes */
{
    int16_t rec[P_NPARAMS];
    uint16_t pool;
    int i, p;
    for (i = 0; i < NPIECES; i++) {
        en_piece(i, rec, &pool);
        for (p = from; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = rec[p];
    }
    for (p = from; p < P_NPARAMS; p++)
        proj.par[p] = proj.slot[proj.piece < NPIECES ? proj.piece : 0].par[p];
}
static void noise10(void)                       /* before 11: each piece's noise as its recipe has it */
{
    int16_t rec[P_NPARAMS];
    uint16_t pool;
    int i, p;
    for (i = 0; i < NPIECES; i++) {
        en_piece(i, rec, &pool);
        for (p = P_NCOL; p < NPAR12; p++)
            proj.slot[i].par[p] = rec[p];
    }
    for (p = P_NCOL; p < NPAR12; p++)
        proj.par[p] = proj.slot[proj.piece < NPIECES ? proj.piece : 0].par[p];
}

static void gaps9(int16_t *par)                  /* before 10: an empty slot is a gap (its root, left as it was, is not
                                                  * a degree of the walk's) */
{
    int i;
    for (i = 0; i < EN_NPROG; i++)
        if (!par[P_CQ0 + i])
            par[P_CR0 + i] = 0, par[P_CX0 + i] = 0;
    par[P_PMODE] = par[P_PMODE] ? EN_PLOOP | EN_WALK_WANDER : EN_WALK_WANDER;   /* Prog: Loop; Random: the walk, now Wander */
}

int project_load(void)
{
    static project_t buf;
    int n, old = 0, i;
    uint8_t fresh[NPIECES];                      /* (a piece never changed: today's recipe) */
    memset(fresh, 0, sizeof fresh);
    memset(&buf, 0, sizeof buf);
    n = plat_store_load(OBJ_PROJ, &buf, sizeof buf);
    if (n >= 8 && buf.magic == PROJ_MAGIC && buf.format == 8u && n >= (int)sizeof(project8_t)) {
        for (i = 0; i < NPIECES; i++) {
            const project8_t *o = (const project8_t *)(const void *)&buf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR10, o->slot[i].pool, i);
        }
        from8((const project8_t *)(const void *)&buf);
        old = 9;
    } else if (n >= 8 && buf.magic == PROJ_MAGIC && (buf.format == 9u || buf.format == 10u) &&
               n >= (int)sizeof(project10_t)) {
        old = (int)buf.format;
        for (i = 0; i < NPIECES; i++) {
            const project10_t *o = (const project10_t *)(const void *)&buf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR10, o->slot[i].pool, i);
        }
        from10((const project10_t *)(const void *)&buf);
    } else if (n >= 8 && buf.magic == PROJ_MAGIC && (buf.format == 11u || buf.format == 12u) &&
               n >= (int)sizeof(project12_t)) {
        old = (int)buf.format;
        for (i = 0; i < NPIECES; i++) {
            const project12_t *o = (const project12_t *)(const void *)&buf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR12, o->slot[i].pool, i);
        }
        from12((const project12_t *)(const void *)&buf);
    } else if (n >= 8 && buf.magic == PROJ_MAGIC && buf.format == 16u && n >= (int)sizeof(project16_t)) {
        old = 16;
        for (i = 0; i < NPIECES; i++) {
            const project16_t *o = (const project16_t *)(const void *)&buf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR16, o->slot[i].pool, i);
        }
        from16((const project16_t *)(const void *)&buf);
    } else if (n >= 8 && buf.magic == PROJ_MAGIC && buf.format == 15u && n >= (int)sizeof(project15_t)) {
        old = 15;
        for (i = 0; i < NPIECES; i++) {
            const project15_t *o = (const project15_t *)(const void *)&buf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR15, o->slot[i].pool, i);
        }
        from15((const project15_t *)(const void *)&buf);
    } else if (n >= 8 && buf.magic == PROJ_MAGIC && buf.format == 14u && n >= (int)sizeof(project14_t)) {
        old = 14;
        for (i = 0; i < NPIECES; i++) {
            const project14_t *o = (const project14_t *)(const void *)&buf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR14, o->slot[i].pool, i);
        }
        from14((const project14_t *)(const void *)&buf);
    } else if (n >= 8 && buf.magic == PROJ_MAGIC && buf.format == 13u && n >= (int)sizeof(project13_t)) {
        old = 13;
        for (i = 0; i < NPIECES; i++) {
            const project13_t *o = (const project13_t *)(const void *)&buf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR13, o->slot[i].pool, i);
        }
        from13((const project13_t *)(const void *)&buf);
    } else {
        proj = buf;
    }
    if (old)                                     /* (the new ones, from the recipes) */
        newpar(old == 16 ? NPAR16 : old == 15 ? NPAR15 : old == 14 ? NPAR14 : old == 13 ? NPAR13 : NPAR12);
    if (old == 9) {                              /* (8 and 9: the chords' slots as 10 has them) */
        gaps9(proj.par);
        for (i = 0; i < NPIECES; i++)
            gaps9(proj.slot[i].par);
    }
    if (old && old <= 10)                        /* (before 11: the noise's own, from the recipes) */
        noise10();
    if (old && old <= 11) {                      /* (before 12: the groove a pattern in the engine's order) */
        groove11(proj.par);
        for (i = 0; i < NPIECES; i++)
            groove11(proj.slot[i].par);
    } else if (old == 12) {                      /* (12: the groove a ladder step) */
        groove12(proj.par);
        for (i = 0; i < NPIECES; i++)
            groove12(proj.slot[i].par);
    }
    if (n >= 8 && proj.magic == PROJ_MAGIC && proj.format == PROJ_FORMAT) {   /* (the ranges as they are now) */
        pars_clamp(proj.par);
        for (i = 0; i < NPIECES; i++)
            pars_clamp(proj.slot[i].par);
        for (i = 0; i < NPIECES; i++)            /* (never changed: today's recipe, its view kept) */
            if (fresh[i] || untouched_now(&proj.slot[i], i)) {
                uint8_t sc = proj.slot[i].screen;
                recipe(i, &proj.slot[i]);
                proj.slot[i].screen = sc;
                if (i == proj.piece)
                    take(&proj.slot[i]);
            }
    }
    if (n >= 8 && project_valid())
        return 0;
    project_defaults();
    return -1;
}

int project_save(void)
{
    keep();
    return plat_store_save(OBJ_PROJ, &proj, sizeof proj);
}

void project_piece(int i)
{
    keep();
    proj.piece = (uint8_t)((i % NPIECES + NPIECES) % NPIECES);
    take(&proj.slot[proj.piece]);
}

void project_piece_reset(void)
{
    slot_t s;
    recipe(proj.piece, &s);
    s.screen = proj.screen;                      /* (the view stays as it is) */
    take(&s);
    proj.slot[proj.piece].pings_ok = 0;          /* (its loops grown afresh too) */
}

void project_apply(void)                         /* (then en_regrow keeps the piece's loops, if it has them) */
{
    int i;
    const slot_t *s = &proj.slot[proj.piece];
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, proj.par[i]);
    en_pool(proj.pool);
    en_pings(s->pings_ok ? s->pings : 0);
}
