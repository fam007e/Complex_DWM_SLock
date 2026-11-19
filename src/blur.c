#include "blur.h"
#include <X11/Xutil.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * All routines need 32 bits per pixel. The blur and dim treat the three
 * colour channels identically, so RGB vs BGR channel order doesn't matter.
 */

static inline int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Sliding-window box blur of one line (O(1) per pixel, edges clamped). */
static void blur_line(const uint32_t *in, uint32_t *out, int n,
                      ptrdiff_t step, int r) {
    int div = 2 * r + 1;
    unsigned sr = 0, sg = 0, sb = 0;

    for (int i = -r; i <= r; i++) {
        uint32_t p = in[clampi(i, 0, n - 1) * step];
        sr += (p >> 16) & 255; sg += (p >> 8) & 255; sb += p & 255;
    }
    for (int x = 0; x < n; x++) {
        out[x * step] = ((sr / div) << 16) | ((sg / div) << 8) | (sb / div);
        uint32_t pa = in[clampi(x + r + 1, 0, n - 1) * step];
        uint32_t pb = in[clampi(x - r,     0, n - 1) * step];
        sr += ((pa >> 16) & 255) - ((pb >> 16) & 255);
        sg += ((pa >>  8) & 255) - ((pb >>  8) & 255);
        sb += ( pa        & 255) - ( pb        & 255);
    }
}

/* Three H+V box passes ≈ gaussian. Buffer is w*h with the given stride. */
static void blur_buf(uint32_t *buf, int w, int h, int stride, int r) {
    uint32_t *tmp = malloc((size_t)stride * h * sizeof *tmp);
    if (!tmp) return;
    for (int pass = 0; pass < 3; pass++) {
        for (int y = 0; y < h; y++)
            blur_line(buf + (size_t)y * stride, tmp + (size_t)y * stride, w, 1, r);
        for (int x = 0; x < w; x++)
            blur_line(tmp + x, buf + x, h, stride, r);
    }
    free(tmp);
}

static inline uint32_t lerp4(uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                             float tx, float ty) {
    uint32_t out = 0;
    for (int s = 0; s <= 16; s += 8) {
        float top = ((a >> s) & 255) * (1 - tx) + ((b >> s) & 255) * tx;
        float bot = ((c >> s) & 255) * (1 - tx) + ((d >> s) & 255) * tx;
        out |= (uint32_t)(top * (1 - ty) + bot * ty + 0.5f) << s;
    }
    return out;
}

void apply_blur(XImage *img, int scale, int radius) {
    if (!img || img->bits_per_pixel != 32 || scale < 1 || radius < 1) return;

    const int W = img->width, H = img->height;
    const int S = img->bytes_per_line / 4;
    const int w = W / scale, h = H / scale;
    if (w < 2 || h < 2) return;

    uint32_t *big = (uint32_t *)img->data;
    uint32_t *sm  = malloc((size_t)w * h * sizeof *sm);
    int      *xi0 = malloc((size_t)W * sizeof *xi0);
    int      *xi1 = malloc((size_t)W * sizeof *xi1);
    float    *xtx = malloc((size_t)W * sizeof *xtx);
    if (!sm || !xi0 || !xi1 || !xtx) goto out;

    /* 1. box-downscale */
    const unsigned n = (unsigned)scale * scale;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            unsigned r = 0, g = 0, b = 0;
            for (int sy = 0; sy < scale; sy++) {
                const uint32_t *row = big + (size_t)(y * scale + sy) * S + x * scale;
                for (int sx = 0; sx < scale; sx++) {
                    uint32_t p = row[sx];
                    r += (p >> 16) & 255; g += (p >> 8) & 255; b += p & 255;
                }
            }
            sm[y * w + x] = ((r / n) << 16) | ((g / n) << 8) | (b / n);
        }
    }

    /* 2. blur the small copy */
    blur_buf(sm, w, h, w, radius);

    /* 3. bilinear upscale back into the XImage */
    for (int x = 0; x < W; x++) {
        float fx = (x + 0.5f) / scale - 0.5f;
        int x0 = (int)floorf(fx);
        xtx[x] = fx - x0;
        xi0[x] = clampi(x0,     0, w - 1);
        xi1[x] = clampi(x0 + 1, 0, w - 1);
    }
    for (int y = 0; y < H; y++) {
        float fy = (y + 0.5f) / scale - 0.5f;
        int y0 = (int)floorf(fy);
        float ty = fy - y0;
        const uint32_t *r0 = sm + (size_t)clampi(y0,     0, h - 1) * w;
        const uint32_t *r1 = sm + (size_t)clampi(y0 + 1, 0, h - 1) * w;
        uint32_t *dst = big + (size_t)y * S;
        for (int x = 0; x < W; x++)
            dst[x] = lerp4(r0[xi0[x]], r0[xi1[x]], r1[xi0[x]], r1[xi1[x]],
                           xtx[x], ty);
    }

out:
    free(sm); free(xi0); free(xi1); free(xtx);
}

void apply_dim(XImage *img, float dim, float vignette) {
    if (!img || img->bits_per_pixel != 32) return;

    const int w = img->width, h = img->height, S = img->bytes_per_line / 4;
    uint32_t *px = (uint32_t *)img->data;

    for (int y = 0; y < h; y++) {
        float ny = ((y + 0.5f) / h - 0.5f) * 2.0f;
        for (int x = 0; x < w; x++) {
            float nx = ((x + 0.5f) / w - 0.5f) * 2.0f;
            float d2 = (nx * nx + ny * ny) * 0.5f;      /* 0 centre → 1 corner */
            float f = 1.0f - dim - vignette * d2;
            if (f < 0.0f) f = 0.0f;
            uint32_t p = px[(size_t)y * S + x];
            px[(size_t)y * S + x] =
                  ((uint32_t)(((p >> 16) & 255) * f) << 16)
                | ((uint32_t)(((p >>  8) & 255) * f) <<  8)
                |  (uint32_t)(( p        & 255) * f);
        }
    }
}

XImage *capture_screen(Display *dpy, Window root) {
    XWindowAttributes gwa;
    if (!XGetWindowAttributes(dpy, root, &gwa)) return NULL;
    return XGetImage(dpy, root, 0, 0, gwa.width, gwa.height, AllPlanes, ZPixmap);
}
