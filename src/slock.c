#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xft/Xft.h>
#include <fontconfig/fontconfig.h>
#include <Imlib2.h>
#include <security/pam_appl.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "config.h"
#include "blur.h"

/* ── Globals ─────────────────────────────────────────────────────────────── */

static Display *dpy;
static int screen;
static Window root, win;
static GC gc;
static XftFont *xft_font;        /* username / password field */
static XftFont *xft_font_time;   /* large clock               */
static XftFont *xft_font_date;   /* date + display name       */
static XftFont *xft_font_hint;   /* hints / warnings          */
static XftFont *xft_font_ampm;   /* small AM/PM suffix        */
static XftDraw *xft_draw;        /* draws into `buf`          */
static Pixmap bg_pixmap;         /* blurred + dimmed desktop  */
static Pixmap buf;               /* back buffer               */

static Imlib_Image spr_avatar, spr_panel, spr_pill[3]; /* pill: normal/wrong/ok */

static int scr_w, scr_h;                 /* whole X screen */
static int mon_x, mon_y, mon_w, mon_h;   /* primary monitor */

static char user_home[PATH_MAX];
static char display_name[128];
static char font_family[256];
static int  custom_font;
static uid_t saved_euid;

static char passwd[256];
static int  pwlen;
static char *password_ptr;

static int failed_attempts;
static int test_mode;
static int shake_dx;
static volatile sig_atomic_t running = 1;

enum SessionState { SESSION_IDLE, SESSION_ACTIVE };
static int session_state = SESSION_IDLE;

enum AuthState { STATE_NORMAL, STATE_WRONG, STATE_CORRECT };
static int auth_state = STATE_NORMAL;

#define PANEL_W       (PASSWD_FIELD_W + 120)
#define PANEL_TOP_PAD 17
#define PANEL_H       (PANEL_TOP_PAD + AVATAR_SIZE + 84 + PASSWD_FIELD_H + 70)

/* ── Cleanup / die ───────────────────────────────────────────────────────── */

static void free_sprite(Imlib_Image *img) {
    if (*img) {
        imlib_context_set_image(*img);
        imlib_free_image();
        *img = NULL;
    }
}

static void cleanup(void) {
    free_sprite(&spr_avatar);
    free_sprite(&spr_panel);
    for (int i = 0; i < 3; i++) free_sprite(&spr_pill[i]);
    memset(passwd, 0, sizeof passwd);

    if (dpy) {
        if (xft_font)      XftFontClose(dpy, xft_font);
        if (xft_font_time) XftFontClose(dpy, xft_font_time);
        if (xft_font_date) XftFontClose(dpy, xft_font_date);
        if (xft_font_hint) XftFontClose(dpy, xft_font_hint);
        if (xft_font_ampm) XftFontClose(dpy, xft_font_ampm);
        if (xft_draw)      XftDrawDestroy(xft_draw);
        if (gc)            XFreeGC(dpy, gc);
        if (buf)           XFreePixmap(dpy, buf);
        if (bg_pixmap)     XFreePixmap(dpy, bg_pixmap);
        if (win)           XDestroyWindow(dpy, win);
        XCloseDisplay(dpy);
    }
    xft_font = xft_font_time = xft_font_date = xft_font_hint = xft_font_ampm = NULL;
    xft_draw = NULL; gc = NULL; buf = bg_pixmap = 0; win = 0; dpy = NULL;
}

static void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    cleanup();
    exit(1);
}

/* ── PAM ─────────────────────────────────────────────────────────────────── */

static int conv(int num_msg, const struct pam_message **msg,
                struct pam_response **resp, void *appdata_ptr) {
    (void)appdata_ptr;
    struct pam_response *reply;

    if (num_msg <= 0 || num_msg > PAM_MAX_NUM_MSG)
        return PAM_CONV_ERR;
    if (!(reply = calloc((size_t)num_msg, sizeof *reply)))
        return PAM_BUF_ERR;

    for (int i = 0; i < num_msg; i++) {
        switch (msg[i]->msg_style) {
        case PAM_PROMPT_ECHO_OFF:
            reply[i].resp = strdup(password_ptr);
            if (!reply[i].resp) goto fail;
            break;
        case PAM_ERROR_MSG:
            fprintf(stderr, "%s\n", msg[i]->msg);
            break;
        case PAM_TEXT_INFO:
            break;
        default:
            goto fail;
        }
    }
    *resp = reply;
    return PAM_SUCCESS;

fail:
    for (int i = 0; i < num_msg; i++) {
        if (reply[i].resp) {
            memset(reply[i].resp, 0, strlen(reply[i].resp));
            free(reply[i].resp);
        }
    }
    free(reply);
    return PAM_CONV_ERR;
}

