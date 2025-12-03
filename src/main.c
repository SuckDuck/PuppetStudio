#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <raylib.h>
#include <string.h>
#include <stdarg.h>
#include "microui.h"
#include "config.h"
#include "viewports.h"
#include "modules.h"
#include "Inconsolata-SemiBold.h"
#include "gridTexture.h"
#include "icons.h"
#include "shadow.h"
#include "utils.h"

typedef enum{
    IDLE,
    MOVING_VIEWPORT,
    RESIZING_VIEWPORT,
    RESIZING_PANEL,
    ON_COMMAND_BAR
} State;

typedef enum{
    LEFT_PANEL,
    RIGHT_PANEL,
    TOP_PANEL,
    BOTTOM_PANEL
} PanelType;

static State state;
static Viewport *targetViewport;
static int *targetPanelSize;
static PanelType targetPanelType;

static Vector2 grabOffset;

static char cmdBuf[CMD_BUF_S] = {0};
static int cmdCursor = 0;
static char *hintsBuf[HINTS_BUF_S] = {0};
static int hintsQ = 0;
static int selectedHint = 0;

static char *logs[LOGS_LINES];
static int logsQ;
static float logsTimer;

ViewportLinkedList viewports;
Viewport *maximizedViewport;
Font inconsolata;
Texture2D iconsTexture;
Texture2D gridTexture;
Texture2D shadowTexture;
MouseCursor currentCursor, nextCursor;
Viewport *viewportsToShow[5] = {0};
bool viewportJustSwitched;
static bool drawFpsFlag;

char *infoText[] = {
    PROJECT_TITLE,
    "  ",
    " SHIFT+ENTER : Show/Hide Command Bar",
    "  SHIFT+W : Show/Hide Workshop",
    "SHIFT+C : Show/Hide Closet",
    "       SHIFT+R : Show/Hide RegionPresets",
    " SHIFT+T : Show/Hide Theater",

    NULL
};

/* <== Logs ====================================================> */

void PushLog(char *format, ...){
    va_list args;
    
    // PUSH THE LOG TO BE PRINTED IN SCREEN
    if (logsQ < LOGS_LINES){
        va_start(args, format);
        logs[logsQ] = (char*) calloc(256, sizeof(char));
        vsnprintf(logs[logsQ++], 256, format, args);
        va_end(args);
    }
    
    // PUSH THE LOG TO BE PRINTED IN STDOUT
    va_start(args, format);
    int logLen = strlen(format);
    char logFormat[logLen+2];
    strcpy(logFormat,format);
    logFormat[logLen] = '\n';
    logFormat[logLen + 1] = '\0';
    vprintf(logFormat,args);
    va_end(args);

    if (logsQ == 1)
        logsTimer = 0;
}

void PushLogSimple(char *log){
    PushLog("%s", log);
}

void PopLog(){
    if (logsQ <= 0) return; 
    free(logs[0]);
    for (int i=0; i<logsQ-1; i++){
        logs[i] = logs[i+1];
    }
    logsQ--;
    logsTimer = 0.0;
}

/* <== Render Pipeline =========================================> */

static void DrawCommandBar(){
    if (state != ON_COMMAND_BAR) return;
    int screenWidth = GetScreenWidth();
    DrawRectangle(0, 0, screenWidth, VIEWPORT_TITLE_H, CMD_BAR_C);
    if (selectedHint == 0)
        DrawRectangle(0, 0, MeasureTextEx(inconsolata, cmdBuf, TITLE_FONT_SIZE, 2).x, VIEWPORT_TITLE_H, CMD_BAR_VIEWPORT_C);
    DrawTextEx(inconsolata, cmdBuf, (Vector2){0,0}, TITLE_FONT_SIZE, 2, TEXT_C);
    
    int hintsOffset = screenWidth/2;
    for (int i=1; i<hintsQ; i++){
        int hintWidth = MeasureTextEx(inconsolata,hintsBuf[i],TITLE_FONT_SIZE,2).x; 
        if (i == selectedHint)
            DrawRectangle(hintsOffset, 0, hintWidth, VIEWPORT_TITLE_H, CMD_BAR_VIEWPORT_C);
        DrawTextEx(inconsolata, hintsBuf[i], (Vector2){hintsOffset,0}, TITLE_FONT_SIZE, 2, TEXT_C);
        hintsOffset += hintWidth + 20;
    }
}

