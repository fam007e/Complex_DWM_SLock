#ifndef BLUR_H
#define BLUR_H

#include <X11/Xlib.h>

// Apply blur effect to the given image
void apply_blur(XImage *image, int radius);

// Capture the current screen content
XImage *capture_screen(Display *dpy, Window root);

#endif // BLUR_H