static int verify_password(const char *pw, const char *user) {
    pam_handle_t *pamh = NULL;
    struct pam_conv pamc = { conv, NULL };
    int ret;

    password_ptr = (char *)pw;
    ret = pam_start("slock", user, &pamc, &pamh);
    if (ret != PAM_SUCCESS) goto end;
    ret = pam_authenticate(pamh, 0);
    if (ret != PAM_SUCCESS) goto end;
    ret = pam_acct_mgmt(pamh, 0);
end:
    if (pamh) pam_end(pamh, ret);
    password_ptr = NULL;
    return ret == PAM_SUCCESS;
}

/* ── Reading user-owned files without root privileges ───────────────────── */

/*
 * slock is setuid root. Anything inside the user's home is opened with the
 * effective UID temporarily set to the real user, so a symlink or crafted
 * file can't be used to make root read something it shouldn't.
 */
static int as_user_begin(void) {
    if (geteuid() == getuid()) return 1;
    return seteuid(getuid()) == 0;
}

static void as_user_end(void) {
    if (geteuid() != saved_euid && seteuid(saved_euid) != 0)
        die("slock: cannot restore privileges\n");
}

static int is_regular_file(const char *path, off_t max_size) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_size <= max_size;
}

static Imlib_Image try_load(const char *path) {
    if (!is_regular_file(path, 32 * 1024 * 1024)) return NULL;
    return imlib_load_image(path);
}

static Imlib_Image load_avatar(void) {
    Imlib_Image img = NULL;
    char path[PATH_MAX];

    if (user_home[0] && as_user_begin()) {
        snprintf(path, sizeof path, "%s%s", user_home, USER_AVATAR_HOME);
        img = try_load(path);
        if (!img) {
            snprintf(path, sizeof path, "%s%s", user_home, USER_AVATAR_HOME_ALT);
            img = try_load(path);
        }
        as_user_end();
    }
    if (!img) img = try_load(USER_AVATAR);
    if (!img) img = try_load(USER_AVATAR_FALLBACK);
    return img;
}

/* ── Fonts ───────────────────────────────────────────────────────────────── */

/* Register a TTF with fontconfig and learn its family name. */
static int register_font(const char *file) {
    if (access(file, R_OK) != 0 || !is_regular_file(file, 64 * 1024 * 1024))
        return 0;
    if (!FcConfigAppFontAddFile(NULL, (const FcChar8 *)file))
        return 0;

    int ok = 0;
    FcFontSet *fs = FcFontSetCreate();
    if (fs) {
        if (FcFileScan(fs, NULL, NULL, NULL, (const FcChar8 *)file, FcFalse) &&
            fs->nfont > 0) {
            FcChar8 *fam = NULL;
            if (FcPatternGetString(fs->fonts[0], FC_FAMILY, 0, &fam) == FcResultMatch) {
                snprintf(font_family, sizeof font_family, "%s", (const char *)fam);
                ok = 1;
            }
        }
        FcFontSetDestroy(fs);
    }
    return ok;
}

static void setup_font_family(void) {
    char path[PATH_MAX];

    if (user_home[0] && as_user_begin()) {
        snprintf(path, sizeof path, "%s%s", user_home, USER_FONT_HOME);
        custom_font = register_font(path);
        as_user_end();
    }
    if (!custom_font) custom_font = register_font(FONT_FILE);
    if (!custom_font) custom_font = register_font(FONT_FILE_FALLBACK);
    if (!custom_font) snprintf(font_family, sizeof font_family, "%s", FONT);
}

static XftFont *open_font(double size, int light) {
    FcPattern *pat = FcPatternCreate();
    if (!pat) return NULL;
    FcPatternAddString(pat, FC_FAMILY, (const FcChar8 *)font_family);
    FcPatternAddDouble(pat, FC_SIZE, size);
    if (light && !custom_font)
        FcPatternAddInteger(pat, FC_WEIGHT, FC_WEIGHT_LIGHT);

    FcResult res;
    FcPattern *match = XftFontMatch(dpy, screen, pat, &res);
    FcPatternDestroy(pat);
    if (!match) return NULL;

    XftFont *f = XftFontOpenPattern(dpy, match);
    if (!f) FcPatternDestroy(match);
    return f;
}

