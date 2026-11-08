/*
 * audio.c - the game's sound for the port: N64AUDIO.C's functions (same names, same rules for
 * the sound effect slots, so the game behaves as it did), on a small player of the port's own
 * in place of libultra's sequence player, sound player and synthesizer.
 *
 * The sounds come from SOUND.DAT (tools/port_sound.py makes it from the ROM): each of the
 * game's bank files, keyed by the ROM address of its sample data, in this little-endian form:
 *   file:       "PBNK", banks, waves, wave table offset, bank offsets[banks]
 *   bank:       instruments, sample rate, percussion instrument (offset), instrument offsets[]
 *   instrument: volume, pan, priority, flags, bend range (cents), sounds, sound offsets[]
 *   sound:      envelope (attack, decay, release times in microseconds; attack and decay
 *               volumes), pan, volume, key map (velocity min/max, key min/max, key base,
 *               detune), flags, wave
 *   wave:       data offset, bytes, samples, loop start, loop end, loop count, format
 * The platform plays the waves (plat_voice_*: the Dreamcast's AICA channels); this file does
 * everything else: the music (the game's compressed MIDI sequences), the envelopes, volumes,
 * pitches and pans, at plat's rate (audio_tick).
 */
#include "port.h"
#include "n64sys.h"
#include "mpfplat.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ------------------------------------------------------------------ the bank files */

typedef struct PFile { char magic[4]; u32 banks, waves, waveOff, bankOff[1]; } PFile;
typedef struct PBank { s32 instCount, sampleRate; u32 percOff, instOff[1]; } PBank;
typedef struct PInst { u8 vol, pan, priority, flags; s16 bend, soundCount; u32 soundOff[1]; } PInst;
typedef struct PSound {
    s32 attackTime, decayTime, releaseTime;
    u8 attackVol, decayVol, pan, vol;
    u8 velMin, velMax, keyMin, keyMax, keyBase;
    s8 detune;
    u8 flags, pad;
    u32 wave;
} PSound;

#define AT(f, off) ((void *)((u8 *)(f) + (off)))

/* the bank files the game has loaded: its handle (the pointer it passes around), ours */
#define MAX_FILES 8
typedef struct Loaded { const void *handle; u32 key; PFile *file; u32 size; s32 global; u32 used; } Loaded;
static Loaded sFiles[MAX_FILES];
static u32 sUseClock;
static u32 sDirN, *sDir;                   /* SOUND.DAT's table: key, offset, size */

static PFile *FileOf(const void *handle) {
    s32 i;
    for (i = 0; i < MAX_FILES; i++) if (sFiles[i].file && sFiles[i].handle == handle) return sFiles[i].file;
    return NULL;
}
static PBank *BankOf(PFile *f, s32 b) { return (f && b >= 0 && (u32)b < f->banks) ? AT(f, f->bankOff[b]) : NULL; }
static PInst *Inst(PFile *f, s32 bank, s32 inst) {
    PBank *b = BankOf(f, bank);
    if (b == NULL || inst < 0 || inst >= b->instCount || b->instOff[inst] == 0) return NULL;
    return (PInst *)AT(f, b->instOff[inst]);
}
static PSound *Sound(PFile *f, PInst *in, s32 s) {
    if (in == NULL || s < 0 || s >= in->soundCount) return NULL;
    return (PSound *)AT(f, in->soundOff[s]);
}
static const PWave *Wave(PFile *f, const PSound *s) {
    if (s->wave >= f->waves) return NULL;
    return (const PWave *)AT(f, f->waveOff) + s->wave;
}

/* ------------------------------------------------------------------ voices */

enum { ENV_OFF, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };
#define MAX_VOICES 24
#define MUSIC_VOICES 16
typedef struct Voice {
    s32 env;                   /* ENV_* (ENV_OFF: free) */
    f32 level, target, step;   /* envelope gain 0..1 (squared, as the synthesizer's), where it is
                                  going, its exponential rate (per second) */
    f32 remain;                /* seconds left of this envelope stage */
    const PSound *snd;
    const PWave *wave;
    PFile *file;
    f32 hz;                    /* playback rate */
    f32 vol, pan;              /* without the envelope: 0..1, 0..1 */
    f32 played;                /* seconds played (one-shot waves end) */
    f32 envScale;              /* envelope times scaled (a sound effect's: by 1 / its pitch, as libultra) */
    f32 lastHz, lastVol, lastPan;
    s32 started;
    s32 ended;                 /* a music note whose wave ran out: silent, but its voice still held */
    /* music */
    s32 chan, key, vel;
    f32 noteLeft;              /* seconds until the note's end (compressed MIDI notes have a length) */
    /* sound effects */
    s32 sfx;                   /* the sound player's id + 1, 0 for music */
} Voice;
static Voice sVoice[MAX_VOICES];
static f32 sOutputRateF = 22050.0f;     /* (the output rate, for the envelope scale) */

