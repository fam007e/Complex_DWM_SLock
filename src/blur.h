#ifndef BLUR_H
#define BLUR_H

#include <X11/Xlib.h>

/* Blur in place. The image is shrunk by `scale`, blurred with `radius`
 * (3-pass box ≈ gaussian) and bilinearly scaled back up. */
void apply_blur(XImage *image, int scale, int radius);

/* Uniform darkening plus a vignette towards the corners. */
void apply_dim(XImage *image, float dim, float vignette);

/* Capture the whole root window. */
XImage *capture_screen(Display *dpy, Window root);

#endif /* BLUR_H */
