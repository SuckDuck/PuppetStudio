#include <stdio.h>
#include "config.h"
#include "microui.h"
#include "raylib.h"
#include <raymath.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "viewports.h"
#include "utils.h"

void InitViewport(  char* title, 
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
                    void (*RenderOverlay)(Viewport*) ){

    Viewport *v = calloc(1, sizeof(Viewport));
    v->title = title;
    v->size = (Rectangle){0,0,w,h*-1};
    v->pos = (Vector2){x,y};
    v->minZoom = minZoom;
    v->maxZoom = maxZoom;
    v->shortcut = shortcut;
    v->hidden = true;
    v->hideCmd = hideCmd;
    v->resizable = resizable;
    v->alwaysOnTop = alwaysOnTop;
    v->Init = Init;
    v->Update = Update;
    v->OnResize = OnResize;
    v->GetCmds = GetCmds;
    v->ExecCmd = ExecCmd;
    v->RightPanel = RightPanel;
    v->LeftPanel = LeftPanel;
    v->TopPanel = TopPanel;
    v->BottomPanel = BottomPanel;
    v->RenderUnderlay = RenderUnderlay;
    v->Render = Render;
    v->RenderOverlay = RenderOverlay;
    v->camera.target = (Vector2){0,0};
    v->camera.offset = (Vector2){w/2,h/2};
    v->camera.rotation = 0.0f;
    v->camera.zoom = 1.0f;
    v->renderTexture = LoadCustomRenderTexture(w,h);
    
    // LINK THE LIST
    if (viewports.tail == NULL){
        viewports.head = v;
    }

    else{
        viewports.tail->next = v;
        v->prev = viewports.tail;
    }

    viewports.tail = v;
}

void CleanUpViewportsPool(){
    for(Viewport *v = viewports.head; v != NULL; v = v->next){
        if (v->prev != NULL)
            free(v->prev);
    }
    free(viewports.tail);
    viewports.head = NULL;
    viewports.tail = NULL;
    maximizedViewport = NULL;
}

void RenderViewportToScreen(Viewport *v){
    BeginMode2D(v->camera);
    ClearBackground(VIEWPORT_BG_C);
    if (v->Render != NULL) v->Render(v);
    EndMode2D();
    if (v->RenderOverlay != NULL) v->RenderOverlay(v);
    DrawRectangleLinesEx(
        (Rectangle){0,0,GetScreenWidth(),GetScreenHeight()},
        VIEWPORT_OUTLINE_T,
        v == viewports.tail ? VIEWPORT_TITLE_C : VIEWPORT_OUTLINE_C);
}

void RenderViewport(Viewport *v){
    BeginTextureMode(v->renderTexture);
    ClearBackground(VIEWPORT_BG_C);
    if (v->RenderUnderlay != NULL) v->RenderUnderlay(v);
    BeginMode2D(v->camera);
    if (v->Render != NULL) v->Render(v);
    EndMode2D();
    if (v->RenderOverlay != NULL) v->RenderOverlay(v);
    EndTextureMode();
}

void UpdateViewportUIInput(Viewport *v){
    Vector2 mousePosition = GetMousePosition();
    mu_input_mousemove(&v->ctx, mousePosition.x, mousePosition.y);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) mu_input_mousedown(&v->ctx, mousePosition.x,mousePosition.y,MU_MOUSE_LEFT);
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) mu_input_mouseup(&v->ctx,mousePosition.x,mousePosition.y,MU_MOUSE_LEFT);
    mu_input_scroll(&v->ctx, 0, GetMouseWheelMove()*SCROLL_SPEED*-1);
    if (IsKeyPressed(KEY_ENTER)) mu_input_keydown(&v->ctx, MU_KEY_RETURN);
    if (IsKeyReleased(KEY_ENTER)) mu_input_keyup(&v->ctx, MU_KEY_RETURN);    
    if (IsKeyPressed(KEY_BACKSPACE)) mu_input_keydown(&v->ctx, MU_KEY_BACKSPACE);
    if (IsKeyReleased(KEY_BACKSPACE)) mu_input_keyup(&v->ctx, MU_KEY_BACKSPACE);

    char pressedChar[2] = {0};
    while (true) {
        *pressedChar = GetCharPressed();
        if (*pressedChar == 0) break;
        mu_input_text(&v->ctx, pressedChar);
    }
}