/* the N64's envelope mixer ramps the (squared) volume by a ratio each frame, not by a step: an
   exponential ramp, from and to no less than 1 in 32767 (a decay to nothing falls away fast and
   tails off; linear, the title music's long drum held on, too loud) */
#define ENV_FLOOR (1.0f / 32767.0f)
static void EnvStage(Voice *v, s32 stage, f32 target, s32 usec) {
    v->env = stage;
    v->target = target * target;
    if (usec <= 0) { v->level = v->target; v->step = 0.0f; v->remain = 0.0f; }
    else {
        f32 from = v->level > ENV_FLOOR ? v->level : ENV_FLOOR;
        f32 to = v->target > ENV_FLOOR ? v->target : ENV_FLOOR;
        v->remain = usec * 1e-6f * v->envScale;
        v->step = logf(to / from) / v->remain;
    }
}

static void VoiceStop(Voice *v) {
    if (v->env != ENV_OFF && v->started && !v->ended) plat_voice_stop((s32)(v - sVoice));
    v->env = ENV_OFF;
    v->started = 0;
    v->ended = 0;
}

static void VoiceRelease(Voice *v) {
    if (v->env == ENV_OFF || v->env == ENV_RELEASE) return;
    EnvStage(v, ENV_RELEASE, 0.0f, v->snd->releaseTime > 0 ? v->snd->releaseTime : 1);
}

/* start a sound on a voice: its envelope from the start */
static void VoiceStart(Voice *v, PFile *f, const PSound *s, f32 hz, f32 vol, f32 pan) {
    VoiceStop(v);
    v->file = f;
    v->snd = s;
    v->wave = Wave(f, s);
    v->hz = hz;
    v->vol = vol;
    v->pan = pan;
    v->played = 0.0f;
    v->level = 0.0f;
    v->started = 0;
    v->lastHz = v->lastVol = v->lastPan = -1.0f;
    v->envScale = hz > 0.0f ? (f32)sOutputRateF / hz : 1.0f;
    if (v->wave == NULL) { v->env = ENV_OFF; return; }
    EnvStage(v, ENV_ATTACK, s->attackVol / 127.0f, s->attackTime);
}

/* ------------------------------------------------------------------ the sound player */

/* (libultra's ALSndPlayer as the game uses it: allocate a sound, set it current, play, stop,
   is it playing, pitch, volume, pan) */
#define MAX_SFX 16
typedef struct Sfx { s32 used; PFile *file; const PSound *snd; s32 voice; f32 pitch; f32 vol; f32 pan; s32 playing; } Sfx;
static Sfx sSfx[MAX_SFX];

static s32 SndpAllocate(PFile *f, const PSound *s) {
    s32 i;
    for (i = 0; i < MAX_SFX; i++)
        if (!sSfx[i].used) {
            memset(&sSfx[i], 0, sizeof(Sfx));
            sSfx[i].used = 1; sSfx[i].file = f; sSfx[i].snd = s; sSfx[i].voice = -1;
            sSfx[i].pitch = 1.0f; sSfx[i].vol = 1.0f; sSfx[i].pan = 0.5f;
            return i;
        }
    return -1;
}
static s32 SfxLive(Sfx *x) { return x->voice >= 0 && sVoice[x->voice].env != ENV_OFF && sVoice[x->voice].sfx == (s32)(x - sSfx) + 1; }
static u32 sAudioFrame;                     /* B_800E33B0: once a retrace */
static void SndpPlay(Sfx *x) {
    s32 i, best = -1;
    f32 low = 2.0f;
    for (i = MUSIC_VOICES; i < MAX_VOICES; i++) {          /* a free voice, else the quietest */
        Voice *v = &sVoice[i];
        if (v->env == ENV_OFF) { best = i; break; }
        if (v->level * v->vol * v->vol < low) { low = v->level * v->vol * v->vol; best = i; }
    }
    if (best < 0) return;
    VoiceStart(&sVoice[best], x->file, x->snd, x->pitch, x->vol, x->pan);
    sVoice[best].sfx = (s32)(x - sSfx) + 1;
    x->voice = best;
    x->playing = 1;
}
static void SndpStop(Sfx *x) {
    if (SfxLive(x)) VoiceRelease(&sVoice[x->voice]);
    x->playing = 0;
}
static s32 SndpState(Sfx *x) { return SfxLive(x); }
static void SndpUpdate(Sfx *x) {
    if (SfxLive(x)) { Voice *v = &sVoice[x->voice]; v->hz = x->pitch; v->vol = x->vol; v->pan = x->pan; }
}

/* ------------------------------------------------------------------ the music: compressed MIDI */

/* the sequence: 16 tracks, each a byte stream of events with delta times; notes carry their
   length (no note-offs); 0xFE hi lo len: repeat len bytes from earlier in the track; meta
   events: tempo, end of track, loop start and loop end (count, current count, offset back) */
