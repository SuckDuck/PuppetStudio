#ifndef CONFIG_H
#define CONFIG_H
#include <raylib.h>

#define PATH_MAX                4096
#define BG_C                    CLITERAL(Color){ 40, 40, 40, 255 }
#define TEXT_C                  LIGHTGRAY
#define LOGS_C                  CLITERAL(Color){ 255, 255, 0, 255 }
#define VIEWPORT_BG_C           CLITERAL(Color){ 55, 55, 55, 255 }
#define VIEWPORT_GRID_C         CLITERAL(Color){ 65, 65, 65, 255 }
#define VIEWPORT_OUTLINE_C      BG_C
#define VIEWPORT_TITLE_C        BLUE
#define CMD_BAR_C               CLITERAL(Color){ 30, 30, 30, 255 }
#define CMD_BAR_VIEWPORT_C      CLITERAL(Color){ 35, 60, 90, 255 }
#define CMD_BAR_COMMAND_C       CLITERAL(Color){ 35, 80, 70, 255 }
#define FILE_HOVER_C            CLITERAL(Color){ 70, 90, 110, 255 }
#define VIEWPORT_TITLE_H        20
#define VIEWPORT_CORNER_S       12
#define VIEWPORT_PANEL_HANDLE_S 5
#define VIEWPORT_OUTLINE_T      1
#define DEFAULT_FONT_SIZE       13
#define TITLE_FONT_SIZE         VIEWPORT_TITLE_H
#define MODKEY                  KEY_LEFT_SHIFT
#define COMMAND_BAR_KEY         KEY_ENTER
#define CMD_BUF_S               256
#define HINTS_BUF_S             256
#define ZOOM_SPEED              0.1f
#define SCROLL_SPEED            8
#define LOGS_LINES              256
#define LOGS_SCREEN_TIME        4
#define MIN_PANEL_SIZE          20
#define ICONS_Q                 7

#ifndef RESIZE_CURSOR
#define RESIZE_CURSOR MOUSE_CURSOR_RESIZE_ALL
#endif

#ifndef PROJECT_TITLE
#define PROJECT_TITLE ""
#endif

#ifndef PROJECT_VERSION
#define PROJECT_VERSION 000
#endif

extern Font inconsolata;
extern Texture2D gridTexture;
extern Texture2D shadowTexture;
extern MouseCursor currentCursor, nextCursor;

#endif