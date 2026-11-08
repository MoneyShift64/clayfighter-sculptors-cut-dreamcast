/*
 * VS.C (0x8004E7B0-0x80050380): the VS screen (see vs.h).
 *
 * VS_Draw is the screen's logic (Game_Render calls it every tick); VS_Frame builds the
 * picture once a frame (VS_Run's loop).
 */
#include "common.h"
#include "mpfplat.h"
#include "vs.h"

static void SetTexFilter(u32 mode) {       /* gDPSetTextureFilter: 0 = point, 0x2000 = bilinear */
    u32 *g = gDL;
    gDL = g + 2;
    g[0] = 0xBA000C02;
    g[1] = mode;
}

/* the screen fade has finished (coming in or going out) and stands at level */
static s32 FadeAt(f64 level) {
    ScreenFade *f = &gScreenFade;
    s32 done = (0.0 <= (f64)f->speed) ? (1.0 <= (f64)f->level) : ((f64)f->level <= 0.0);
    if (!done && f->active != 0) return 0;
    return (f64)f->level == level;
}

static void FadeToWhite(void) {
    func_8002A990(&gScreenFade, 1);
    gScreenFade.rgb[2] = 255;
    gScreenFade.rgb[1] = 255;
    gScreenFade.rgb[0] = 255;
    gVs->leaving = 1;
}

static void MenuSound(void) {
    func_800797EC(B_800F7E6C, 0, 0, 7, 0x7FFF, 0, 0, 1.3f, 0);
}

static u8 *ImageOf(u8 *file, s32 at) { return file + *(s32 *)(file + at); }

/* 0x8004E7B0 */
void VS_Init(s32 char1, s32 costume1, s32 char2, s32 costume2, s32 *arena, s32 select) {
    VsScreen *vs;
    ArenaPath *p;

    if (gVs != NULL) return;
    gVs = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(VsScreen), 0);
    bzero(gVs, sizeof(VsScreen));
    vs = gVs;
    vs->char1 = char1;
    vs->costume1 = costume1;
    vs->char2 = char2;
    vs->costume2 = costume2;
    vs->arena = *arena;
    vs->arenaOut = arena;
    func_8002EA90(vs);
    vs = gVs;
    vs->portrait1 = LoadCharacterPortrait(char1, costume1);
    vs->image1 = ImageOf(vs->portrait1, 0xC);
    vs->portrait2 = LoadCharacterPortrait(char2, costume2);
    vs->image2 = ImageOf(vs->portrait2, 0xC);
    vs->frameFile = func_80080360(0xF9BB20);
    vs->frameTop = ImageOf(vs->frameFile, 0xC);
    *(s16 *)(vs->frameTop + 0xA) = 0x18;
    *(s16 *)(vs->frameTop + 0xC) = 0x23;
    vs->frameBack = ImageOf(vs->frameFile, 0x14);
    *(s16 *)(vs->frameBack + 0xA) = 0x19;
    *(s16 *)(vs->frameBack + 0xC) = 0x32;
    vs->font2File = func_80080360(0xF56270);
    vs->font2 = ImageOf(vs->font2File, 0xC);
    vs->font1File = func_80080360(0xF4D8B0);
    vs->font1 = ImageOf(vs->font1File, 0xC);
    vs->slide = 80.0f;
    func_8002A340(&gScreenFade, gScreenFade.type);
    vs = gVs;
    vs->result = -1;
    if (select) {
        vs->selecting = 1;
        vs->angle = 0x9555;
        vs->at[0] = 0.0f;
        vs->at[1] = 0.0f;
        vs->at[2] = 300.0f;
        vs->eye[0] = (f32)((f64)vsSin(vs->angle) * 1500.0);
        vs->eye[1] = 200.0f;
        vs->eye[2] = (f32)((f64)vsCos(vs->angle) * 1500.0);
        vs->eye[0] = (f32)((f64)vs->eye[0] + 0.0);
        vs->eye[2] = (f32)((f64)vs->eye[2] + 300.0);
        vs->height = 200.0f;
        vs->radius = 1500.0f;
    } else {
        p = &D_800AD4F8[vs->arena];
        vs->path = p;
        vs->n = p->n - 1;
        vs->pts = p->pts + 3;
        vs->t = 0.0f;
        vs->at[0] = p->pts[0];
        vs->at[1] = p->pts[1];
        vs->at[2] = -p->pts[2];
        vs->at[1] = (f32)((f64)vs->at[1] - 100.0);
        vs->atFrom[0] = vs->at[0]; vs->atFrom[1] = vs->at[1]; vs->atFrom[2] = vs->at[2];
        vs->atTo[0] = vs->at[0]; vs->atTo[1] = vs->at[1]; vs->atTo[2] = vs->at[2];
        vs->eye[0] = p->pts[3];
        vs->eye[1] = p->pts[4];
        vs->eye[2] = -p->pts[5];
        vs->eye[1] = (f32)((f64)vs->eye[1] - 100.0);
    }
    func_8007C5BC(10000, 0x100, 0xDAC);
}

