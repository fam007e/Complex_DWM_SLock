#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xft/Xft.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <security/pam_appl.h>
#include <sys/types.h>
#include <pwd.h>
#include <ctype.h>
#include "config.h"
#include "blur.h"

static Display *dpy;
static int screen;
static Window root, win;
static Pixmap pmap;
static GC gc;
static XftFont *xft_font;
static XftDraw *xft_draw;
static XftColor xft_color;

static int failed_attempts = 0;

static int conv(int num_msg, const struct pam_message **msg,
                struct pam_response **resp, void *appdata_ptr) {
    // Implement PAM conversation function
    // This is a simplified version and needs to be expanded
    return PAM_SUCCESS;
}

static int verify_password(const char *password) {
    // Implement password verification using PAM
    // This is a simplified version and needs to be expanded
    return 1; // Return 1 for success, 0 for failure
}

static void die(const char *errstr, ...) {
    va_list ap;
    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    exit(1);
}

static void init_x() {
    dpy = XOpenDisplay(NULL);
    if (!dpy)
        die("Cannot open display\n");
    
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    
    XSetWindowAttributes wa;
    wa.override_redirect = 1;
    wa.background_pixel = COLOR_BACKGROUND;
    
    win = XCreateWindow(dpy, root, 0, 0, DisplayWidth(dpy, screen),
                        DisplayHeight(dpy, screen), 0, DefaultDepth(dpy, screen),
                        CopyFromParent, DefaultVisual(dpy, screen),
                        CWOverrideRedirect | CWBackPixel, &wa);
    
    XMapRaised(dpy, win);
    
    gc = XCreateGC(dpy, win, 0, NULL);
    
    // Initialize Xft
    xft_font = XftFontOpenName(dpy, screen, FONT);
    if (!xft_font)
        die("Cannot load font\n");
    
    xft_draw = XftDrawCreate(dpy, win, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));
    
    XRenderColor xrender_color = {
        .red = COLOR_FOREGROUND >> 16 & 0xff00,
        .green = COLOR_FOREGROUND >> 8 & 0xff00,
        .blue = COLOR_FOREGROUND & 0xff00,
        .alpha = 0xffff
    };
    XftColorAllocValue(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen),
                       &xrender_color, &xft_color);
}

static void draw_text(const char *text, int x, int y) {
    XftDrawStringUtf8(xft_draw, &xft_color, xft_font, x, y, (XftChar8 *)text, strlen(text));
}

static void draw_lock_screen() {
    XImage *background = capture_screen(dpy, root);
    apply_blur(background, BLUR_RADIUS);
    
    XPutImage(dpy, win, gc, background, 0, 0, 0, 0, background->width, background->height);
    XDestroyImage(background);
    
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char time_str[64];
    strftime(time_str, sizeof(time_str), TIME_FORMAT, tm);
    
    draw_text(time_str, TIME_X, TIME_Y);
    draw_text(USERNAME, USERNAME_X, USERNAME_Y);
    draw_text("Enter password:", PASSWORD_PROMPT_X, PASSWORD_PROMPT_Y);
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "-v") == 0) {
        printf("slock-"VERSION"\n");
        return 0;
    } else if (argc != 1) {
        die("usage: slock [-v]\n");
    }
    
    init_x();
    
    char passwd[256];
    int len = 0;
    XEvent ev;
    KeySym ksym;
    
    while (1) {
        draw_lock_screen();
        XNextEvent(dpy, &ev);
        if (ev.type == KeyPress) {
            char buf[32];
            int num = XLookupString(&ev.xkey, buf, sizeof buf, &ksym, 0);
            if (ksym == XK_Return) {
                passwd[len] = '\0';
                if (verify_password(passwd)) {
                    return 0;
                } else {
                    failed_attempts++;
                    if (failed_attempts >= MAX_ATTEMPTS) {
                        system("systemctl restart sddm.service");
                        return 1;
                    }
                    len = 0;
                }
            } else if (ksym == XK_Escape) {
                len = 0;
            } else if (ksym == XK_BackSpace) {
                if (len > 0)
                    len--;
            } else if (num && !iscntrl((int)buf[0]) && len < sizeof(passwd) - 1) {
                memcpy(passwd + len, buf, num);
                len += num;
            }
        }
    }
    
    return 0;
}