/* ── Colour + text helpers ───────────────────────────────────────────────── */

/* XRender wants premultiplied colour, so scale the channels by alpha. */
static void make_xft_color(unsigned long rgb, float alpha, XftColor *out) {
    XRenderColor c;
    c.red   = (unsigned short)(((rgb >> 16) & 0xFF) * 257 * alpha);
    c.green = (unsigned short)(((rgb >>  8) & 0xFF) * 257 * alpha);
    c.blue  = (unsigned short)(( rgb        & 0xFF) * 257 * alpha);
    c.alpha = (unsigned short)(alpha * 0xFFFF);
    XftColorAllocValue(dpy, DefaultVisual(dpy, screen),
                       DefaultColormap(dpy, screen), &c, out);
}

static void free_xft_color(XftColor *c) {
    XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen), c);
}

/* Left-aligned text with a soft drop shadow. `y` is the baseline. */
static void text_at(XftFont *f, XftColor *c, int x, int y, const char *s) {
    XftColor sh;
    make_xft_color(0x000000, 0.35f, &sh);
    XftDrawStringUtf8(xft_draw, &sh, f, x + 1, y + 2,
                      (const XftChar8 *)s, (int)strlen(s));
    free_xft_color(&sh);
    XftDrawStringUtf8(xft_draw, c, f, x, y, (const XftChar8 *)s, (int)strlen(s));
}

static int text_width(XftFont *f, const char *s) {
    XGlyphInfo e;
    XftTextExtentsUtf8(dpy, f, (const XftChar8 *)s, (int)strlen(s), &e);
    return e.xOff;
}

static void text_center(XftFont *f, XftColor *c, int cx, int y, const char *s) {
    text_at(f, c, cx - text_width(f, s) / 2, y, s);
}

/* Big time with a small AM/PM tucked close, centred as one unit. */
static void draw_clock(XftColor *c, int cx, int y, const char *ts, const char *ap) {
    int tw = text_width(xft_font_time, ts);
    int total = tw;
    if (ap[0]) total += AMPM_GAP + text_width(xft_font_ampm, ap);
    int x = cx - total / 2;
    text_at(xft_font_time, c, x, y, ts);
    if (ap[0]) text_at(xft_font_ampm, c, x + tw + AMPM_GAP, y, ap);
}

/* ── Sprites (pre-rendered, anti-aliased) ────────────────────────────────── */

typedef struct { float r, g, b, a; } px_t;   /* premultiplied */