/* 0x8004EBF0 */
void VS_Shutdown(void) {
    if (gVs == NULL) return;
    func_8002ECBC(gVs);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gVs->portrait1);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gVs->portrait2);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gVs->frameFile);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gVs->font2File);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gVs->font1File);
    if (gVs->ownPoints != 0) mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gVs->pts);
    func_8007C5BC(0x2000, 0x100, 0xDAC);
    func_8002AC24(&gScreenFade);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gVs);
    gVs = NULL;
}

/* A or Start in "Select Arena": the camera flies from where it is into the arena. If it is
 * looking roughly along the arena's own path (within 0.2 of a direction), it joins the path
 * at that point; otherwise it takes a straight four-point path to the path's end. */
static void ChooseArena(void) {
    VsScreen *vs = gVs;
    ArenaPath *p = &D_800AD4F8[vs->arena];
    f32 *pts = p->pts + 3, cam[3], d[3], e[3], best = -1.0f, dot, *q, *buf;
    s32 n = p->n - 1, bestK = -1, k, cnt;

    cam[0] = vs->eye[0];
    cam[1] = vs->eye[1];
    cam[2] = -vs->eye[2];
    cam[1] = (f32)((f64)cam[1] + 100.0);
    for (k = 0; k < n - 1; k++) {
        q = &pts[k * 3];
        d[0] = q[0] - cam[0]; d[1] = q[1] - cam[1]; d[2] = q[2] - cam[2];
        func_80087DD0(d);
        e[0] = q[3] - q[0]; e[1] = q[4] - q[1]; e[2] = q[5] - q[2];
        func_80087DD0(e);
        dot = d[0] * e[0] + d[1] * e[1] + d[2] * e[2];
        if (best < dot) {
            bestK = k;
            best = dot;
        }
        if (0.2 < (f64)best) break;
    }
    if (bestK < 0 || (f64)best < 0.2) {
        q = &pts[(n - 1) * 3];
        d[0] = q[0] - cam[0]; d[1] = q[1] - cam[1]; d[2] = q[2] - cam[2];
        B_800E1230[0] = cam[0]; B_800E1230[1] = cam[1]; B_800E1230[2] = cam[2];
        for (k = 0; k < 3; k++) B_800E1230[3 + k] = (f32)((f64)cam[k] + (f64)d[k] * 0.33);
        for (k = 0; k < 3; k++) B_800E1230[6 + k] = (f32)((f64)cam[k] + (f64)d[k] * 0.66);
        B_800E1230[9] = q[0]; B_800E1230[10] = q[1]; B_800E1230[11] = q[2];
        vs->path = p;
        vs->n = 4;
        vs->pts = B_800E1230;
        vs->ownPoints = 0;
    } else {
        cnt = n - bestK + 1;
        buf = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, cnt * 12, 0);
        for (k = 0; k < cnt; k++) {
            if (k == 0) {
                buf[0] = cam[0]; buf[1] = cam[1]; buf[2] = cam[2];
            } else {
                q = &pts[(bestK + k - 1) * 3];
                buf[k * 3 + 0] = q[0]; buf[k * 3 + 1] = q[1]; buf[k * 3 + 2] = q[2];
            }
        }
        vs->path = &D_800AD4F8[vs->arena];
        vs->n = cnt;
        vs->pts = buf;
        vs->ownPoints = 1;
    }
    mpDebugPrintf("\nownPoints = %d\n", gVs->ownPoints);
    for (k = 0; k < gVs->n; k++)
        mpDebugPrintf("(%f,%f,%f)\n", (f64)gVs->pts[k * 3], (f64)gVs->pts[k * 3 + 1], (f64)gVs->pts[k * 3 + 2]);
    vs = gVs;
    vs->t = 0.0f;
    p = &D_800AD4F8[vs->arena];
    vs->atTo[0] = p->pts[0];
    vs->atTo[1] = p->pts[1];
    vs->atTo[2] = -p->pts[2];
    vs->atTo[1] = (f32)((f64)vs->atTo[1] - 100.0);
    vs->atFrom[0] = vs->at[0]; vs->atFrom[1] = vs->at[1]; vs->atFrom[2] = vs->at[2];
    vs->selecting = 0;
    CharMenuSound_Load(D_800AB220[vs->arena]);
}