typedef struct Track {
    u8 *loc;                    /* next byte (NULL: the track has ended) */
    u8 *bu; s32 buLen;          /* a repeat being read */
    u8 status;
    f64 when;                   /* the tick of its next event */
} Track;
typedef struct Chan { const PInst *inst; s32 vol, pan, bend, fx; } Chan;
static struct {
    u8 *data;
    s32 playing;
    Track t[16];
    Chan c[16];
    f64 tick;                   /* now, in ticks */
    f64 tps;                    /* ticks a second */
    u32 division;
    s32 vol;                    /* alCSPSetVol: 0..0x7FFF */
    PFile *file;
    s32 bank;
} sSeq;

static u8 TrackByte(Track *t) {
    u8 b;
    if (t->bu) {
        b = *t->bu++;
        if (--t->buLen == 0) t->bu = NULL;
        return b;
    }
    b = *t->loc++;
    if (b == 0xFE) {
        u8 n = *t->loc++;
        if (n != 0xFE) {
            u32 back = ((u32)n << 8) | *t->loc++;
            s32 len = *t->loc++;
            t->bu = t->loc - (back + 4);
            t->buLen = len;
            b = *t->bu++;
            if (--t->buLen == 0) t->bu = NULL;
        }
    }
    return b;
}
static u32 TrackVarLen(Track *t) {
    u32 v = 0;
    u8 b;
    do { b = TrackByte(t); v = (v << 7) | (b & 0x7F); } while (b & 0x80);
    return v;
}

/* libultra's __lookupSoundQuick: a binary search over the instrument's key and velocity ranges,
   and no note at all when it finds none (searching in order and falling back to the first sound
   played some notes with the wrong sound, out of tune) */
static const PSound *KeySound(const PInst *in, s32 key, s32 vel) {
    s32 l = 1, r = in->soundCount, i;
    while (r >= l) {
        const PSound *s;
        i = (l + r) / 2;
        s = (const PSound *)AT(sSeq.file, in->soundOff[i - 1]);
        if (key >= s->keyMin && key <= s->keyMax && vel >= s->velMin && vel <= s->velMax) return s;
        else if (key < s->keyMin || (vel < s->velMin && key <= s->keyMax)) r = i - 1;
        else l = i + 1;
    }
    return NULL;
}

static f32 Cents(f32 c) { return powf(2.0f, c / 1200.0f); }

static f32 NoteVol(const Voice *v) {
    const Chan *c = &sSeq.c[v->chan];
    const PInst *in = c->inst;
    (void)in;                  /* (libultra's __vsVol: no instrument volume) */
    f32 dry = cosf(c->fx * (1.5707963f / 127.0f));      /* (the synthesizer's dry amount, eqpower[fx]; it
                                                           comes after the square, so its root here) */
    return (v->vel / 127.0f) * (v->snd->vol / 127.0f) * (c->vol / 127.0f) * (sSeq.vol / 32767.0f) * sqrtf(dry);
}
static f32 NotePan(const Voice *v) {
    s32 p = sSeq.c[v->chan].pan + v->snd->pan - 64;    /* (__vsPan: no instrument pan) */
    p = p < 0 ? 0 : p > 127 ? 127 : p;
    return p / 127.0f;
}
static f32 NoteHz(const Voice *v) {
    const Chan *c = &sSeq.c[v->chan];
    f32 cents = (v->key - v->snd->keyBase) * 100.0f + v->snd->detune;
    if (c->inst) cents += (f32)((s32)c->inst->bend * c->bend / 8192);   /* (whole cents, as libultra) */
    return sOutputRateF * Cents(cents);          /* (libultra: a ratio to the output rate; the bank's rate unused) */
}

static void NoteOn(s32 chan, s32 key, s32 vel, u32 len) {
    const PInst *in = sSeq.c[chan].inst;
    const PSound *s;
    Voice *v = NULL;
    s32 i;
    if (in == NULL || sSeq.file == NULL) return;
    s = KeySound(in, key, vel);
    if (s == NULL) return;
    /* a free voice; with all 16 busy (releasing ones too) the note isn't played, as libultra's
       __mapVoice (taking one over cut a note short and played one the N64 doesn't) */
    for (i = 0; i < MUSIC_VOICES; i++)
        if (sVoice[i].env == ENV_OFF) { v = &sVoice[i]; break; }
    if (v == NULL) return;
    v->chan = chan; v->key = key; v->vel = vel; v->sfx = 0;
    v->snd = s;
    VoiceStart(v, sSeq.file, s, 0.0f, 0.0f, 0.5f);
    v->hz = NoteHz(v); v->vol = NoteVol(v); v->pan = NotePan(v);
    v->noteLeft = (f32)(len / sSeq.tps);
}

static void SeqStop(void) {
    s32 i;
    sSeq.playing = 0;
    for (i = 0; i < MUSIC_VOICES; i++) VoiceRelease(&sVoice[i]);
}

/* the header's 17 words (track offsets, division): in this machine's order once the data is
   converted (port_assets.py), else as in the ROM */