static float clampf(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

static void over(px_t *d, float r, float g, float b, float a) {
    d->r = r * a + d->r * (1 - a);
    d->g = g * a + d->g * (1 - a);
    d->b = b * a + d->b * (1 - a);
    d->a = a + d->a * (1 - a);
}

/* Premultiplied float pixel → Imlib2 non-premultiplied ARGB. */
static DATA32 pack(px_t p) {
    if (p.a <= 0.001f) return 0;
    return ((DATA32)(p.a * 255 + 0.5f) << 24)
         | ((DATA32)(clampf(p.r / p.a) * 255 + 0.5f) << 16)
         | ((DATA32)(clampf(p.g / p.a) * 255 + 0.5f) <<  8)
         |  (DATA32)(clampf(p.b / p.a) * 255 + 0.5f);
}

static Imlib_Image build_avatar_sprite(void) {
    const int   S  = AVATAR_SIZE + 2 * SPR_PAD;
    const float c  = S / 2.0f;
    const float Ri = AVATAR_SIZE / 2.0f;
    const float R0 = Ri + RING_GAP, R1 = R0 + RING_W;

    Imlib_Image face = NULL, src = load_avatar();
    if (src) {
        imlib_context_set_image(src);
        int w = imlib_image_get_width(), h = imlib_image_get_height();
        int m = w < h ? w : h;
        if (m > 0)
            face = imlib_create_cropped_scaled_image((w - m) / 2, (h - m) / 2,
                                                     m, m, AVATAR_SIZE, AVATAR_SIZE);
        imlib_context_set_image(src);
        imlib_free_image();
    }
    DATA32 *fd = NULL;
    if (face) {
        imlib_context_set_image(face);
        fd = imlib_image_get_data_for_reading_only();
    }

    Imlib_Image out = imlib_create_image(S, S);
    if (!out) { free_sprite(&face); return NULL; }
    imlib_context_set_image(out);
    imlib_image_set_has_alpha(1);
    DATA32 *od = imlib_image_get_data();

    for (int y = 0; y < S; y++) {
        for (int x = 0; x < S; x++) {
            float dx = x + 0.5f - c, dy = y + 0.5f - c;
            float d = sqrtf(dx * dx + dy * dy);
            px_t p = { 0, 0, 0, 0 };

            /* soft drop shadow */
            float sh = clampf(1.0f - (d - R1) / SHADOW_R);
            over(&p, 0, 0, 0, sh * sh * 0.45f);

            /* photo (or a neutral disc when no image was found) */
            float cov = clampf(Ri - d + 0.5f);
            if (cov > 0) {
                float cr = .33f, cg = .33f, cb = .40f, ca = 1.0f;
                if (fd) {
                    int ix = x - SPR_PAD, iy = y - SPR_PAD;
                    ix = ix < 0 ? 0 : (ix >= AVATAR_SIZE ? AVATAR_SIZE - 1 : ix);
                    iy = iy < 0 ? 0 : (iy >= AVATAR_SIZE ? AVATAR_SIZE - 1 : iy);
                    DATA32 q = fd[iy * AVATAR_SIZE + ix];
                    ca = ((q >> 24) & 255) / 255.0f;
                    cr = ((q >> 16) & 255) / 255.0f;
                    cg = ((q >>  8) & 255) / 255.0f;
                    cb = ( q        & 255) / 255.0f;
                }
                over(&p, cr, cg, cb, ca * cov);
            }

            /* ring, separated from the photo by RING_GAP */
            float ring = clampf(R1 - d + 0.5f) * clampf(d - R0 + 0.5f);
            over(&p, 1, 1, 1, 0.85f * ring);

            od[y * S + x] = pack(p);
        }
    }
    imlib_context_set_image(out);
    imlib_image_put_back_data(od);
    free_sprite(&face);
    return out;
}

/* Rounded-rectangle distance: <0 inside, >0 outside (in pixels). */
static float rrect_dist(float px, float py, float w, float h, float r) {
    float qx = px < r ? r : (px > w - r ? w - r : px);
    float qy = py < r ? r : (py > h - r ? h - r : py);
    float dx = px - qx, dy = py - qy;
    return sqrtf(dx * dx + dy * dy) - r;
}

static Imlib_Image build_pill(float br, float bg, float bb, float border_a) {
    const int w = PASSWD_FIELD_W, h = PASSWD_FIELD_H;
    Imlib_Image img = imlib_create_image(w, h);
    if (!img) return NULL;
    imlib_context_set_image(img);
    imlib_image_set_has_alpha(1);
    DATA32 *d = imlib_image_get_data();

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float dist = rrect_dist(x + 0.5f, y + 0.5f, w, h, h / 2.0f);
            float fill = clampf(0.5f - dist);
            float edge = fill * clampf(dist + 1.5f);          /* ~1px border */
            px_t p = { 0, 0, 0, 0 };
            over(&p, 0, 0, 0, 0.30f * fill);
            over(&p, 1, 1, 1, 0.08f * fill);
            over(&p, br, bg, bb, border_a * edge);
            d[y * w + x] = pack(p);
        }
    }
    imlib_context_set_image(img);
    imlib_image_put_back_data(d);
    return img;
}

static Imlib_Image build_panel(int w, int h, float radius) {
    Imlib_Image img = imlib_create_image(w, h);
    if (!img) return NULL;
    imlib_context_set_image(img);
    imlib_image_set_has_alpha(1);
    DATA32 *d = imlib_image_get_data();

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float dist = rrect_dist(x + 0.5f, y + 0.5f, w, h, radius);
            float fill = clampf(0.5f - dist);
            float edge = fill * clampf(dist + 1.5f);
            px_t p = { 0, 0, 0, 0 };
            over(&p, 0, 0, 0, 0.22f * fill);
            over(&p, 1, 1, 1, 0.10f * edge);
            d[y * w + x] = pack(p);
        }
    }
    imlib_context_set_image(img);
    imlib_image_put_back_data(d);
    return img;
}

