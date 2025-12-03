#ifndef UTILS_H
#define UTILS_H
#include <stdint.h>
#include "raylib.h"
#include "viewports.h"
#include <stddef.h>

void OpenExplorer(char *out, int len);
float VectorToDegrees(Vector2 v);
Vector2 DegreesToVector(float d);
bool IsPointOnRect(Vector2 p, Rectangle r);
bool IsPointOnRectBorder(Vector2 p, Rectangle r, bool center, float lineThickness);
bool IsPointOnCircle(Vector2 p, Vector2 center, float radius);
void DrawTextCustom(Vector2 pos, int q, ...);
void DrawTextCustom2(Vector2 pos, char *format, ...);
void DrawTextInCorner(Viewport *v, Color rectColor, char *format, ...);
void DrawTextInCenter(float titleSize, float otherSize, float w, float h, char **t);
int text_width(mu_Font font, int font_size, const char *str, int len);
int text_height(mu_Font font, int font_size);
char* ToLower(char* s);
char* ToUpper(char* s);
void PushLog(char *format, ...);
void ChangeCursor(MouseCursor c);
unsigned long djb2Hash(const unsigned char *data, size_t len);
int MuNumberORNa(mu_Context *ctx, char *label, float *value, bool condition, bool space);
void GetRectCorners(Rectangle rect, Vector2 center, float zoom, float rotation, Vector2 *c0, Vector2 *c1, Vector2 *c2, Vector2 *c3);
float AngleBetweenVectors(Vector2 a, Vector2 b);
Vector2 Vector2Single(float v);
RenderTexture2D LoadCustomRenderTexture(int width, int height);
int RemoveDir(char *path);
Color InvertColor(Color color);

#endif