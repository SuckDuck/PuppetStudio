#include <dirent.h>
#include <fcntl.h>
#include <raylib.h>
#include <rlgl.h>
#include <raymath.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "config.h"
#include "viewports.h"
#include "microui.h"

Vector2 DegreesToVector(float d){
    float radians = d*(M_PI/180);
    return (Vector2){cos(radians),sin(radians)};
}

float VectorToDegrees(Vector2 v) {
    return atan2(v.y, v.x) * (180.0f / M_PI);
}

Vector2 Vector2RotateDegrees(Vector2 v, float angle){
    float rad = angle * (M_PI / 180.0f);
    float cosA = cos(rad);
    float sinA = sin(rad);
    return (Vector2){
        v.x * cosA - v.y * sinA,
        v.x * sinA + v.y * cosA
    };
}

bool IsPointOnRect(Vector2 p, Rectangle r){
    return p.x > r.x && p.x < r.x+r.width && p.y > r.y && p.y < r.y+r.height;
}

bool IsPointOnRectBorder(Vector2 p, Rectangle r, bool center, float lineThickness){
    if (center){
        r.x -= r.width/2;
        r.y -= r.height/2;
    }
    
    float halfT = lineThickness/2;
    if (IsPointOnRect(p, (Rectangle){r.x-halfT,r.y,lineThickness,r.height})) return true;
    if (IsPointOnRect(p, (Rectangle){r.x,r.y-halfT,r.width,lineThickness}))  return true;
    if (IsPointOnRect(p, (Rectangle){r.x,r.y+r.height-halfT,r.width,lineThickness})) return true;
    if (IsPointOnRect(p, (Rectangle){r.x+r.width-halfT,r.y,lineThickness,r.height})) return true;
    return false;
}

bool IsPointOnCircle(Vector2 p, Vector2 center, float radius){
    return p.x > center.x-radius && p.x < center.x+radius && p.y > center.y-radius && p.y < center.y+radius;
}

void DrawTextCustom(Vector2 pos, int q, ...){
    va_list args;
    float offset = 0;
    float spaceW = MeasureTextEx(inconsolata," ",DEFAULT_FONT_SIZE,2).x;
    va_start(args, q);
    for (int i = 0; i < q; i++) {
        char *word = va_arg(args, char*);
        DrawTextEx(
            inconsolata, 
            word, 
            (Vector2){pos.x+offset,pos.y}, 
            DEFAULT_FONT_SIZE, 
            2, 
            TEXT_C);
        
        offset += MeasureTextEx(inconsolata,word,DEFAULT_FONT_SIZE,2).x + spaceW;
    }
    
    va_end(args);
}

void DrawTextCustom2(Vector2 pos, char *format, ...){
    va_list args;
    va_start(args, format);
    char text[256] = {0};
    vsnprintf(text, 256, format, args);
    DrawTextEx(
        inconsolata, 
        text, 
        pos, 
        DEFAULT_FONT_SIZE, 
        2, 
        TEXT_C
    );
    va_end(args);
}

void DrawTextInCorner(Viewport *v, Color rectColor, char *format, ...){
    va_list args;
    va_start(args, format);
    char text[256] = {0};
    vsnprintf(text, 256, format, args);
    Vector2 textSize = MeasureTextEx(inconsolata, text, DEFAULT_FONT_SIZE, 2);
    Vector2 pos = (Vector2){v->size.width-textSize.x-5,(v->size.height*-1)-textSize.y};
    DrawRectangle(pos.x, pos.y, textSize.x, textSize.y, rectColor);
    DrawTextEx(
        inconsolata, 
        text, 
        pos, 
        DEFAULT_FONT_SIZE, 
        2, 
        LOGS_C
    );
    va_end(args);

}

void DrawTextInCenter(float titleSize, float otherSize, float w, float h, char **t) {
    if (titleSize == -1) titleSize = TITLE_FONT_SIZE;
    if (titleSize < -1) titleSize = DEFAULT_FONT_SIZE;
    if (otherSize < 0) otherSize = DEFAULT_FONT_SIZE;
    
    float lineSpacing = 5;
    Vector2 center = { w / 2, h / 2 };

    // Calculates max width and max height
    float maxTextW = 0;
    float totalH = 0;
    for (char **s = t; *s != NULL; s++) {
        float fontSize = (s == t) ? titleSize : otherSize;
        Vector2 size = MeasureTextEx(inconsolata, *s, fontSize, 2);
        if (size.x > maxTextW) maxTextW = size.x;
        totalH += size.y + lineSpacing;
    }

    // Draw center text
    float y = center.y - totalH / 2;
    for (char **s = t; *s != NULL; s++) {
        float fontSize = (s == t) ? titleSize : otherSize;
        Vector2 size = MeasureTextEx(inconsolata, *s, fontSize, 2);
        float x = center.x - size.x / 2;
        DrawTextEx(inconsolata, *s, (Vector2){ x, y }, fontSize, 2, TEXT_C);
        y += size.y + lineSpacing;
    }
}

