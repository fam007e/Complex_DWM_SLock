#include "blur.h"
#include <stdlib.h>

void apply_blur(XImage *image, int radius) {
    int width = image->width;
    int height = image->height;
    int *src = (int*)image->data;
    int *temp = malloc(width * height * sizeof(int));
    
    // Horizontal blur
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int r = 0, g = 0, b = 0, count = 0;
            for (int i = -radius; i <= radius; i++) {
                if (x + i >= 0 && x + i < width) {
                    int pixel = src[y * width + (x + i)];
                    r += (pixel >> 16) & 0xFF;
                    g += (pixel >> 8) & 0xFF;
                    b += pixel & 0xFF;
                    count++;
                }
            }
            temp[y * width + x] = ((r / count) << 16) | ((g / count) << 8) | (b / count);
        }
    }
    
    // Vertical blur
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            int r = 0, g = 0, b = 0, count = 0;
            for (int i = -radius; i <= radius; i++) {
                if (y + i >= 0 && y + i < height) {
                    int pixel = temp[(y + i) * width + x];
                    r += (pixel >> 16) & 0xFF;
                    g += (pixel >> 8) & 0xFF;
                    b += pixel & 0xFF;
                    count++;
                }
            }
            src[y * width + x] = ((r / count) << 16) | ((g / count) << 8) | (b / count);
        }
    }
    
    free(temp);
}

XImage *capture_screen(Display *dpy, Window root) {
    XWindowAttributes gwa;
    XGetWindowAttributes(dpy, root, &gwa);
    int width = gwa.width;
    int height = gwa.height;
    
    XImage *img = XGetImage(dpy, root, 0, 0, width, height, AllPlanes, ZPixmap);
    return img;
}