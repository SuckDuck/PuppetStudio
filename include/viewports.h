#ifndef VIEWPORTS_H
#define VIEWPORTS_H
#include <raylib.h>
#include "microui.h"

typedef struct UIPanel{
    int size;
    bool resizable;
} UIPanel;

typedef struct Viewport{
    char* title;
    Rectangle size;
    Vector2 pos;
    mu_Context ctx;
    RenderTexture renderTexture;
    Camera2D camera;
    float minZoom;
    float maxZoom;
    KeyboardKey shortcut;
    bool hidden;
    bool hideCmd;
    bool resizable;
    bool disableOffsetUpdate;
    bool alwaysOnTop;
    bool updateAlways;
    bool renderAlways;
    UIPanel rightPanel;
    UIPanel leftPanel;
    UIPanel topPanel;
    UIPanel bottomPanel;
    void (*Init)(struct Viewport*);
    void (*Update)(struct Viewport*);
    void (*OnResize)(struct Viewport*);
    const char** (*GetCmds)();
    void (*ExecCmd)(struct Viewport*, int argc, char **argv);
    void (*RightPanel)(struct Viewport*, mu_Context*);
    void (*LeftPanel)(struct Viewport*, mu_Context*);
    void (*TopPanel)(struct Viewport*, mu_Context*);
    void (*BottomPanel)(struct Viewport*, mu_Context*);
    void (*RenderUnderlay)(struct Viewport*);
    void (*Render)(struct Viewport*);
    void (*RenderOverlay)(struct Viewport*);
    struct Viewport *prev;
    struct Viewport *next;
} Viewport;

typedef struct ViewportLinkedList{
    Viewport *head;
    Viewport *tail;
} ViewportLinkedList;

extern ViewportLinkedList viewports;
extern Viewport *maximizedViewport;
extern bool viewportJustSwitched;
extern Viewport *viewportsToShow[5];

void InitViewport(
    char* title, 
    float w, float h, 
    float x, float y, 
    float minZoom, float maxZoom, 
    KeyboardKey shortcut, 
    bool hideCmd,
    bool resizable,
    bool alwaysOnTop,
    void (*Init)(Viewport*), 
    void (*Update)(Viewport*), 
    void (*OnResize)(struct Viewport*),
    const char** (GetCmds)(),
    void (*ExecCmd)(struct Viewport*, int argc, char **argv), 
    void (*RightPanel)(Viewport*, mu_Context*), 
    void (*LeftPanel)(Viewport*, mu_Context*), 
    void (*TopPanel)(Viewport*, mu_Context*), 
    void (*BottomPanel)(Viewport*, mu_Context*), 
    void (*RenderUnderlay)(Viewport*),
    void (*Render)(Viewport*), 
    void (*RenderOverlay)(Viewport*) );

void CleanUpViewportsPool();
void RenderViewportToScreen(Viewport *v);
void RenderViewport(Viewport *v);
void UpdateViewportUIInput(Viewport *v);
void CleanViewportUIInput(Viewport *v);
void ProcessViewportUI(Viewport *v);
void SetViewportPanelsDimensions(Viewport *v, int leftPanel, int rightPanel, int topPanel, int bottomPanel);
void ResizeViewport(Viewport *v, int w, int h);
bool ResizeViewportW(Viewport *v, int w);
bool ResizeViewportH(Viewport *v, int h);
void DrawGridTexture(Viewport *v);
void DrawViewport(Viewport* v);
void DrawViewportUI(Viewport *v);
void DrawViewportShadow(Viewport *v);
void RestoreViewport();
void MaximizeViewport(Viewport *v);
void SetViewportOnTop(Viewport* v);
bool IsMouseInsideViewport(Viewport* v, bool titleBar, bool resizeHandle, bool onlyViewport);
int RegisterViewports();

Vector2 GetMouseOverlayPosition(Viewport* v);
Vector2 GetMouseViewportPosition(Viewport* v);
bool IsMouseButtonPressedFocusSafe(int button);
bool IsMouseButtonReleasedFocusSafe(int button);
void ViewportUpdateZoom(Viewport* v);
int ViewportUpdatePan(Viewport* v);
void ToggleViewport(Viewport *v);
void ToggleViewportByName(char *name);
void OpenViewportByName(char *name);
void CloseViewportByName(char *name);
void GetPanelsHandles(Viewport *v, Rectangle *L, Rectangle *R, Rectangle *T, Rectangle *B);

//filedialog.c
void FileDialogInit(Viewport *v);
void FileDialogUpdate(Viewport *v);
const char **FileDialogGetCommands();
void FileDialogExecCmd(Viewport *v, int argc, char **argv);
void FileDialogRender(Viewport *v);
void FileDialogRenderOverlay(Viewport *v);

//storagebridge.c
void StorageBridgeInit(Viewport *v);
const char **StorageBridgeGetCommands();
void StorageBridgeExecCmd(Viewport *v, int argc, char **argv);
void StorageBridgeLeftPanel(Viewport *v, mu_Context *ctx);

//workshop.c
void WorkShopInit(Viewport *v);
void WorkShopUpdate(Viewport *v);
const char **WorkShopGetCommands();
void WorkShopExecCmd(Viewport *v, int argc, char **argv);
void WorkShopRightPanel(Viewport *v, mu_Context *ctx);
void WorkShopRenderUnderlay(Viewport *v);
void WorkShopRender(Viewport *v);
void WorkShopRenderOverlay(Viewport *v);

//closet.c
void ClosetInit(Viewport *v);
void ClosetUpdate(Viewport *v);
const char **ClosetGetCommands();
void ClosetExecCmd(Viewport *v, int argc, char **argv);
void ClosetRightPanel(Viewport *v, mu_Context *ctx);
void ClosetTopPanel(Viewport *v, mu_Context *ctx);
void ClosetRender(Viewport *v);
void ClosetRenderOverlay(Viewport *v);

//regionpresets.c
void RPInit(Viewport *v);
void RPUpdate(Viewport *v);
const char** RPGetCmds();
void RPExecCmd(Viewport *v, int argc, char **argv);
void RPTopPanel(Viewport *v, mu_Context *ctx);
void RPRenderUnderlay(Viewport *v);
void RPRender(Viewport *v);
void RPRenderOverlay(Viewport *v);

//theatre.c
void TheatreInit(Viewport *v);
void TheatreUpdate(Viewport *v);
void TheatreOnResize(Viewport *v);
const char **TheatreGetCommands();
void TheatreExecCmd(Viewport *v, int argc, char **argv);
void TheatreLeftPanel(Viewport *v, mu_Context *ctx);
void TheatreRightPanel(Viewport *v, mu_Context *ctx);
void TheatreBottomPanel(Viewport *v, mu_Context *ctx);
void TheatreRenderUnderlay(Viewport *v);
void TheatreRender(Viewport *v);
void TheatreRenderOverlay(Viewport *v);

//welcome.c
void WelcomeInit(Viewport *v);
const char **WelcomeGetCommands();
void WelcomeExecCmd(Viewport *v, int argc, char **argv);
void WelcomeLeftPanel(Viewport *v, mu_Context *ctx);

#endif