static void DrawLogs(){
    int logsYOffset = 0;
    for (int i=logsQ-1; i>=0; i--){
        logsYOffset += DEFAULT_FONT_SIZE + 2;/* line spacing */
        Vector2 logPosition = (Vector2){0,GetScreenHeight()-logsYOffset};
        DrawTextEx(inconsolata, logs[i], logPosition, DEFAULT_FONT_SIZE, 2, LOGS_C);
    }
}

static int Render(){
    for (Viewport *v = viewports.head; v != NULL; v = v->next){
        if (v == viewports.tail || v->renderAlways) RenderViewport(v);
    }
    
    BeginDrawing();
    if (maximizedViewport != NULL){
        RenderViewportToScreen(maximizedViewport);
        DrawViewportUI(maximizedViewport);
    }
    else{
        ClearBackground(BG_C);
        DrawTextInCenter(32, 20, GetScreenWidth(), GetScreenHeight(), infoText);
    }
    
    // FOR EACH VIEWPORT
    for (Viewport *v = viewports.head; v != NULL; v = v->next) {
        if (v->hidden) continue;
        DrawViewportShadow(v);
        DrawViewportUI(v);
        DrawViewport(v);
    }

    DrawCommandBar();
    DrawLogs();
    if (drawFpsFlag) DrawFPS(10,10);
    EndDrawing();
    return 0;
}

/* <== States ==================================================> */