static u32 SeqWord(const u8 *data, s32 i) {
    u32 w = ((const u32 *)data)[i];
    return ((const u32 *)data)[0] < 0x10000 ? w : READ_BE32(data + i * 4);
}

/* libultra's __setInstChanState: a channel's instrument brings its volume and pan with it
   (until the sequence sets them; without this the first notes of the title music, before
   their channel's volume came, were played at full volume) */
static void SetInst(s32 chan, const PInst *in) {
    if (in == NULL) return;
    sSeq.c[chan].inst = in;
    sSeq.c[chan].vol = in->vol;
    sSeq.c[chan].pan = in->pan;
}

static void SeqStart(u8 *data) {
    s32 i;
    memset(sSeq.t, 0, sizeof(sSeq.t));
    sSeq.data = data;
    sSeq.division = SeqWord(data, 16);
    if (sSeq.division == 0) sSeq.division = 96;
    sSeq.tps = sSeq.division * 1e6 / 500000.0;
    sSeq.tick = 0.0;
    for (i = 0; i < 16; i++) {
        u32 o = SeqWord(data, i);
        sSeq.c[i].vol = 127; sSeq.c[i].pan = 64; sSeq.c[i].bend = 0; sSeq.c[i].fx = 0;
        sSeq.c[i].inst = Inst(sSeq.file, sSeq.bank, 0);
        SetInst(i, sSeq.c[i].inst);              /* (alCSPSetBank: instrument 0's settings) */
        if (o) { sSeq.t[i].loc = data + o; sSeq.t[i].when = TrackVarLen(&sSeq.t[i]); }
    }
    {   /* libultra's __initFromBank: channel 10 (9 here) plays the bank's percussion instrument
           (the title music's drums played on the bass, too loud) */
        PBank *b = BankOf(sSeq.file, sSeq.bank);
        if (b && b->percOff) SetInst(9, (const PInst *)AT(sSeq.file, b->percOff));
    }
    sSeq.playing = 1;
}

/* one event of track n (its time has come) */
static void TrackEvent(s32 n) {
    Track *t = &sSeq.t[n];
    u8 b = TrackByte(t), st;
    s32 d1 = 0, chan;
    if (b & 0x80) { st = b; if (st < 0xF0) t->status = st; }
    else { st = t->status; d1 = b; }
    if (st == 0xFF) {                                      /* meta */
        u8 type = TrackByte(t);
        if (type == 0x51) {
            u32 us = (u32)TrackByte(t) << 16; us |= (u32)TrackByte(t) << 8; us |= TrackByte(t);
            if (us) sSeq.tps = sSeq.division * 1e6 / us;
        } else if (type == 0x2F) {
            t->loc = NULL;
            return;
        } else if (type == 0x2E) {                         /* loop start: number, 0xFF */
            TrackByte(t); TrackByte(t);
        } else if (type == 0x2D) {                         /* loop end */
            u8 *cnt;
            u32 back;
            TrackByte(t);                                  /* the loop count */
            cnt = t->bu ? t->bu : t->loc;                  /* (the current count, kept in the data) */
            {
                u8 cur = TrackByte(t);
                back = (u32)TrackByte(t) << 24; back |= (u32)TrackByte(t) << 16;
                back |= (u32)TrackByte(t) << 8; back |= TrackByte(t);
                if (cur == 0xFF) t->loc -= back;           /* for ever */
                else if (cur == 0) { cnt[0] = cnt[-1]; }    /* done: ready for next time */
                else { cnt[0] = cur - 1; t->loc -= back; }
            }
        }
    } else {
        chan = st & 15;
        switch (st & 0xF0) {
        case 0x90: {
            s32 key = (b & 0x80) ? TrackByte(t) : d1;
            s32 vel = TrackByte(t);
            u32 len = TrackVarLen(t);
            if (vel) NoteOn(chan, key, vel, len);
            break;
        }
        case 0x80: if (b & 0x80) TrackByte(t); TrackByte(t); break;
        case 0xA0: if (b & 0x80) TrackByte(t); TrackByte(t); break;
        case 0xB0: {
            s32 cc = (b & 0x80) ? TrackByte(t) : d1, val = TrackByte(t), i;
            if (cc == 7) sSeq.c[chan].vol = val;
            else if (cc == 10) sSeq.c[chan].pan = val;
            else if (cc == 91) sSeq.c[chan].fx = val;          /* (the reverb send: less of the dry sound) */
            for (i = 0; i < MUSIC_VOICES; i++)
                if (sVoice[i].env != ENV_OFF && !sVoice[i].sfx && sVoice[i].chan == chan) {
                    sVoice[i].vol = NoteVol(&sVoice[i]); sVoice[i].pan = NotePan(&sVoice[i]);
                }
            break;
        }
        case 0xC0: {
            s32 prog = (b & 0x80) ? TrackByte(t) : d1;
            const PInst *in = Inst(sSeq.file, sSeq.bank, prog);
            SetInst(chan, in);
            break;
        }
        case 0xD0: if (b & 0x80) TrackByte(t); break;
        case 0xE0: {
            s32 lo = (b & 0x80) ? TrackByte(t) : d1, hi = TrackByte(t), i;
            sSeq.c[chan].bend = ((hi << 7) | lo) - 8192;
            for (i = 0; i < MUSIC_VOICES; i++)
                if (sVoice[i].env != ENV_OFF && !sVoice[i].sfx && sVoice[i].chan == chan) sVoice[i].hz = NoteHz(&sVoice[i]);
            break;
        }
        default: break;
        }
    }
    if (t->loc) t->when += TrackVarLen(t);
}

