#ifndef CONFIG_H
#define CONFIG_H

// Macro for stringification
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Version handling
#ifndef BUILD_VERSION
#define BUILD_VERSION unknown
#endif

#define VERSION TOSTRING(BUILD_VERSION)

// User configuration
#define USERNAME "yourusername"
#define MAX_ATTEMPTS 10
#define BLUR_RADIUS 10

// Font and background settings
#define FONT "assets/fonts/font.ttf"
#define FONT_SIZE 12
#define BACKGROUND_IMAGE "assets/images/background.jpg"

// Time display settings
#define TIME_FORMAT "%H:%M:%S"
#define TIME_X 20
#define TIME_Y 50

// Username display settings
#define USERNAME_X 20
#define USERNAME_Y 100

// Password prompt settings
#define PASSWORD_PROMPT_X 20
#define PASSWORD_PROMPT_Y 150

// Message display settings
#define MESSAGE_X 20
#define MESSAGE_Y 200

// Color definitions
#define COLOR_BACKGROUND 0x000000  // Black
#define COLOR_FOREGROUND 0xFFFFFF  // White
#define COLOR_FAILURE 0xFF0000     // Red

#endif // CONFIG_H