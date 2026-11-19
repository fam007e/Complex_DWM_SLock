#ifndef CONFIG_H
#define CONFIG_H

/* ── Stringification helpers ────────────────────────────────────────────── */
#define STRINGIFY(x) #x
#define TOSTRING(x)  STRINGIFY(x)

/* ── Version (set by the Makefile via -DBUILD_VERSION=...) ──────────────── */
#ifndef BUILD_VERSION
#define BUILD_VERSION unknown
#endif
#define VERSION TOSTRING(BUILD_VERSION)

/* ── Asset paths ─────────────────────────────────────────────────────────── */
/* DATADIR is passed by the Makefile (-DDATADIR=\"...\"); this default only
 * applies when compiling by hand. */
#ifndef DATADIR
#define DATADIR "/usr/local/share/slock"
#endif

/*
 * Avatar search order:
 *   1. $HOME_FROM_PASSWD/.config/slock/avatar.png
 *   2. $HOME_FROM_PASSWD/.face
 *   3. DATADIR/avatar.png                (installed default)
 *   4. assets/images/avatar.png          (project-root fallback for `-t`)
 *
 * Font search order (first readable file wins, otherwise FONT is used):
 *   1. $HOME_FROM_PASSWD/.config/slock/font.ttf
 *   2. DATADIR/font.ttf
 *   3. assets/fonts/font.ttf
 *
 * The home directory comes from getpwuid(), never from $HOME.
 */
#define USER_AVATAR_HOME     "/.config/slock/avatar.png"
#define USER_AVATAR_HOME_ALT "/.face"
#define USER_FONT_HOME       "/.config/slock/font.ttf"
#define USER_AVATAR          DATADIR "/avatar.png"
#define USER_AVATAR_FALLBACK "assets/images/avatar.png"
#define FONT_FILE            DATADIR "/font.ttf"
#define FONT_FILE_FALLBACK   "assets/fonts/font.ttf"

/* ── Auth ────────────────────────────────────────────────────────────────── */
#define MAX_ATTEMPTS 10

/* ── Background ──────────────────────────────────────────────────────────── */
/* Blur runs on a 1/BLUR_SCALE copy, so the effective radius is
 * BLUR_SCALE * BLUR_RADIUS px (4 * 5 = 20 px). */
#define BLUR_SCALE   4
#define BLUR_RADIUS  5
#define DIM_AMOUNT   0.30f   /* uniform darkening               */
#define VIGNETTE     0.35f   /* extra darkening towards corners */

/* ── Avatar ──────────────────────────────────────────────────────────────── */
#define AVATAR_SIZE  150
#define RING_W       3
#define RING_GAP     4
#define SHADOW_R     18
#define SPR_PAD      (RING_GAP + RING_W + SHADOW_R)

/* ── Fonts ───────────────────────────────────────────────────────────────── */
/* FONT is only used when no font file is found (any fontconfig family). */
#define FONT           "sans-serif"
#define FONT_SIZE      12    /* username / password field */
#define TIME_FONT_SIZE 64    /* large clock               */
#define DATE_FONT_SIZE 18    /* date and display name     */
#define HINT_FONT_SIZE 11    /* hints / warnings          */

/* ── Time / date formats ─────────────────────────────────────────────────── */
/*
 * 12-hour clock: SHOW_AMPM 1 with TIME_FORMAT "%I:%M" (leading zero is
 * stripped in code, so 03:45 shows as 3:45) and a smaller AM/PM suffix.
 * 24-hour clock: SHOW_AMPM 0 with TIME_FORMAT "%H:%M".
 */
#define SHOW_AMPM      1
#define TIME_FORMAT    "%I:%M"
#define AMPM_FONT_SIZE 22    /* size of the AM/PM suffix        */
#define AMPM_GAP       6     /* px between the time and AM/PM   */
#define DATE_FORMAT "%A %B %d"

/* ── Password field ──────────────────────────────────────────────────────── */
#define PASSWD_FIELD_W 280
#define PASSWD_FIELD_H 34

/* ── Colours (0xRRGGBB) ──────────────────────────────────────────────────── */
#define COLOR_BACKGROUND 0x000000
#define COLOR_TEXT_LIGHT 0xFFFFFF
#define COLOR_FAILURE    0xFF2222
#define COLOR_SUCCESS    0x22FF22
#define COLOR_WARN_MID   0xFF8800
#define COLOR_WARN_LOW   0xFFDD00

#endif /* CONFIG_H */