static void SeqAdvance(f32 dt) {
    s32 n, guard = 0;
    if (!sSeq.playing) return;
    sSeq.tick += dt * sSeq.tps;
    for (;;) {
        s32 best = -1;
        f64 w = 1e30;
        for (n = 0; n < 16; n++) if (sSeq.t[n].loc && sSeq.t[n].when < w) { w = sSeq.t[n].when; best = n; }
        if (best < 0) { sSeq.playing = 0; return; }
        if (w > sSeq.tick || ++guard > 2000) return;
        TrackEvent(best);
    }
}

/* ------------------------------------------------------------------ the tick */

static f32 sSfxScale = 1.0f;

void audio_tick(f32 dt) {
    s32 i;
    plat_snd_lock();
    SeqAdvance(dt);
    for (i = 0; i < MAX_VOICES; i++) {
        Voice *v = &sVoice[i];
        f32 vol, pan, hz;
        if (v->env == ENV_OFF) continue;
        /* the envelope */
        if (v->remain > 0.0f) {
            f32 s = dt < v->remain ? dt : v->remain;
            v->level = (v->level > ENV_FLOOR ? v->level : ENV_FLOOR) * expf(v->step * s);
            v->remain -= s;
        }
        if (v->remain <= 0.0f) {
            v->level = v->target;
            if (v->env == ENV_ATTACK) {
                /* (a sound effect, as libultra's sound player: no decay time, it holds the attack
                   volume; else it ends when the decay does) */
                if (v->sfx && v->snd->decayTime == -1) v->env = ENV_SUSTAIN;
                else EnvStage(v, ENV_DECAY, v->snd->decayVol / 127.0f, v->snd->decayTime);
            } else if (v->env == ENV_DECAY) {
                if (v->sfx) {
                    VoiceRelease(v); continue; }
                v->env = ENV_SUSTAIN;
            }
            else if (v->env == ENV_RELEASE) { VoiceStop(v); continue; }
        }
        if (!v->sfx && v->env != ENV_RELEASE) {            /* a note's end */
            v->noteLeft -= dt;
            if (v->noteLeft <= 0.0f) VoiceRelease(v);
        }
        /* a one-shot wave's end */
        if (v->wave->loopCount == 0 && v->hz > 0.0f && !v->ended) {
            v->played += dt;
            if (v->played * v->hz >= (f32)v->wave->samples) {
                /* (a sound effect ends here; a music note keeps its voice until its release is
                   over, as libultra's sequence player: it frees a voice only then) */
                if (v->sfx || !v->started) { VoiceStop(v); continue; }
                plat_voice_stop(i);
                v->ended = 1;
            }
        }
        if (v->ended) continue;
        vol = v->vol * v->vol * v->level;                  /* (the N64 synthesizer squares the volume) */
        pan = v->pan;
        hz = v->hz;
        if (!v->started) {
            plat_voice_start(i, v->wave, (const u8 *)v->file + v->wave->off, hz, vol, pan);
            v->started = 1;
            v->lastHz = hz; v->lastVol = vol; v->lastPan = pan;
        } else if (hz != v->lastHz || vol != v->lastVol || pan != v->lastPan) {
            plat_voice_set(i, hz, vol, pan);
            v->lastHz = hz; v->lastVol = vol; v->lastPan = pan;
        }
    }
    plat_snd_unlock();
}

/* once a retrace (the game's audio frame counter) */
void audio_retrace(void) { sAudioFrame++; }

/* ------------------------------------------------------------------ N64AUDIO.C */

/* the sound effect slots (SfxSlot, as N64AUDIO.C) */
typedef struct Slot { s32 id; const void *bankFile; u32 serial; s32 looped; u32 frame; } Slot;
static Slot sPlaying[MAX_SFX], sStopping[MAX_SFX];
static s32 sSlots;                          /* B_800E2718 */
static u32 sOutputRate = 22050, sSerial;
static s32 sMusicVolume = 0x7FFF, sSfxVolume = 0x7FFF;   /* D_800B0AB8, D_800B0AB4 */
static f32 sVolScale = 1.0f;                             /* D_800B0ABC */