static void IdleState(){
    Vector2 mousePosition = GetMousePosition();
    if (viewportJustSwitched && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){
        viewportJustSwitched = false;
    }

    ChangeCursor(MOUSE_CURSOR_DEFAULT);

    for (int i=0; i<5; i++){
        Viewport *v = viewportsToShow[i];
        if (v != NULL){
            CleanViewportUIInput(viewports.tail);
            ProcessViewportUI(viewports.tail);
            if (v->hidden) ToggleViewport(v);
            else SetViewportOnTop(v);
            viewportsToShow[i] = NULL;
            return;
        }
    }

    if (IsKeyDown(MODKEY)){
        
        // COMMAND BAR TOGGLE
        if (IsKeyPressed(COMMAND_BAR_KEY)){
            memset(cmdBuf, '\0', CMD_BUF_S);
            cmdCursor = 0;
            selectedHint = 0;
            state = ON_COMMAND_BAR;
            return;
        }

        // VIEWPORTS TOGGLE
        for (Viewport *v = viewports.head; v != NULL; v = v->next){
            if (v->shortcut <= 0) continue;
            if (IsKeyPressed(v->shortcut)){
                ToggleViewport(v);
                return;
            }
        }
    }
    
    // VIEWPORTS FOCUS
    if (!IsMouseInsideViewport(viewports.tail,false,false,false) && !viewports.tail->alwaysOnTop ){
        for (Viewport *v = viewports.tail->prev; v != NULL; v = v->prev){ // for each viewport except the last-one
            if (v->hidden || !IsMouseInsideViewport(v, false, false,false)) continue;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){    
                viewportJustSwitched = true;
                SetViewportOnTop(v);
                break;
            }
        }
    }

    if (viewports.tail->hidden) return;
    Viewport *v = viewports.tail;

    // VIEWPORT MOVEMENT
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
        if (IsMouseInsideViewport(v, true, false, false)){
            state = MOVING_VIEWPORT;
            targetViewport = v;
            grabOffset = (Vector2){mousePosition.x-targetViewport->pos.x,mousePosition.y-targetViewport->pos.y};
            return;
        }
    }

    // VIEWPORT RESIZE
    if (v->resizable && IsMouseInsideViewport(v, false, true, false)){
        
        #if __linux__
            ChangeCursor(MOUSE_CURSOR_RESIZE_ALL);
        #else
            ChangeCursor(MOUSE_CURSOR_RESIZE_NWSE);
        #endif
        
    
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
            state = RESIZING_VIEWPORT;
            targetViewport = v;
            grabOffset = (Vector2){
                mousePosition.x-targetViewport->pos.x,
                mousePosition.y-targetViewport->pos.y };
            return;
        }
    }

    // PANELS RESIZE
    Rectangle leftPanelHandle, rightPanelHandle, topPanelHandle, bottomPanelHandle;
    GetPanelsHandles(v, &leftPanelHandle, &rightPanelHandle, &topPanelHandle, &bottomPanelHandle);

    if (v->LeftPanel != NULL && v->leftPanel.resizable && IsPointOnRect(mousePosition, leftPanelHandle)){
        ChangeCursor(MOUSE_CURSOR_RESIZE_EW);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
            targetPanelSize = &v->leftPanel.size;
            targetPanelType = LEFT_PANEL;
            grabOffset = v->pos;
            state = RESIZING_PANEL;
        }
    }

    else if (v->RightPanel != NULL && v->rightPanel.resizable && IsPointOnRect(mousePosition, rightPanelHandle) && !IsMouseInsideViewport(v, false, true, false)){
        ChangeCursor(MOUSE_CURSOR_RESIZE_EW);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
            targetPanelSize = &v->rightPanel.size;
            targetPanelType = RIGHT_PANEL;
            grabOffset = (Vector2){v->pos.x + v->leftPanel.size + v->size.width + v->rightPanel.size,0};
            state = RESIZING_PANEL;
        }
    }

    else if (v->TopPanel != NULL && v->topPanel.resizable && IsPointOnRect(mousePosition, topPanelHandle)){
        ChangeCursor(MOUSE_CURSOR_RESIZE_NS);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
            targetPanelSize = &v->topPanel.size;
            targetPanelType = TOP_PANEL;
            grabOffset = (Vector2){0,v->pos.y+VIEWPORT_TITLE_H};
            state = RESIZING_PANEL;
        }
    }

    else if (v->BottomPanel != NULL && v->bottomPanel.resizable && IsPointOnRect(mousePosition, bottomPanelHandle)){
        ChangeCursor(MOUSE_CURSOR_RESIZE_NS);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
            targetPanelSize = &v->bottomPanel.size;
            targetPanelType = BOTTOM_PANEL;
            grabOffset = (Vector2){0,v->pos.y + VIEWPORT_TITLE_H + v->topPanel.size + (v->size.height*-1) + v->bottomPanel.size};
            state = RESIZING_PANEL;
        }
    }

    // VIEWPORT EXIT
    if (IsMouseInsideViewport(v, false, false, false) && IsKeyPressed(KEY_ESCAPE)){
        ToggleViewport(v);
        return;
    }

    // VIEWPORT UPDATE
    if (IsMouseInsideViewport(v, false, false, true) && v->Update != NULL && !v->updateAlways ){
        v->Update(v);
    }

    else { // MAXIMIZED VIEWPORT UPDATE
        if (maximizedViewport != NULL && maximizedViewport->Update != NULL && !v->alwaysOnTop){
            maximizedViewport->Update(maximizedViewport);
            UpdateViewportUIInput(maximizedViewport);
            ProcessViewportUI(maximizedViewport);
        }
    }

    UpdateViewportUIInput(v);
}

static void MovingViewportState(){
    Vector2 mousePosition = GetMousePosition();
    targetViewport->pos = (Vector2){mousePosition.x-grabOffset.x,mousePosition.y-grabOffset.y};
    ProcessViewportUI(targetViewport);
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){
        state = IDLE;
        return;
    }
}

static void ResizingViewportState(){
    Vector2 mousePosition = GetMousePosition();
    float w = mousePosition.x - targetViewport->pos.x - targetViewport->rightPanel.size - targetViewport->leftPanel.size;
    float h = mousePosition.y - targetViewport->pos.y - targetViewport->topPanel.size - targetViewport->bottomPanel.size - VIEWPORT_TITLE_H;

    ResizeViewport(targetViewport, w, h);
    if (targetViewport->OnResize != NULL)
        targetViewport->OnResize(targetViewport);
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){
        state = IDLE;
        return;
    }
}