static void init_sprites(void) {
    imlib_context_set_display(dpy);
    imlib_context_set_visual(DefaultVisual(dpy, screen));
    imlib_context_set_colormap(DefaultColormap(dpy, screen));
    imlib_context_set_drawable(buf);

    spr_avatar  = build_avatar_sprite();
    spr_panel   = build_panel(PANEL_W, PANEL_H, 22.0f);
    spr_pill[0] = build_pill(1.0f, 1.0f, 1.0f, 0.45f);
    spr_pill[1] = build_pill(1.0f, 0.2f, 0.2f, 0.95f);
    spr_pill[2] = build_pill(0.2f, 1.0f, 0.3f, 0.95f);
}

static void blit(Imlib_Image img, int x, int y) {
    if (!img) return;
    imlib_context_set_image(img);
    imlib_context_set_blend(1);
    imlib_render_image_on_drawable(x, y);
}

/* ── Misc helpers ────────────────────────────────────────────────────────── */

static void get_monitor(void) {
    mon_x = mon_y = 0;
    mon_w = scr_w;
    mon_h = scr_h;

    int n = 0;
    XRRMonitorInfo *m = XRRGetMonitors(dpy, root, True, &n);
    if (!m) return;
    if (n > 0) {
        int pick = 0;
        for (int i = 0; i < n; i++)
            if (m[i].primary) { pick = i; break; }
        mon_x = m[pick].x;     mon_y = m[pick].y;
        mon_w = m[pick].width; mon_h = m[pick].height;
    }
    XRRFreeMonitors(m);
}

static int caps_lock_on(void) {
    Window rw, cw;
    int rx, ry, wx, wy;
    unsigned int mask = 0;
    XQueryPointer(dpy, root, &rw, &cw, &rx, &ry, &wx, &wy, &mask);
    return (mask & LockMask) != 0;
}

/* Number of UTF-8 characters in the password buffer. */
static int pw_chars(void) {
    int n = 0;
    for (int i = 0; i < pwlen; i++)
        if (((unsigned char)passwd[i] & 0xC0) != 0x80) n++;
    return n;
}

/* ── Drawing ─────────────────────────────────────────────────────────────── */