void CleanViewportUIInput(Viewport *v){
    mu_input_mousemove(&v->ctx, 0, 0);
    mu_input_mouseup(&v->ctx, 0, 0, MU_MOUSE_LEFT);
    mu_input_mouseup(&v->ctx, 0, 0, MU_MOUSE_RIGHT);
    mu_input_scroll(&v->ctx, 0, 0);
}

void ProcessViewportUI(Viewport *v){
    mu_Context *ctx = &v->ctx;
    mu_begin(ctx);
    
    // RIGHT PANEL
    if (v->RightPanel != NULL){    
        mu_Rect rect = mu_rect(
            v->pos.x+v->size.width + v->leftPanel.size + 2, 
            v->pos.y+VIEWPORT_TITLE_H, 
            v->rightPanel.size-1, 
            (v->size.height*-1)+v->topPanel.size+v->bottomPanel.size);

        mu_begin_window(ctx, "rightPanel", rect);
        mu_get_current_container(ctx)->rect = rect;
        v->RightPanel(v,&v->ctx);
        mu_end_window(ctx);
    }

    // LEFT PANEL
    if (v->LeftPanel != NULL){
        mu_Rect rect = mu_rect(
            v->pos.x, 
            v->pos.y+VIEWPORT_TITLE_H, 
            v->leftPanel.size, 
            (v->size.height*-1)+v->topPanel.size+v->bottomPanel.size );

        mu_begin_window(ctx, "leftPanel", rect);
        mu_get_current_container(ctx)->rect = rect;
        v->LeftPanel(v,&v->ctx);
        mu_end_window(ctx);
    }

    // TOP PANEL
    if (v->TopPanel != NULL){
        mu_Rect rect = mu_rect(
            v->pos.x + v->leftPanel.size+1, 
            v->pos.y+VIEWPORT_TITLE_H-1, 
            v->size.width, 
            v->topPanel.size );

        mu_begin_window(ctx, "topPanel", rect);
        mu_get_current_container(ctx)->rect = rect;
        v->TopPanel(v,&v->ctx);
        mu_end_window(ctx);
    }

    // BOTTOM PANEL
    if (v->BottomPanel != NULL){
        mu_Rect rect = mu_rect(
            v->pos.x + v->leftPanel.size+1, 
            v->pos.y + (v->size.height*-1) + VIEWPORT_TITLE_H + v->topPanel.size +1, 
            v->size.width, 
            v->bottomPanel.size-1 );

        mu_begin_window(ctx, "bottomPanel", rect);
        mu_get_current_container(ctx)->rect = rect;
        v->BottomPanel(v,&v->ctx);
        mu_end_window(ctx);
    }

    mu_end(&v->ctx);
}

void SetViewportPanelsDimensions(Viewport *v, int leftPanel, int rightPanel, int topPanel, int bottomPanel){
    if (v->LeftPanel != NULL) v->leftPanel.size = leftPanel;
    if (v->RightPanel != NULL) v->rightPanel.size = rightPanel;
    if (v->TopPanel != NULL) v->topPanel.size = topPanel;
    if (v->BottomPanel != NULL) v->bottomPanel.size = bottomPanel;
}

void ResizeViewport(Viewport *v, int w, int h){
    if (w == -1) w = v->size.width;
    if (h == -1) h = v->size.height;
    if (w >= 20) v->size.width = w;
    if (h >= 20) v->size.height = h*-1;
    
    if (!v->disableOffsetUpdate){
        v->camera.offset = (Vector2){
            v->size.width/2,
            (v->size.height*-1)/2
        };
    }

    UnloadRenderTexture(v->renderTexture);
    v->renderTexture = LoadCustomRenderTexture(v->size.width, v->size.height*-1);

    ProcessViewportUI(v);
}

bool ResizeViewportW(Viewport *v, int w){
    if (w < 20) return false;
    ResizeViewport(v, w, -1);
    return true;
}