static void ResizingPanelState(){
    int newSize, delta;
    switch (targetPanelType){
        case LEFT_PANEL:
        case RIGHT_PANEL:
            ChangeCursor(MOUSE_CURSOR_RESIZE_EW);
            if (targetPanelType == LEFT_PANEL) newSize = GetMousePosition().x - grabOffset.x;
            else newSize = grabOffset.x - GetMousePosition().x;
            if (newSize > MIN_PANEL_SIZE){
                delta = newSize - *targetPanelSize;
                if (ResizeViewportW(viewports.tail, viewports.tail->size.width-delta))
                    *targetPanelSize = newSize;
            }
        break;
        
        
        case TOP_PANEL:
        case BOTTOM_PANEL:
            ChangeCursor(MOUSE_CURSOR_RESIZE_NS);    
            if (targetPanelType == TOP_PANEL) newSize = GetMousePosition().y - grabOffset.y;
            else newSize = grabOffset.y - GetMousePosition().y;
            if (newSize > MIN_PANEL_SIZE){
                delta = newSize - *targetPanelSize;
                if (ResizeViewportH(viewports.tail,(viewports.tail->size.height*-1)-delta))
                    *targetPanelSize = newSize;
            }
            
        break;
    }
    
    if (viewports.tail->OnResize != NULL)
        viewports.tail->OnResize(viewports.tail);

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){
        targetPanelSize = NULL;
        state = IDLE;
        return;
    }
}

static void OnCommandBarState(){
    // DELETE CHARACTERS
    if (cmdBuf[0] != '\0' && IsKeyPressed(KEY_BACKSPACE)){
        cmdBuf[cmdCursor-1] = '\0';
        cmdCursor--;
    }

    // CANCEL COMMAND
    if (IsKeyPressed(KEY_ESCAPE)){
        state = IDLE;
        return;
    }

    // ACCEPT COMMAND
    if (IsKeyPressed(KEY_ENTER)){
        state = IDLE;
        if (strcmp(hintsBuf[selectedHint], "fps") == 0){
            drawFpsFlag = !drawFpsFlag;
            return;
        }
        
        for (Viewport *v = viewports.head; v != NULL; v = v->next){
            if (strcmp(hintsBuf[selectedHint],v->title) == 0){
                if (v->hidden) ToggleViewport(v);
                else SetViewportOnTop(v);
                return;
            }
        }
        
        Viewport *top = viewports.tail;
        if (!top->hidden){
            char *argv[32];
            int argc = 0;
            while (true){
                argv[argc] = strtok(argc > 0 ? NULL:hintsBuf[selectedHint]," ");
                if (argv[argc] == NULL) break;
                argc++;
            }
            top->ExecCmd(top,argc,argv);
        }
        
        return;
    }

    // AUTOCOMPLETE COMMAND (TAB)
    if (IsKeyPressed(KEY_TAB) && selectedHint > 0){
        memset(cmdBuf,'\0', sizeof(char)*CMD_BUF_S);
        strcpy(cmdBuf,hintsBuf[selectedHint]);
        selectedHint = 0;
        cmdCursor = strlen(cmdBuf);
    }

    // ADD CHARACTERS
    char pressedChar;
    while (true) {
        pressedChar = GetCharPressed();
        if (pressedChar == 0) break;
        cmdBuf[cmdCursor++] = pressedChar;
    }

    // HINTS
    memset(hintsBuf+1, '\0', HINTS_BUF_S);
    hintsQ = 1;
    for (Viewport *v = viewports.head; v != NULL; v = v->next){
        if (!v->hideCmd && strstr(v->title, cmdBuf) != NULL)
            hintsBuf[hintsQ++] = v->title;
    }

    if (!viewports.tail->hidden){ //focus window hints
        Viewport *v = viewports.tail;
        const char **vCommands = v->GetCmds();
        int o=0;
        while (true){
            if (vCommands[o] == NULL) break;
            if (strstr(vCommands[o], cmdBuf) != NULL)
                hintsBuf[hintsQ++] = vCommands[o];
            o++;
        }
    }

    if (IsKeyPressed(KEY_RIGHT)) selectedHint++;
    if (IsKeyPressed(KEY_LEFT)) selectedHint--;
    if (selectedHint >= hintsQ) selectedHint = hintsQ-1;
    if (selectedHint < 0) selectedHint = 0;
    
}

/* <== Core ====================================================> */