static void draw_lock_screen(void) {
    const int cx = mon_x + mon_w / 2;
    const int active = (session_state == SESSION_ACTIVE);

    XCopyArea(dpy, bg_pixmap, buf, gc, 0, 0, scr_w, scr_h, 0, 0);

    /* full-screen feedback tint (premultiplied, ~25% opacity) */
    if (auth_state == STATE_WRONG) {
        XRenderColor t = { 0x4000, 0, 0, 0x4000 };
        XRenderFillRectangle(dpy, PictOpOver, XftDrawPicture(xft_draw),
                             &t, 0, 0, scr_w, scr_h);
    } else if (auth_state == STATE_CORRECT) {
        XRenderColor t = { 0, 0x4000, 0, 0x4000 };
        XRenderFillRectangle(dpy, PictOpOver, XftDrawPicture(xft_draw),
                             &t, 0, 0, scr_w, scr_h);
    }

    XftColor col, dim;
    make_xft_color(COLOR_TEXT_LIGHT, 1.0f, &col);
    make_xft_color(COLOR_TEXT_LIGHT, 0.70f, &dim);

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[32] = "", ap[16] = "", ds[64] = "";
    if (tm) {
        strftime(ts, sizeof ts, TIME_FORMAT, tm);
        strftime(ds, sizeof ds, DATE_FORMAT, tm);
#if SHOW_AMPM
        strftime(ap, sizeof ap, "%p", tm);
        if (ts[0] == '0') memmove(ts, ts + 1, strlen(ts));   /* 03:45 → 3:45 */
#endif
    }

    int time_y = mon_y + (active ? mon_h / 6 : mon_h * 2 / 5);
    draw_clock(&col, cx, time_y, ts, ap);
    text_center(xft_font_date, &dim, cx, time_y + 44, ds);

    if (!active) {
        text_center(xft_font_hint, &dim, cx, time_y + 100,
                    "Click or press a key to unlock");
    } else {
        int ay = mon_y + mon_h / 2 - AVATAR_SIZE - 60;
        if (ay < time_y + 72) ay = time_y + 72;      /* small screens */
        int fy   = ay + AVATAR_SIZE + 84;
        int fx_c = cx + shake_dx;                    /* shaken centre */

        blit(spr_panel, cx - PANEL_W / 2, ay - PANEL_TOP_PAD);
        blit(spr_avatar, cx - AVATAR_SIZE / 2 - SPR_PAD, ay - SPR_PAD);

        int hour = tm ? tm->tm_hour : 12;
        const char *greet = hour < 12 ? "Good morning"
                          : hour < 18 ? "Good afternoon" : "Good evening";
        text_center(xft_font_hint, &dim, cx, ay + AVATAR_SIZE + 32, greet);
        text_center(xft_font_date, &col, cx, ay + AVATAR_SIZE + 60, display_name);

        int idx = auth_state == STATE_WRONG ? 1 : auth_state == STATE_CORRECT ? 2 : 0;
        blit(spr_pill[idx], fx_c - PASSWD_FIELD_W / 2, fy);

        int base = fy + (PASSWD_FIELD_H + xft_font->ascent - xft_font->descent) / 2;

        if (auth_state == STATE_CORRECT) {
            XftColor ok;
            make_xft_color(COLOR_SUCCESS, 1.0f, &ok);
            text_center(xft_font, &ok, fx_c, base, "Access Granted");
            free_xft_color(&ok);
        } else if (pwlen == 0) {
            XftColor ph;
            make_xft_color(0xFFFFFF, 0.5f, &ph);
            text_center(xft_font, &ph, fx_c, base, "Password");
            free_xft_color(&ph);
        } else {
            char dots[24 * 3 + 1];
            int n = pw_chars();
            if (n > 24) n = 24;
            for (int i = 0; i < n; i++) memcpy(dots + i * 3, "\xE2\x80\xA2", 3); /* • */
            dots[n * 3] = '\0';
            text_center(xft_font, &col, fx_c, base, dots);
        }

        int msg_y = fy + PASSWD_FIELD_H + 30;
        if (auth_state == STATE_WRONG) {
            XftColor err;
            make_xft_color(COLOR_FAILURE, 1.0f, &err);
            text_center(xft_font, &err, cx, msg_y, "Incorrect password");
            free_xft_color(&err);

            int rem = MAX_ATTEMPTS - failed_attempts;
            if (rem > 0) {
                char warn[64];
                snprintf(warn, sizeof warn, "%d attempt%s remaining",
                         rem, rem == 1 ? "" : "s");
                unsigned long rgb = rem <= 2 ? COLOR_FAILURE
                                  : rem <= 5 ? COLOR_WARN_MID : COLOR_WARN_LOW;
                XftColor wc;
                make_xft_color(rgb, 1.0f, &wc);
                text_center(xft_font_hint, &wc, cx, msg_y + 22, warn);
                free_xft_color(&wc);
            }
        } else if (caps_lock_on()) {
            XftColor cl;
            make_xft_color(COLOR_WARN_LOW, 1.0f, &cl);
            text_center(xft_font_hint, &cl, cx, msg_y, "Caps Lock is on");
            free_xft_color(&cl);
        }
    }

    free_xft_color(&col);
    free_xft_color(&dim);
    XCopyArea(dpy, buf, win, gc, 0, 0, scr_w, scr_h, 0, 0);
    XFlush(dpy);
}

static void shake_field(void) {
    static const int off[] = { -12, 12, -8, 8, -5, 5, -2, 2, 0 };
    for (size_t i = 0; i < sizeof off / sizeof *off; i++) {
        shake_dx = off[i];
        draw_lock_screen();
        usleep(18000);
    }
    shake_dx = 0;
}

/* ── X11 initialisation ──────────────────────────────────────────────────── */

static void grab_input(void) {
    int i;
    for (i = 0; i < 100; i++) {
        if (XGrabKeyboard(dpy, win, False, GrabModeAsync, GrabModeAsync,
                          CurrentTime) == GrabSuccess)
            break;
        usleep(20000);
    }
    if (i == 100) die("slock: cannot grab keyboard\n");

    for (i = 0; i < 100; i++) {
        if (XGrabPointer(dpy, win, False, ButtonPressMask, GrabModeAsync,
                         GrabModeAsync, None, None, CurrentTime) == GrabSuccess)
            break;
        usleep(20000);
    }
    if (i == 100) die("slock: cannot grab pointer\n");
}