static void LoadDir(void) {
    u32 head[2];
    if (sDir) return;
    if (plat_sound_read(head, 0, 8) != 8 || memcmp(head, "CFSD", 4) != 0) { sDirN = 0; sDir = (u32 *)1; return; }
    sDirN = head[1];
    sDir = malloc(sDirN * 12);
    if (sDir == NULL || plat_sound_read(sDir, 8, sDirN * 12) != sDirN * 12) sDirN = 0;
}

static void ForgetFile(s32 k) {
    s32 i;
    PFile *f = sFiles[k].file;
    if (f == NULL) return;
    for (i = 0; i < MAX_VOICES; i++) if (sVoice[i].env != ENV_OFF && sVoice[i].file == f) VoiceStop(&sVoice[i]);
    for (i = 0; i < MAX_SFX; i++) if (sSfx[i].used && sSfx[i].file == f) sSfx[i].used = 0;
    if (sSeq.file == f) { sSeq.playing = 0; sSeq.file = NULL; }
    /* (another handle may share the file) */
    for (i = 0; i < MAX_FILES; i++) if (i != k && sFiles[i].file == f) { sFiles[k].file = NULL; return; }
    plat_snd_forget(f, sFiles[k].size);
    free(f);
    sFiles[k].file = NULL;
}

/* the game names a bank file by its handle; ours is found by the ROM address of its samples */
static void Register(const void *handle, u32 key, s32 global) {
    s32 i, k = -1, n;
    u32 off = 0, size = 0;
    PFile *f = NULL;
    LoadDir();
    for (i = 0; i < (s32)sDirN; i++) if (sDir[i * 3] == key) { off = sDir[i * 3 + 1]; size = sDir[i * 3 + 2]; }
    if (size == 0) return;
    plat_snd_lock();
    for (i = 0; i < MAX_FILES; i++) if (sFiles[i].file && sFiles[i].handle == handle) ForgetFile(i);
    for (i = 0; i < MAX_FILES; i++) if (sFiles[i].file && sFiles[i].key == key) f = sFiles[i].file;   /* already in */
    /* room: the fighters' files are let go of, oldest first, beyond the last two */
    for (;;) {
        s32 chars = 0, old = -1;
        for (i = 0; i < MAX_FILES; i++)
            if (sFiles[i].file && !sFiles[i].global) {
                chars++;
                if (old < 0 || sFiles[i].used < sFiles[old].used) old = i;
            }
        if (global || f != NULL || chars < 2 || old < 0) break;   /* (a file already in needs no room) */
        ForgetFile(old);
    }
    for (i = 0; i < MAX_FILES; i++) if (sFiles[i].file == NULL) { k = i; break; }
    plat_snd_unlock();
    if (k < 0) return;
    if (f == NULL) {
        f = malloc(size);
        if (f == NULL) return;
        n = (s32)plat_sound_read(f, off, size);
        if (n != (s32)size || memcmp(f->magic, "PBNK", 4) != 0) { free(f); return; }
    }
    plat_snd_lock();
    sFiles[k].handle = handle; sFiles[k].key = key; sFiles[k].file = f; sFiles[k].size = size; sFiles[k].global = global;
    sFiles[k].used = ++sUseClock;
    plat_snd_unlock();
}

void alBnkfNew(void *file, u32 table) { Register(file, table, 0); }

/* 0x800786B0: audio start-up (the game: 22050 Hz, 16 voices, 8 sound effects, fx type 1) */
void func_800786B0(u32 frequency, s32 maxVoices, s32 maxSounds, s32 fxType) {
    s32 i;
    (void)maxVoices; (void)fxType;
    sOutputRate = frequency;
    sOutputRateF = (f32)frequency;
    sSlots = maxSounds < MAX_SFX ? maxSounds : MAX_SFX;
    for (i = 0; i < MAX_SFX; i++) { sPlaying[i].id = -1; sStopping[i].id = -1; }
    sSeq.vol = 0x7FFF;
    plat_snd_init();
}

/* 0x80078E44: load a sound bank from the ROM (romTable: its sample data in the ROM) */
void *func_80078E44(u32 romAddr, u32 size, u32 romTable) {
    void *bank = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, 16, 0);   /* (the game's handle) */
    (void)romAddr; (void)size;
    Register(bank, romTable, 1);
    return bank;
}