bool ResizeViewportH(Viewport *v, int h){
    if (h < 20) return false;
    ResizeViewport(v, -1, h);
    return true;
}

void DrawGridTexture(Viewport *v){
    DrawTextureRec(gridTexture,v->size,Vector2Zero(),VIEWPORT_GRID_C);
}

void DrawViewport(Viewport *v){
    Vector2 pos = (Vector2){
        v->pos.x + v->leftPanel.size+1,
        v->pos.y + VIEWPORT_TITLE_H + v->topPanel.size
    };

    DrawTextureRec(v->renderTexture.texture,v->size,pos,WHITE);

    // DRAW BORDER
    Rectangle border = (Rectangle){
        v->pos.x, v->pos.y,
        v->size.width+v->rightPanel.size+v->leftPanel.size + 1,
        (v->size.height*-1) + VIEWPORT_TITLE_H + v->topPanel.size + v->bottomPanel.size
    };

    DrawRectangleLinesEx(
        border,
        VIEWPORT_OUTLINE_T,
        v == viewports.tail ? VIEWPORT_TITLE_C : VIEWPORT_OUTLINE_C);
    
    // DRAW TITLEBAR
    DrawRectangle(
        v->pos.x,v->pos.y, 
        v->size.width + v->rightPanel.size + v->leftPanel.size, 
        VIEWPORT_TITLE_H, 
        VIEWPORT_TITLE_C );
    
    DrawTextEx(
        inconsolata, 
        v->title, 
        (Vector2){ v->pos.x+VIEWPORT_OUTLINE_T, v->pos.y+VIEWPORT_OUTLINE_T }, 
        VIEWPORT_TITLE_H, 
        1, 
        TEXT_C );
}

void DrawViewportUI(Viewport *v){
    mu_Command *cmd = NULL;
    int radius;
    while (mu_next_command(&v->ctx, &cmd)) {
        switch(cmd->type){
            case MU_COMMAND_TEXT:
            DrawTextEx(
                *(Font*)(cmd->text.font), 
                cmd->text.str, 
                (Vector2){cmd->text.pos.x,cmd->text.pos.y},
                cmd->text.font_size,
                2,
                *(Color*)&cmd->text.color
            );
            break;

            case MU_COMMAND_RECT:
            DrawRectangle(
                cmd->rect.rect.x, 
                cmd->rect.rect.y, 
                cmd->rect.rect.w, 
                cmd->rect.rect.h, 
                *(Color*)&cmd->rect.color
            );
            break;

            case MU_COMMAND_CIRCLE:            
            radius = cmd->circle.rect.w/2;
            DrawCircle(
                cmd->circle.rect.x + radius,
                cmd->circle.rect.y + radius,
                radius,
                *(Color*)&cmd->circle.color
            );
            break;

            case MU_COMMAND_CIRCLE_LINES:
            radius = cmd->circle.rect.w/2;
            DrawRing(
                (Vector2){
                    cmd->circle_lines.rect.x + radius,
                    cmd->circle_lines.rect.y + radius }, 
                radius-2, 
                radius, 
                0.0, 
                360.0, 
                20, 
                *(Color*)&cmd->circle_lines.color
            );
            break;

            case MU_COMMAND_ICON: 
            Texture2D icons = *(Texture2D*) v->ctx.style->icons;
            int iconsWidth = icons.width/ICONS_Q;
            DrawTexturePro(
                *(Texture2D*) v->ctx.style->icons,
                (Rectangle){iconsWidth*(cmd->icon.id-1),0,iconsWidth,icons.height}, 
                (Rectangle){cmd->icon.rect.x, cmd->icon.rect.y, cmd->icon.rect.w, cmd->icon.rect.h}, 
                (Vector2){0,0}, 
                0.0, 
                TEXT_C
            );
            break;
            
            case MU_COMMAND_CLIP: 
            mu_Rect r = cmd->clip.rect;    
            if (r.x == 0 && r.y == 0){
                EndScissorMode();
                break;
            }
            
            BeginScissorMode(r.x, r.y, r.w, r.h);
            break;
        }
    }
}