static void init_x(void) {
    dpy = XOpenDisplay(NULL);
    if (!dpy) die("slock: cannot open display\n");

    screen = DefaultScreen(dpy);
    root   = RootWindow(dpy, screen);
    scr_w  = DisplayWidth(dpy, screen);
    scr_h  = DisplayHeight(dpy, screen);
    get_monitor();

    /* Capture, blur and dim BEFORE the lock window exists. */
    XImage *background = capture_screen(dpy, root);
    if (!background) die("slock: cannot capture screen\n");
    apply_blur(background, BLUR_SCALE, BLUR_RADIUS);
    apply_dim(background, DIM_AMOUNT, VIGNETTE);

    XSetWindowAttributes wa;
    wa.override_redirect = 1;
    wa.background_pixel  = COLOR_BACKGROUND;
    win = XCreateWindow(dpy, root, 0, 0, scr_w, scr_h, 0,
                        DefaultDepth(dpy, screen), CopyFromParent,
                        DefaultVisual(dpy, screen),
                        CWOverrideRedirect | CWBackPixel, &wa);

    bg_pixmap = XCreatePixmap(dpy, win, scr_w, scr_h, DefaultDepth(dpy, screen));
    buf       = XCreatePixmap(dpy, win, scr_w, scr_h, DefaultDepth(dpy, screen));
    gc        = XCreateGC(dpy, win, 0, NULL);
    XPutImage(dpy, bg_pixmap, gc, background, 0, 0, 0, 0,
              background->width, background->height);
    XDestroyImage(background);

    /* The blurred desktop is the window background, so there is no black
     * flash between mapping and the first draw. */
    XSetWindowBackgroundPixmap(dpy, win, bg_pixmap);

    XSelectInput(dpy, win, KeyPressMask | ExposureMask | ButtonPressMask);
    XMapRaised(dpy, win);
    grab_input();

    /* Fonts and sprites after the screen is already locked. */
    setup_font_family();
    xft_font      = open_font(FONT_SIZE, 0);
    xft_font_time = open_font(TIME_FONT_SIZE, 1);
    xft_font_date = open_font(DATE_FONT_SIZE, 0);
    xft_font_hint = open_font(HINT_FONT_SIZE, 0);
    xft_font_ampm = open_font(AMPM_FONT_SIZE, 0);
    if (!xft_font || !xft_font_time || !xft_font_date || !xft_font_hint ||
        !xft_font_ampm)
        die("slock: cannot load fonts\n");

    xft_draw = XftDrawCreate(dpy, buf, DefaultVisual(dpy, screen),
                             DefaultColormap(dpy, screen));
    if (!xft_draw) die("slock: cannot create XftDraw\n");

    init_sprites();
}

/* ── Display-manager restart (max-attempts fallback) ─────────────────────── */

/* Run a program with an empty environment; returns its exit status or -1. */
static int run(const char *path, char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        char *envp[] = { NULL };
        execve(path, argv, envp);
        _exit(127);
    }
    int status;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static int try_restart(const char *ctl, const char *service) {
    char *probe[]   = { (char *)ctl, "is-active", "--quiet", (char *)service, NULL };
    char *restart[] = { (char *)ctl, "restart", (char *)service, NULL };
    if (run(ctl, probe) != 0) return 0;
    run(ctl, restart);
    return 1;
}

static void restart_display_manager(void) {
    const char *ctl[] = { "/usr/bin/systemctl", "/bin/systemctl", NULL };
    const char *svc[] = { "display-manager.service", "sddm.service",
                          "lightdm.service", "gdm.service", "xdm.service", NULL };

    for (int i = 0; ctl[i]; i++) {
        if (access(ctl[i], X_OK) != 0) continue;
        for (int j = 0; svc[j]; j++)
            if (try_restart(ctl[i], svc[j])) return;
    }

    fprintf(stderr, "slock: no display manager found, falling back to TTY1\n");
    char *chvt[] = { "/usr/bin/chvt", "1", NULL };
    run("/usr/bin/chvt", chvt);
}

/* ── Entry point ─────────────────────────────────────────────────────────── */

static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