/* the slots (N64AUDIO.C's rules) */
static Sfx *SfxId(s32 id) { return (id >= 0 && id < MAX_SFX && sSfx[id].used) ? &sSfx[id] : NULL; }
static void StopSlot(s32 i, s32 j) {
    Sfx *x = SfxId(sPlaying[i].id);
    if (x) SndpStop(x);
    sStopping[j] = sPlaying[i];
    sPlaying[i].id = -1;
}
static s32 FreeStoppingSlot(void) {
    s32 j;
    for (j = 0; j < sSlots; j++) if (sStopping[j].id < 0) break;
    return j;
}
static void FreeFinished(void) {                            /* func_8007B144 */
    s32 i;
    for (i = 0; i < sSlots; i++) {
        if (sPlaying[i].id >= 0) {
            Sfx *x = SfxId(sPlaying[i].id);
            if (x == NULL || (sPlaying[i].frame != sAudioFrame && !SndpState(x))) {
                if (x) x->used = 0;
                sPlaying[i].id = -1;
            }
        }
        if (sStopping[i].id >= 0) {
            Sfx *x = SfxId(sStopping[i].id);
            if (x == NULL || !SndpState(x)) {
                if (x) x->used = 0;
                sStopping[i].id = -1;
            }
        }
    }
}
static s32 FreeSlot(void) {                                 /* func_8007AFC8 */
    u32 oldest = 0xFFFFFFFF;
    s32 i, j, best = 0;
    FreeFinished();
    for (i = 0; i < sSlots; i++) {
        if (sPlaying[i].id < 0) return i;
        if (sPlaying[i].serial < oldest) { best = i; oldest = sPlaying[i].serial; }
    }
    if (sSlots <= 0) return -1;
    j = FreeStoppingSlot();
    if (j >= sSlots) return -1;
    StopSlot(best, j);
    return best;
}

static const PSound *LookUp(const void *bf, s32 bank, s32 inst, s32 sound, PFile **pf) {
    PFile *f = FileOf(bf);
    *pf = f;
    if (f == NULL) return NULL;
    return Sound(f, Inst(f, bank, inst), sound);
}

/* the sample rate hidden in a sound's key map (N64AUDIO.C) */
static u32 KeyRate(const PSound *s) { return s->keyMin * 10000u + s->keyMax * 100u + s->keyBase; }

/* 0x800797EC: play a sound effect (see N64AUDIO.C) */
void func_800797EC(ALBankFile *bf, s32 bank, s32 inst, s32 sound, s32 volume, s32 pan, s32 restart,
                   f32 pitch, s32 randomPitch) {
    PFile *f;
    const PSound *snd;
    s32 looped, i, j, slot, id;
    f32 p;
    plat_snd_lock();
    snd = LookUp(bf, bank, inst, sound, &f);
    if (snd == NULL) { plat_snd_unlock(); return; }
    looped = snd->releaseTime != 0;
    FreeFinished();
    if (looped) {
        for (i = 0; i < sSlots; i++) {
            if (sPlaying[i].id < 0 || !sPlaying[i].looped || sPlaying[i].bankFile != bf) continue;
            if (!restart) { plat_snd_unlock(); return; }
            j = FreeStoppingSlot();
            if (j >= sSlots) break;
            StopSlot(i, j);
        }
    }
    slot = FreeSlot();
    if (slot >= 0) {
        id = SndpAllocate(f, snd);
        if (id >= 0) {
            Sfx *x = &sSfx[id];
            if (randomPitch) {                             /* (guRandom as the original: the game's
                                                              random numbers stay in step) */
                s32 r = (s32)guRandom() % 1024 - 512;
                f32 ff = (f32)r / 512.0f;
                p = looped ? ff * 0.05f + 1.0f : ff * 0.04f + 1.0f;
            } else {
                p = 1.0f;
            }
            x->pitch = (f32)KeyRate(snd) * p * pitch;
            x->vol = (volume * (f32)sSfxVolume / 0x7FFF) / 32767.0f * (snd->vol / 127.0f) * sSfxScale;
            x->pan = (pan + 0x40) / 127.0f;
            sPlaying[slot].id = id; sPlaying[slot].bankFile = bf; sPlaying[slot].serial = sSerial++;
            sPlaying[slot].looped = looped; sPlaying[slot].frame = sAudioFrame;
            SndpPlay(x);
        }
    }
    plat_snd_unlock();
}

/* 0x80079D70: play a sound at the music volume, at its own pitch (the intro's tune) */
void func_80079D70(ALBankFile *bf, s32 bank, s32 inst, s32 sound, s32 volume, s32 pan) {
    PFile *f;
    const PSound *snd;
    s32 slot, id;
    plat_snd_lock();
    snd = LookUp(bf, bank, inst, sound, &f);
    if (snd) {
        FreeFinished();
        slot = FreeSlot();
        if (slot >= 0 && (id = SndpAllocate(f, snd)) >= 0) {
            Sfx *x = &sSfx[id];
            x->pitch = (f32)KeyRate(snd);
            x->vol = (volume * (f32)sMusicVolume / 0x7FFF) / 32767.0f * (snd->vol / 127.0f) * sVolScale;
            x->pan = (pan + 0x40) / 127.0f;
            sPlaying[slot].id = id; sPlaying[slot].bankFile = bf; sPlaying[slot].serial = sSerial++;
            sPlaying[slot].looped = 0; sPlaying[slot].frame = sAudioFrame;
            SndpPlay(x);
        }
    }
    plat_snd_unlock();
}

/* 0x8007A0BC: stop the looped sounds of a bank file */
void func_8007A0BC(ALBankFile *bf) {
    s32 i, j;
    plat_snd_lock();
    for (i = 0; i < sSlots; i++) {
        if (sPlaying[i].id < 0 || !sPlaying[i].looped || sPlaying[i].bankFile != bf) continue;
        j = FreeStoppingSlot();
        if (j >= sSlots) break;
        StopSlot(i, j);
    }
    plat_snd_unlock();
}