void DrawViewportShadow(Viewport *v){
     Rectangle dst = (Rectangle){
        v->pos.x, v->pos.y,
        v->size.width+v->rightPanel.size+v->leftPanel.size + 1,
        (v->size.height*-1) + VIEWPORT_TITLE_H + v->topPanel.size + v->bottomPanel.size
    };

    // DRAW SHAODW
    int borderWidth = 20;
    DrawTexturePro(
        shadowTexture, 
        (Rectangle){
            0,0,
            256,256
        }, 
        (Rectangle){
            dst.x-borderWidth, dst.y-borderWidth,
            dst.width+(borderWidth*2), 
            dst.height+(borderWidth*2)
        }, 
        Vector2Zero(), 
        0, 
        WHITE
    );
}

void SetViewportOnTop(Viewport *v){
    if (v == viewports.tail) return;
    
    if (v->prev != NULL) v->prev->next = v->next;
    else viewports.head = v->next;
        
    v->next->prev = v->prev;
    viewports.tail->next = v;
    v->prev = viewports.tail;
    v->next = NULL;
    viewports.tail = v;
}

void RestoreViewport(){
    if (viewports.tail == NULL){
        viewports.head = maximizedViewport;
    }
    else{
        viewports.tail->next = maximizedViewport;
        maximizedViewport->prev = viewports.tail;
    }
    viewports.tail = maximizedViewport;
    maximizedViewport = NULL;
}

void MaximizeViewport(Viewport *v){
    if (v == NULL) return;
    if (maximizedViewport != NULL) return;
    v->pos = (Vector2){0,0};
    maximizedViewport = v;
    
    // take off from the queue
    if (v == viewports.head) viewports.head = v->next;
    if (v == viewports.tail) viewports.tail = v->prev;
    if (v->prev != NULL) v->prev->next = v->next;
    if (v->next != NULL) v->next->prev = v->prev;
    v->prev = NULL;
    v->next = NULL;
}

bool IsMouseInsideViewport(Viewport* v, bool titleBar, bool resizeHandle, bool onlyViewport){
    Vector2 mousePosition = GetMousePosition();
    if (v == maximizedViewport) return true;
    
    if (titleBar){
        Rectangle title = (Rectangle){
            v->pos.x, 
            v->pos.y, 
            v->size.width+v->rightPanel.size + v->leftPanel.size, 
            VIEWPORT_TITLE_H
        };
        
        if (IsPointOnRect(mousePosition, title)) return true;
        return false;
    }
    
    if (resizeHandle){
        Rectangle corner = (Rectangle){
            v->pos.x+v->size.width+v->rightPanel.size+v->leftPanel.size-VIEWPORT_CORNER_S,
            v->pos.y+(v->size.height*-1)+v->topPanel.size+v->bottomPanel.size+VIEWPORT_TITLE_H-VIEWPORT_CORNER_S,
            VIEWPORT_CORNER_S,VIEWPORT_CORNER_S
        };
        if (IsPointOnRect(mousePosition, corner)) return true;
        return false;
    }

    if (onlyViewport){
        Rectangle onlyViewportArea = (Rectangle){
            v->pos.x+v->leftPanel.size,
            v->pos.y+VIEWPORT_TITLE_H+v->topPanel.size,
            v->size.width,
            (v->size.height*-1)
        };
        if (IsPointOnRect(mousePosition, onlyViewportArea)) return true;
        return false;
    }

    Rectangle viewportArea = (Rectangle){
        v->pos.x,v->pos.y,
        v->size.width + v->rightPanel.size + v->leftPanel.size,
        (v->size.height*-1) + v->topPanel.size + v->bottomPanel.size + VIEWPORT_TITLE_H
    };
    if (IsPointOnRect(mousePosition, viewportArea)) return true;
    return false;
}

Vector2 GetMouseOverlayPosition(Viewport* v){
    Vector2 mousePosition = GetMousePosition();
    return (Vector2){
        mousePosition.x - (v->pos.x + VIEWPORT_OUTLINE_T + v->leftPanel.size),
        mousePosition.y - (v->pos.y + VIEWPORT_TITLE_H + v->topPanel.size)
    };
}