int main(int argc, char **argv) {
    saved_euid = geteuid();

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    if (argc == 2 && strcmp(argv[1], "-v") == 0) {
        printf("slock-%s\n", VERSION);
        return 0;
    } else if (argc == 2 && strcmp(argv[1], "-t") == 0) {
        test_mode = 1;
    } else if (argc != 1) {
        die("usage: slock [-v|-t]\n");
    }

    /* Identity comes from the passwd database, never from the environment. */
    struct passwd *pw = getpwuid(getuid());
    if (!pw) die("slock: getpwuid failed\n");
    char user[128];
    snprintf(user, sizeof user, "%s", pw->pw_name);
    snprintf(user_home, sizeof user_home, "%s", pw->pw_dir ? pw->pw_dir : "");
    snprintf(display_name, sizeof display_name, "%s",
             (pw->pw_gecos && *pw->pw_gecos) ? pw->pw_gecos : pw->pw_name);
    char *comma = strchr(display_name, ',');
    if (comma) *comma = '\0';
    if (!display_name[0]) snprintf(display_name, sizeof display_name, "%s", user);

    init_x();
    draw_lock_screen();

    XEvent ev;
    KeySym ksym;

    while (running) {
        /* poll() with a 1 s timeout keeps the clock ticking. Drain Xlib's
         * internal queue first: events can be buffered without the socket
         * becoming readable. */
        if (!XPending(dpy)) {
            struct pollfd pfd = { .fd = ConnectionNumber(dpy), .events = POLLIN };
            int r = poll(&pfd, 1, 1000);
            if (r < 0 && errno == EINTR) continue;
            if (r == 0) { draw_lock_screen(); continue; }
        }

        XNextEvent(dpy, &ev);

        if (ev.type == KeyPress) {
            char kb[32];
            int num = XLookupString(&ev.xkey, kb, sizeof kb, &ksym, NULL);

            if (IsModifierKey(ksym)) {
                /* Caps Lock changes the warning line */
                if (ksym == XK_Caps_Lock || ksym == XK_Shift_Lock)
                    draw_lock_screen();
                continue;
            }

            /* test mode: Esc always quits, q quits when the field is empty */
            if (test_mode && (ksym == XK_Escape || (ksym == XK_q && pwlen == 0))) {
                running = 0;
                continue;
            }

            int is_enter = (ksym == XK_Return || ksym == XK_KP_Enter);

            /* IDLE → ACTIVE. Enter/Escape only open the form; any other key
             * opens it and is then processed so the first character is kept. */
            if (session_state == SESSION_IDLE) {
                session_state = SESSION_ACTIVE;
                if (is_enter || ksym == XK_Escape) {
                    draw_lock_screen();
                    continue;
                }
            }

            if (auth_state == STATE_WRONG) auth_state = STATE_NORMAL;

            if (is_enter) {
                passwd[pwlen] = '\0';
                if (verify_password(passwd, user)) {
                    auth_state = STATE_CORRECT;
                    pwlen = 0;
                    memset(passwd, 0, sizeof passwd);
                    draw_lock_screen();
                    sleep(1);
                    running = 0;
                } else {
                    auth_state = STATE_WRONG;
                    failed_attempts++;
                    pwlen = 0;
                    memset(passwd, 0, sizeof passwd);
                    shake_field();
                    draw_lock_screen();
                    if (failed_attempts >= MAX_ATTEMPTS) {
                        sleep(2);
                        running = 0;
                        if (!test_mode) restart_display_manager();
                    }
                }
                continue;
            }

            if (IsFunctionKey(ksym) || IsMiscFunctionKey(ksym) ||
                IsKeypadKey(ksym)   || IsPFKey(ksym))
                continue;

            if (ksym == XK_Escape) {
                pwlen = 0;
                memset(passwd, 0, sizeof passwd);
            } else if (ksym == XK_BackSpace || ksym == XK_Delete) {
                /* remove one whole UTF-8 character */
                while (pwlen > 0 && ((unsigned char)passwd[pwlen - 1] & 0xC0) == 0x80)
                    passwd[--pwlen] = '\0';
                if (pwlen > 0) passwd[--pwlen] = '\0';
            } else if (num > 0 && !iscntrl((unsigned char)kb[0]) &&
                       pwlen + num < (int)sizeof passwd) {
                memcpy(passwd + pwlen, kb, (size_t)num);
                pwlen += num;
            }
            draw_lock_screen();

        } else if (ev.type == ButtonPress) {
            if (session_state == SESSION_IDLE) {
                session_state = SESSION_ACTIVE;
                draw_lock_screen();
            }

        } else if (ev.type == Expose) {
            if (ev.xexpose.count == 0)
                draw_lock_screen();
        }
    }

    memset(passwd, 0, sizeof passwd);
    int exit_code = (failed_attempts >= MAX_ATTEMPTS && !test_mode) ? 1 : 0;
    cleanup();
    return exit_code;
}