static int MainLoop(){
    switch (state){
        case IDLE:               IdleState();              break;
        case MOVING_VIEWPORT:    MovingViewportState();    break;
        case RESIZING_VIEWPORT:  ResizingViewportState();  break;
        case RESIZING_PANEL:     ResizingPanelState();     break;
        case ON_COMMAND_BAR:     OnCommandBarState();      break;
    }

    // EXECUTE THE 'ALWAYS' LOGIC
    for (Viewport *v = viewports.head; v != NULL; v = v->next){
        if (v->updateAlways) v->Update(v);
        if (v->hidden) ProcessViewportUI(viewports.tail);
    }

    // EXECUTE EVERY MODULE
    for (Module *m = modules.head; m != NULL; m = m->next){
        if (m->Update != NULL){
            m->Update(m);
        }
    }

    // EXECUTE LOGS LOGIC
    if (logsQ > 0){
        logsTimer += GetFrameTime();
        if (logsTimer >= LOGS_SCREEN_TIME){
            PopLog();
        }
    }
    
    // SET CURSOR PER FRAME
    if (nextCursor != currentCursor){
        currentCursor = nextCursor;
        SetMouseCursor(currentCursor);
    }
    return 0;
}

static int Init(){
    inconsolata = LoadFontFromMemory(
        ".ttf", 
        Inconsolata_SemiBold_ttf, 
        Inconsolata_SemiBold_ttf_len, 
        256, 0, 250
    );

    Image iconsImage = LoadImageFromMemory(".png", icons_png, icons_png_len);
    iconsTexture = LoadTextureFromImage(iconsImage);
    UnloadImage(iconsImage);

    Image gridImage = LoadImageFromMemory(".png", gridTexture_png, gridTexture_png_len);
    gridTexture = LoadTextureFromImage(gridImage);
    UnloadImage(gridImage);

    Image shadowImage = LoadImageFromMemory(".png", shadow_png, shadow_png_len);
    shadowTexture = LoadTextureFromImage(shadowImage);
    UnloadImage(shadowImage);

    RegisterModules();
    for (Module *m = modules.head; m != NULL; m = m->next){
        if (m->Init != NULL){
            m->Init(m);
        }
    }

    RegisterViewports();
    for (Viewport *v = viewports.head; v != NULL; v = v->next){
        mu_init(&v->ctx);
        v->ctx.text_width = text_width;
        v->ctx.text_height = text_height;
        v->ctx.style->title_height = VIEWPORT_TITLE_H;
        v->ctx.style->font = (void*) &inconsolata;
        v->ctx.style->icons = (void*) &iconsTexture;
        v->ctx.style->colors[MU_COLOR_WINDOWBG]  = *(mu_Color*) &VIEWPORT_BG_C;
        v->ctx.style->colors[MU_COLOR_TITLEBG]   = *(mu_Color*) &VIEWPORT_TITLE_C;
        v->ctx.style->colors[MU_COLOR_TITLETEXT] = *(mu_Color*) &TEXT_C;
        v->ctx.style->colors[MU_COLOR_BORDER]    = *(mu_Color*) &VIEWPORT_OUTLINE_C;
        v->ctx.style->control_font_size          = DEFAULT_FONT_SIZE;
        v->ctx.style->title_font_size            = TITLE_FONT_SIZE;
        
        if (v->Init != NULL){
            v->Init(v);
        }
    }

    hintsBuf[0] = cmdBuf;

    return 0;
}

static void CleanUp(){
    UnloadTexture(iconsTexture);
    UnloadTexture(gridTexture);
    UnloadTexture(shadowTexture);
    for (Viewport *v = viewports.head; v != NULL; v = v->next){
        UnloadRenderTexture(v->renderTexture);
    }

    for (int i = 0; i < logsQ; ++i) {
        if (logs[i]) free(logs[i]);
    }
    logsQ = 0;

    CleanUpViewportsPool();
}

int main(){
    // raylib init
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(100, 100, PROJECT_TITLE); // width and height cannot be 0 because of wasm
    MaximizeWindow();
    SetExitKey(0);
    
    Init();
    SetTargetFPS(60);
    while (!WindowShouldClose()){
        MainLoop();
        Render();
    }

    CleanUp();
    CloseWindow();
    return 0;
}