/* the circling camera: the stick zooms (radius 800..2600) and raises it (height 100..2600,
 * the two together kept under 4000), C-left/right turn it, left/right change the arena */
static void SelectArena(void) {
    VsScreen *vs = gVs;
    f64 lim;

    vs->angle += 0x12;
    vs->eye[0] = vsSin(vs->angle) * vs->radius;
    vs->eye[1] = vs->height;
    vs->eye[2] = vsCos(vs->angle) * vs->radius;
    vs->eye[0] = (f32)((f64)vs->eye[0] + 0.0);
    vs->eye[2] = (f32)((f64)vs->eye[2] + 300.0);
    if (B_800F77D8 & 8) {
        vs->radius = (f32)((f64)vs->radius - 10.0);
        if ((f64)vs->radius < 800.0) vs->radius = 800.0f;
    }
    if (B_800F77D8 & 4) {
        vs->radius = (f32)((f64)vs->radius + 10.0);
        if (2600.0 < (f64)vs->radius) vs->radius = 2600.0f;
        if (1400.0 < (f64)vs->radius) {
            lim = 2600.0 - (f64)vs->radius + 1400.0;
            if (lim < (f64)vs->height) vs->height = (f32)lim;
        }
    }
    if (B_800F77D8 & 1) {
        vs->height = (f32)((f64)vs->height - 10.0);
        if ((f64)vs->height < 100.0) vs->height = 100.0f;
    }
    if (B_800F77D8 & 2) {
        vs->height = (f32)((f64)vs->height + 10.0);
        if (2600.0 < (f64)vs->height) vs->height = 2600.0f;
        if (1400.0 < (f64)vs->height) {
            lim = 2600.0 - (f64)vs->height + 1400.0;
            if (lim < (f64)vs->radius) vs->radius = (f32)lim;
        }
    }
    if (B_800F8C78 & 0xA00) {
        MenuSound();
        if (--gVs->arena < 0) gVs->arena = 25;
    }
    if (B_800F8C78 & 0x500) {
        MenuSound();
        if (!(++gVs->arena < 26)) gVs->arena = 0;
    }
    if (B_800F77D8 & 0x2020) gVs->angle -= 0x5B;
    if (B_800F77D8 & 0x10) gVs->angle += 0x5B;
    if (B_800F8C78 & 0x9000) ChooseArena();
}

/* the camera flying along the path, the portraits sliding apart; A or Start (or reaching
 * the end of the path) fades to white */
static void FlyIn(void) {
    VsScreen *vs;
    f32 s;

    if (FadeAt(1.0)) {
        if (!(B_800F8C78 & 0x9000)) goto fly;
        FadeToWhite();
    }
    if (B_800F8C78 & 0x9000) gVs->skip = 1;
fly:
    vs = gVs;
    Spline_Point(vs->pts, vs->n, vs->t, vs->eye, 0);
    vs = gVs;
    vs->eye[2] = -vs->eye[2];
    vs->eye[1] = (f32)((f64)vs->eye[1] - 100.0);
    vs->t = Spline_Advance(vs->pts, vs->n, vs->t, 10.0f, 0);
    if (1.0 <= (f64)Spline_Advance(gVs->pts, gVs->n, gVs->t, 238.0f, 0) && FadeAt(1.0)) FadeToWhite();
    vs = gVs;
    s = (0.3 < (f64)vs->t) ? 1.0f : (f32)((f64)vs->t / 0.3);
    vs->at[0] = (vs->atTo[0] - vs->atFrom[0]) * s + vs->atFrom[0];
    vs->at[1] = (vs->atTo[1] - vs->atFrom[1]) * s + vs->atFrom[1];
    vs->at[2] = (vs->atTo[2] - vs->atFrom[2]) * s + vs->atFrom[2];
    if (80.0 < (f64)vs->slide) {
        vs->slide = (f32)((f64)vs->slide - 5.0);
        if ((f64)vs->slide < 100.0) vs->slide = 100.0f;
    }
}