int text_width(mu_Font font, int font_size, const char *str, int len){
    return MeasureTextEx(*(Font*)font,str,font_size,2).x;
}

int text_height(mu_Font font, int font_size){
    return font_size;
}

char* ToLower(char* s) {
  for(char *p=s; *p; p++) *p=tolower(*p);
  return s;
}

char* ToUpper(char* s) {
  for(char *p=s; *p; p++) *p=toupper(*p);
  return s;
}

void ChangeCursor(MouseCursor c){
    nextCursor = c;
}

unsigned long djb2Hash(const unsigned char *data, size_t len){
    unsigned long h = 5381;
    for (size_t i = 0; i < len; ++i)
        h = ((h << 5) + h) + data[i]; // h * 33 + data[i]
    return h;
}

int MuNumberORNa(mu_Context *ctx, char *label, float *value, bool condition, bool space){
    int res = 0;
    if (space) mu_space(ctx);
    mu_label(ctx,label,ctx->style->control_font_size);
    if (condition){
        res = mu_number_ex(
            ctx,
            value,
            0,
            MU_SLIDER_FMT,
            ctx->style->control_font_size,
            1,
            MU_OPT_ALIGNCENTER
        );
    }
    else mu_textbox_ex(ctx, "n/a", 3, ctx->style->control_font_size, MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT);
    return res;
}

void GetRectCorners(Rectangle rect, Vector2 center, float zoom, float rotation, Vector2 *c0, Vector2 *c1, Vector2 *c2, Vector2 *c3){
    if (c0){
        *c0 = Vector2RotateDegrees((Vector2){
            center.x/zoom*-1,
            center.y/zoom*-1
        },rotation);
        *c0 = Vector2Add((Vector2){rect.x,rect.y}, *c0);
    }

    if (c1){
        *c1 = Vector2RotateDegrees((Vector2){
            (rect.width-center.x)/zoom,
            center.y/zoom*-1
        },rotation);
        *c1 = Vector2Add((Vector2){rect.x,rect.y}, *c1);
    }

    if (c2){
        *c2 = Vector2RotateDegrees((Vector2){
            (rect.width-center.x)/zoom,
            (rect.height-center.y)/zoom
        },rotation);
        *c2 = Vector2Add((Vector2){rect.x,rect.y}, *c2);
    }

    if (c3){
        *c3 = Vector2RotateDegrees((Vector2){
            center.x/zoom*-1,
            (rect.height-center.y)/zoom
        },rotation);
        *c3 = Vector2Add((Vector2){rect.x,rect.y}, *c3);
    }
}

float AngleBetweenVectors(Vector2 a, Vector2 b) {
    float dot = a.x*b.x + a.y*b.y;
    float det = a.x*b.y - a.y*b.x;
    float angle = atan2(det, dot);
    return angle * (180.0f / M_PI);
}

Vector2 Vector2Single(float v){
    return (Vector2){v,v};
}

RenderTexture2D LoadCustomRenderTexture(int width, int height){
    RenderTexture2D target = { 0 };

    target.id = rlLoadFramebuffer(); // Load an empty framebuffer

    if (target.id > 0)
    {
        rlEnableFramebuffer(target.id);

        // Create color texture (default to RGBA)
        target.texture.id = rlLoadTexture(NULL, width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8, 1);
        target.texture.width = width;
        target.texture.height = height;
        target.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
        target.texture.mipmaps = 1;

        // Create depth renderbuffer/texture
        target.depth.id = rlLoadTextureDepth(width, height, true);
        target.depth.width = width;
        target.depth.height = height;
        target.depth.format = 19;       //DEPTH_COMPONENT_24BIT?
        target.depth.mipmaps = 1;

        // Attach color texture and depth renderbuffer/texture to FBO
        rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
        rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);

        // Check if fbo is complete with attachments (valid)
        if (rlFramebufferComplete(target.id)) TRACELOG(LOG_INFO, "FBO: [ID %i] Framebuffer object created successfully", target.id);

        rlDisableFramebuffer();
    }
    else TRACELOG(LOG_WARNING, "FBO: Framebuffer object can not be created");

    return target;
}

int RemoveDir(char *path){
    DIR *dir = opendir(path);
    struct dirent *entry;
    char fullpath[1024];

    if (!dir) return -1;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                RemoveDir(fullpath);
            } else {
                unlink(fullpath);
            }
        }
    }

    closedir(dir);
    return rmdir(path);
}

Color InvertColor(Color color){
    return (Color){
        255 - color.r,
        255 - color.g,
        255 - color.b
    };
}