Vector2 GetMouseViewportPosition(Viewport* v){
    Vector2 mousePosition = GetMousePosition();
    return (Vector2){
        (mousePosition.x - (v->pos.x+v->leftPanel.size) - v->camera.offset.x)/v->camera.zoom + v->camera.target.x,
        (mousePosition.y - (v->pos.y+v->topPanel.size+VIEWPORT_TITLE_H) - v->camera.offset.y)/v->camera.zoom + v->camera.target.y
    };
}

bool IsMouseButtonPressedFocusSafe(int button){
    if (viewportJustSwitched) return false;
    return IsMouseButtonPressed(button);
}

bool IsMouseButtonReleasedFocusSafe(int button){
    if (viewportJustSwitched) return false;
    return IsMouseButtonReleased(button);
}

void ViewportUpdateZoom(Viewport* v){
    if (!IsMouseInsideViewport(v, false, false, true)) return;
    v->camera.zoom += GetMouseWheelMove() * ZOOM_SPEED;
    if (v->camera.zoom < v->minZoom) v->camera.zoom = v->minZoom;
    if (v->camera.zoom > v->maxZoom) v->camera.zoom = v->maxZoom;
}

int ViewportUpdatePan(Viewport* v){
    if (!IsMouseInsideViewport(v, false, false, true)) return 1;
    if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
        Vector2 d = GetMouseDelta();
        v->camera.target = (Vector2){v->camera.target.x - (d.x / v->camera.zoom),v->camera.target.y - (d.y / v->camera.zoom)};
        return 0;
    }
    return 1;
}

void ToggleViewport(Viewport *v){
    if (v == maximizedViewport){
        v->hidden = false;
        return;
    }

    v->hidden = !v->hidden;
    if (!v->hidden){
        SetViewportOnTop(v);
    }
    else if (v == viewports.tail){
        while (true){
            if (v == NULL) return;
            if (!v->hidden){
                SetViewportOnTop(v);
                return;
            }
            
            v = v->prev;    
        }
    }
}

void ToggleViewportByName(char *name){
    for (Viewport *v = viewports.head; v != NULL; v = v->next){
        if (strcmp(name, v->title) == 0){
            ToggleViewport(v);
            return;
        }
    }
}

void OpenViewportByName(char *name){
    for (Viewport *v = viewports.head; v != NULL; v = v->next){
        if (strcmp(name, v->title) == 0){
            for (int i=0; i<5; i++){
                if (viewportsToShow[i] == NULL){
                    viewportsToShow[i] = v;
                    return;
                }
            }
        }
    }
}

void CloseViewportByName(char *name){
    for (Viewport *v = viewports.head; v != NULL; v = v->next){
        if (strcmp(name, v->title) == 0){
            if (!v->hidden) ToggleViewport(v);
            return;
        }
    }
}

void GetPanelsHandles(Viewport *v, Rectangle *L, Rectangle *R, Rectangle *T, Rectangle *B){
    if (v->LeftPanel && L){
        *L = (Rectangle){
            v->pos.x + v->leftPanel.size - VIEWPORT_PANEL_HANDLE_S,
            v->pos.y + VIEWPORT_TITLE_H,
            VIEWPORT_PANEL_HANDLE_S*2,
            (v->size.height*-1)+v->topPanel.size+v->bottomPanel.size
        };
    }

    if (v->RightPanel && R){
        *R = (Rectangle){
            v->pos.x + v->leftPanel.size + v->size.width - VIEWPORT_PANEL_HANDLE_S,
            v->pos.y + VIEWPORT_TITLE_H,
            VIEWPORT_PANEL_HANDLE_S*2,
            (v->size.height*-1)+v->topPanel.size+v->bottomPanel.size
        };
    }

    if (v->TopPanel && T){
        *T = (Rectangle){
            v->pos.x + v->leftPanel.size,
            v->pos.y + VIEWPORT_TITLE_H + v->topPanel.size - VIEWPORT_PANEL_HANDLE_S,
            v->size.width,
            VIEWPORT_PANEL_HANDLE_S*2
        };
    }

    if (v->BottomPanel && B){
        *B = (Rectangle){
            v->pos.x + v->leftPanel.size,
            v->pos.y + VIEWPORT_TITLE_H + v->topPanel.size + (v->size.height*-1) - VIEWPORT_PANEL_HANDLE_S,
            v->size.width,
            VIEWPORT_PANEL_HANDLE_S*2
        };
    }
}

