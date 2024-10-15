#ifndef CONFIG_H
#define CONFIG_H

#ifndef VERSION
#define VERSION TOSTRING(BUILD_VERSION)
#endif

#define USERNAME "yourusername"
#define MAX_ATTEMPTS 10
#define BLUR_RADIUS 10

#define FONT "assets/fonts/font.ttf"
#define FONT_SIZE 12
#define BACKGROUND_IMAGE "assets/images/background.jpg"

#define TIME_FORMAT "%H:%M:%S"
#define TIME_X 20
#define TIME_Y 50

#define USERNAME_X 20
#define USERNAME_Y 100

#define PASSWORD_PROMPT_X 20
#define PASSWORD_PROMPT_Y 150

#define MESSAGE_X 20
#define MESSAGE_Y 200

#define COLOR_BACKGROUND 0x000000  // Black
#define COLOR_FOREGROUND 0xFFFFFF  // White
#define COLOR_FAILURE 0xFF0000     // Red

#endif // CONFIG_H