/* 0x8004ECF0: the logic, once a tick */
s32 VS_Draw(void) {
    if (gVs == NULL) return 0;
    func_8002ED34(gVs);
    if (gVs->selecting != 0) SelectArena();
    else FlyIn();
    func_8002AA14(&gScreenFade, 0.03f);
    if (FadeAt(1.0)) func_8002AC24(&gScreenFade);
    if (FadeAt(0.0) && gVs->leaving != 0) {
        gVs->result = 0;
        *gVs->arenaOut = gVs->arena;
        gVs->leaving = 0;
    }
    return gVs->result;
}

static void SetTextColours(u8 hi, u8 lo) {
    B_800F7D94 = 255; B_800F7778 = 255;
    B_800F7789 = hi; B_800F7770 = hi;
    B_800F7E78 = 0; B_800F4E34 = 0;
    B_800F7788 = hi; B_800F7E70 = hi;
    B_800F88BC = lo; B_800F88C8 = lo;
    B_800F7ECC = lo; B_800F2D1C = lo;
}

static void ArenaName(const char *s, f32 *up, f32 *right) {
    SetTexFilter(0);
    func_80082518(gVs->font2, &gDL, s, (f32)(-(func_80082298(gVs->font2, s) / 2)), -100.0f, 0, up, right, 8);
    SetTexFilter(0x2000);
}

/* one portrait in its frame */
static void Portrait(f32 x, u8 *image, s32 flags, f32 *up, f32 *right) {
    func_80084CB0(&gDL, gVs->frameBack, x, 50.0f, 0, up, right, flags);
    SetTexFilter(0);
    func_80083A64(&gDL, image, x, 50.0f, 0, up, right, flags);
    SetTexFilter(0x2000);
    func_80084CB0(&gDL, gVs->frameTop, x, 50.0f, 0, up, right, flags);
}

/* 0x8004FB48: the picture, once a frame */
s32 VS_Frame(void) {
    f32 cam[16], up[3], right[3];
    char buf[0x100], buf2[0x100];
    u16 *spacing;

    if (gVs == NULL) return 0;
    func_8007E860(&gDL);
    func_8007E9DC(&gDL);
    func_8007EEF0(&gDL);
    func_80088654(cam, gVs->eye, gVs->at);
    up[0] = 0.0f; up[1] = 1.0f; up[2] = 0.0f;
    right[0] = -1.0f; right[1] = 0.0f; right[2] = 0.0f;
    func_8002FD50(gVs, cam);
    func_8002FE34(gVs, cam);
    if (gVs->selecting != 0) {
        func_8007F94C(&gDL);
        func_80082210(gVs->font1, &gDL, 2);
        spacing = (u16 *)(gVs->font1 + 4);             /* letters 2 pixels closer */
        *spacing -= 2;
        SetTexFilter(0);
        func_800824CC(gVs->font1, &gDL, "Select Arena", (f32)(-(func_80082298(gVs->font1, "Select Arena") / 2)),
                      65.0f, 0.0f, up, right, 8);
        SetTexFilter(0x2000);
        spacing = (u16 *)(gVs->font1 + 4);
        *spacing += 2;
        SetTextColours(255, 0);
        mpSPrintf(buf, "%s", func_8000A6E4(gVs->arena));
        ArenaName(buf, up, right);
    } else {
        func_8007F94C(&gDL);
        SetTextColours(0xBE, 0);
        SetTexFilter(0x2000);
        Portrait(-gVs->slide + -4.0f, gVs->image1, 9, up, right);
        SetTexFilter(0x2000);
        Portrait(gVs->slide + -4.0f, gVs->image2, 8, up, right);
        SetTexFilter(0);
        func_80082210(gVs->font1, &gDL, 1);
        func_800824CC(gVs->font1, &gDL, "VS", (f32)(-5 - func_80082298(gVs->font1, "VS") / 2), 65.0f, 0.0f, up,
                      right, 8);
        SetTexFilter(0x2000);
        SetTextColours(255, 0);
        mpSPrintf(buf2, "%s %s", D_800AD5C8[gVs->arena], func_8000A6E4(gVs->arena));
        ArenaName(buf2, up, right);
    }
    func_8002AC80(&gScreenFade, &gDL);
    return gVs->result;
}