int RegisterViewports(){
    //filedialog.c
    InitViewport(
        "File dialog", 
        500, 400, 
        0, 0, 
        0.5, 3.0, 
        KEY_NULL, 
        true,
        false,
        true,
        FileDialogInit, 
        &FileDialogUpdate, 
        NULL,
        &FileDialogGetCommands, 
        &FileDialogExecCmd, 
        NULL,
        NULL,
        NULL,
        NULL, 
        NULL,
        FileDialogRender, 
        &FileDialogRenderOverlay
    );
    
    //workshop.c
    InitViewport(
        "WorkShop",
        500, 500,
        0.0, 0.0,
        0.5, 3.0,
        KEY_W,
        false,
        true,
        false,
        &WorkShopInit,
        &WorkShopUpdate,
        NULL,
        &WorkShopGetCommands,
        &WorkShopExecCmd,
        &WorkShopRightPanel,
        NULL,
        NULL,
        NULL,
        &WorkShopRenderUnderlay,
        &WorkShopRender,
        &WorkShopRenderOverlay
    );

    //closet.c
    InitViewport(
        "Closet",
        500, 500,
        0.0, 0.0,
        0.5, 3.0,
        KEY_C,
        false,
        true,
        false,
        &ClosetInit,
        &ClosetUpdate,
        NULL,
        &ClosetGetCommands,
        &ClosetExecCmd,
        &ClosetRightPanel,
        NULL,
        &ClosetTopPanel,
        NULL,
        NULL,
        &ClosetRender,
        &ClosetRenderOverlay
    );

    //regionpresets.c
    InitViewport(
        "RegionPresets", 
        320, 300, 
        0, 0, 
        0.5, 3.0, 
        KEY_R, 
        false, 
        false, 
        false, 
        &RPInit, 
        &RPUpdate, 
        NULL,
        &RPGetCmds, 
        &RPExecCmd, 
        NULL, 
        NULL,
        &RPTopPanel,
        NULL,
        &RPRenderUnderlay, 
        &RPRender, 
        &RPRenderOverlay
    );

    //theatre.c
    InitViewport(
        "Theater", 
        680, 500, 
        0, 0, 
        0.5, 3.0, 
        KEY_T, 
        false, 
        true, 
        false, 
        &TheatreInit, 
        &TheatreUpdate, 
        &TheatreOnResize,
        &TheatreGetCommands, 
        &TheatreExecCmd, 
        &TheatreRightPanel, 
        &TheatreLeftPanel,
        NULL,
        &TheatreBottomPanel,
        &TheatreRenderUnderlay, 
        &TheatreRender, 
        &TheatreRenderOverlay
    );

    #if __wasm__
        //storagebridge.c
        InitViewport(
            "Storage Bridge", 
            1, 170, 
            0, 0, 
            0.5, 3.0, 
            KEY_NULL, 
            false,
            false,
            false,
            StorageBridgeInit, 
            NULL, 
            NULL,
            &StorageBridgeGetCommands, 
            &StorageBridgeExecCmd, 
            NULL,
            &StorageBridgeLeftPanel,
            NULL,
            NULL, 
            NULL,
            NULL, 
            NULL
        );

        //welcome.c
        InitViewport(
            "Welcome", 
            1, 410,
            0, 0, 
            1, 1, 
            KEY_NULL, 
            true, 
            false, 
            false, 
            &WelcomeInit, 
            NULL, 
            NULL, 
            &WelcomeGetCommands, 
            &WelcomeExecCmd, 
            NULL, 
            &WelcomeLeftPanel, 
            NULL, 
            NULL, 
            NULL, 
            NULL, 
            NULL
        );
    #endif

    return 0;
}