/* 0x8007A2C8: n64PlaySFX: whether the sound is a looped one */
s32 n64PlaySFX(ALBankFile *bf, s32 bank, s32 inst, s32 sound) {
    PFile *f;
    const PSound *s = LookUp(bf, bank, inst, sound, &f);
    return s ? s->releaseTime != 0 : 0;
}

/* 0x8007A3B8: is a looped sound of this bank file playing? */
s32 func_8007A3B8(ALBankFile *bf) {
    s32 i, r = 0;
    plat_snd_lock();
    FreeFinished();
    for (i = 0; i < sSlots; i++)
        if (sPlaying[i].id >= 0 && sPlaying[i].looped && sPlaying[i].bankFile == bf) r = 1;
    plat_snd_unlock();
    return r;
}

/* 0x8007A530: stop every sound effect */
void func_8007A530(void) {
    s32 i, j;
    plat_snd_lock();
    FreeFinished();
    for (i = 0; i < sSlots; i++) {
        if (sPlaying[i].id < 0) continue;
        j = FreeStoppingSlot();
        if (j >= sSlots) break;
        StopSlot(i, j);
    }
    plat_snd_unlock();
}

/* 0x8007A710: stop the first playing sound effect; 1 if there was one */
s32 func_8007A710(void) {
    s32 i, j, found = 0;
    plat_snd_lock();
    FreeFinished();
    for (i = 0; i < sSlots; i++) {
        if (sPlaying[i].id < 0) continue;
        j = FreeStoppingSlot();
        if (j < sSlots) StopSlot(i, j);
        found = 1;
        break;
    }
    plat_snd_unlock();
    return found;
}

/* 0x80078F60: n64FreeSampleBank - stop every sound, then free the bank */
void n64FreeSampleBank(void *bank) {
    s32 i;
    plat_snd_lock();
    for (i = 0; i < sSlots; i++) {
        if (sPlaying[i].id >= 0) { Sfx *x = SfxId(sPlaying[i].id); if (x) { SndpStop(x); x->used = 0; } sPlaying[i].id = -1; }
        if (sStopping[i].id >= 0) { Sfx *x = SfxId(sStopping[i].id); if (x) x->used = 0; sStopping[i].id = -1; }
    }
    for (i = 0; i < MAX_FILES; i++) if (sFiles[i].file && sFiles[i].handle == bank) ForgetFile(i);
    plat_snd_unlock();
    if (bank) mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, bank);
}

/* 0x800794CC: the instrument bank the music uses */
void func_800794CC(ALBankFile *bankFile, s32 bank) {
    plat_snd_lock();
    sSeq.file = FileOf(bankFile);
    sSeq.bank = bank;
    plat_snd_unlock();
}

/* 0x800795B4: play a tune (a compressed MIDI sequence) */
void func_800795B4(void *seqData) {
    func_800796C0();
    plat_snd_lock();
    sSeq.vol = sMusicVolume;
    if (sSeq.file) SeqStart(seqData);
    plat_snd_unlock();
}

/* 0x800796C0: stop the music */
void func_800796C0(void) {
    plat_snd_lock();
    SeqStop();
    plat_snd_unlock();
}

/* 0x8007A8F4: music volume */
void func_8007A8F4(s32 volume) {
    s32 i;
    sMusicVolume = volume;
    plat_snd_lock();
    sSeq.vol = (s32)((f32)volume * sVolScale);
    for (i = 0; i < MUSIC_VOICES; i++) if (sVoice[i].env != ENV_OFF && !sVoice[i].sfx) sVoice[i].vol = NoteVol(&sVoice[i]);
    plat_snd_unlock();
}
s32 n64Audio_GetMusicVolume_8007A9F8(void) { return sMusicVolume; }

/* 0x8007AA04: sound effects volume (the playing ones too) */
void func_8007AA04(s32 volume) {
    s32 i;
    f32 old = sSfxScale;
    sSfxVolume = volume;
    plat_snd_lock();
    sSfxScale = sVolScale;
    /* (N64AUDIO.C sets every playing effect to this volume) */
    for (i = 0; i < MAX_SFX; i++)
        if (sSfx[i].used) { sSfx[i].vol = (old > 0.0f ? sSfx[i].vol / old : sSfx[i].vol) * sSfxScale; SndpUpdate(&sSfx[i]); }
    plat_snd_unlock();
}
s32 n64Audio_GetSfxVolume_8007AB64(void) { return sSfxVolume; }

/* 0x8007AB70: master volume scale */
void func_8007AB70(f32 scale) {
    sVolScale = scale;
    func_8007A8F4(sMusicVolume);
    func_8007AA04(sSfxVolume);
}
f32 n64Audio_GetVolumeScale_8007AC5C(void) { return sVolScale; }
