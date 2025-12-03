#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <stdio.h>
#include "microui.h"
#include "utils.h"
#include "viewports.h"
#include "modules.h"
#include "puppets.h"
#include "theater.h"

typedef enum State {
    IDLE,
    SETTING_RECT,
    SETTING_POINT_A,
    SETTING_POINT_B
} State;

typedef enum ClosetSelectorOption {
    WORKSHOP_OPT,
    THEATER_OPT
} ClosetSelectorOptions;

static State state;
static Vector2 mousePosition;
ClosetSelectorOptions closetSelectorOpts;
Puppet **closetSelectedPuppet;
Bone **closetSelectedBone;
static const char *commands[] = {
    NULL
};

// regionpresets.c
extern void AddRegion(char *name, Rectangle rect, Vector2 A, Vector2 B);

/* <== States =========================================> */

static void IdleState(Viewport *v){    
    // SET POINT A
    if (IsPointOnCircle(mousePosition,(*closetSelectedBone)->skin.pointA,(HINGE_RADIUS+2)/v->camera.zoom)){
        ChangeCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressedFocusSafe(MOUSE_BUTTON_LEFT)){
            state = SETTING_POINT_A;
            return;
        }
    }

    // SET POINT B
    else if (IsPointOnCircle(mousePosition,(*closetSelectedBone)->skin.pointB,(HINGE_RADIUS+2)/v->camera.zoom)){
        ChangeCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressedFocusSafe(MOUSE_BUTTON_LEFT)){
            state = SETTING_POINT_B;
            return;
        }
    }
    
    // SET RECT
    else if (IsMouseButtonPressedFocusSafe(MOUSE_BUTTON_LEFT)){
        (*closetSelectedBone)->skin.rect.x = mousePosition.x;
        (*closetSelectedBone)->skin.rect.y = mousePosition.y;
        (*closetSelectedBone)->skin.rect.width = 0;
        (*closetSelectedBone)->skin.rect.height = 0;
        state = SETTING_RECT;
    }
}

static void SettingRectState(Viewport *v){
    Bone *b = (*closetSelectedBone);
    b->skin.rect.width = mousePosition.x - b->skin.rect.x;
    b->skin.rect.height = mousePosition.y - b->skin.rect.y;
    if (IsMouseButtonReleasedFocusSafe(MOUSE_LEFT_BUTTON)){
        if (closetSelectorOpts == THEATER_OPT){
            NewPuppetSnapshot(*closetSelectedPuppet, timeline.currentFrame);
        }
        state = IDLE;
    }
}

static void SettingPointAState(Viewport *v){
    ChangeCursor(MOUSE_CURSOR_POINTING_HAND);
    (*closetSelectedBone)->skin.pointA.x = mousePosition.x;
    (*closetSelectedBone)->skin.pointA.y = mousePosition.y;
    if (IsMouseButtonReleasedFocusSafe(MOUSE_LEFT_BUTTON)){
        if (closetSelectorOpts == THEATER_OPT)
            NewPuppetSnapshot(*closetSelectedPuppet, timeline.currentFrame);
        state = IDLE;
    }
}

static void SettingPointBState(Viewport *v){
    ChangeCursor(MOUSE_CURSOR_POINTING_HAND);
    (*closetSelectedBone)->skin.pointB.x = mousePosition.x;
    (*closetSelectedBone)->skin.pointB.y = mousePosition.y;
    if (IsMouseButtonReleasedFocusSafe(MOUSE_LEFT_BUTTON)){
        SetSkinAngle(&(*closetSelectedBone)->skin);
        if (closetSelectorOpts == THEATER_OPT)
            NewPuppetSnapshot(*closetSelectedPuppet, timeline.currentFrame);
        state = IDLE;
    }
}

/* <== Callbacks ======================================> */

void ClosetInit(Viewport *v){
    v->renderAlways = true;
    SetViewportPanelsDimensions(v, 0, 300, 30, 0);
    closetSelectedPuppet = &onEditPuppet;
    closetSelectedBone = &onEditSelectedBone;
}

void ClosetUpdate(Viewport *v){
    ViewportUpdateZoom(v);
    ViewportUpdatePan(v);
    mousePosition = GetMouseViewportPosition(v);
    if ((*closetSelectedPuppet) == NULL || (*closetSelectedPuppet)->atlas == NULL || (*closetSelectedBone) == NULL) return;
    
    switch (state) {
        case IDLE:           IdleState(v);           break;
        case SETTING_RECT:   SettingRectState(v);    break;
        case SETTING_POINT_A: SettingPointAState(v); break;
        case SETTING_POINT_B: SettingPointBState(v); break;
    }

}

const char **ClosetGetCommands(){
    return commands;
}

void ClosetExecCmd(Viewport *v, int argc, char **argv){}

void ClosetRightPanel(Viewport *v, mu_Context *ctx){
        
    /* <== Rect ===========================================> */
    if (mu_header(ctx, "Rect")){
        mu_layout_row(ctx, 5, (int[]) {20, 18, 75, 18, 75 }, 0);
        mu_space(ctx);
        mu_label(ctx,"X:",ctx->style->control_font_size);
        if ((*closetSelectedBone) != NULL){
            mu_number_ex(
                ctx, 
                &(*closetSelectedBone)->skin.rect.x, 
                0.1f, 
                MU_SLIDER_FMT, 
                ctx->style->control_font_size, 
                1, 
                MU_OPT_ALIGNCENTER
            );
        }                
        else mu_textbox_ex(ctx, "n/a", 3, ctx->style->control_font_size, MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT);

        mu_label(ctx,"Y:",ctx->style->control_font_size); 
        if ((*closetSelectedBone) != NULL){
            mu_number_ex(
                ctx, 
                &(*closetSelectedBone)->skin.rect.y, 
                0.1f, 
                MU_SLIDER_FMT, 
                ctx->style->control_font_size, 
                1, 
                MU_OPT_ALIGNCENTER
            );
        }                
        else mu_textbox_ex(ctx, "n/a", 3, ctx->style->control_font_size, MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT);

        mu_space(ctx);
        mu_label(ctx,"W:",ctx->style->control_font_size); 
        if ((*closetSelectedBone) != NULL){
            mu_number_ex(
                ctx, 
                &(*closetSelectedBone)->skin.rect.width, 
                0.1f, 
                MU_SLIDER_FMT, 
                ctx->style->control_font_size, 
                1, 
                MU_OPT_ALIGNCENTER
            );
        }                
        else mu_textbox_ex(ctx, "n/a", 3, ctx->style->control_font_size, MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT);

        mu_label(ctx,"H:",ctx->style->control_font_size); 
        if ((*closetSelectedBone) != NULL){
            mu_number_ex(
                ctx, 
                &(*closetSelectedBone)->skin.rect.height, 
                0.1f, 
                MU_SLIDER_FMT, 
                ctx->style->control_font_size, 
                1, 
                MU_OPT_ALIGNCENTER
            );
        }                
        else mu_textbox_ex(ctx, "n/a", 3, ctx->style->control_font_size, MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT);
    }

    mu_vertical_space(ctx,5);

    /* <== Points =========================================> */
    if (mu_header(ctx, "Points:")){
        mu_layout_row(ctx, 5, (int[]) {20, 28, 75, 28, 75 }, 0);
        mu_space(ctx);
        mu_label(ctx,"AX:",ctx->style->control_font_size);
        if ((*closetSelectedBone) != NULL){
            mu_number_ex(
                ctx, 
                &(*closetSelectedBone)->skin.pointA.x, 
                0.1f, 
                MU_SLIDER_FMT, 
                ctx->style->control_font_size, 
                1, 
                MU_OPT_ALIGNCENTER
            );
        }                
        else mu_textbox_ex(ctx, "n/a", 3, ctx->style->control_font_size, MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT);

        mu_label(ctx,"AY:",ctx->style->control_font_size); 
        if ((*closetSelectedBone) != NULL){
            mu_number_ex(
                ctx, 
                &(*closetSelectedBone)->skin.pointA.y, 
                0.1f, 
                MU_SLIDER_FMT, 
                ctx->style->control_font_size, 
                1, 
                MU_OPT_ALIGNCENTER
            );
        }                
        else mu_textbox_ex(ctx, "n/a", 3, ctx->style->control_font_size, MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT);

        mu_space(ctx);
        mu_label(ctx,"BX:",ctx->style->control_font_size);
        if ((*closetSelectedBone) != NULL){
            mu_number_ex(
                ctx, 
                &(*closetSelectedBone)->skin.pointB.x, 
                0.1f, 
                MU_SLIDER_FMT, 
                ctx->style->control_font_size, 
                1, 
                MU_OPT_ALIGNCENTER
            );
        }                
        else mu_textbox_ex(ctx, "n/a", 3, ctx->style->control_font_size, MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT);

        mu_label(ctx,"BY:",ctx->style->control_font_size); 
        if ((*closetSelectedBone) != NULL){
            mu_number_ex(
                ctx, 
                &(*closetSelectedBone)->skin.pointB.y, 
                0.1f, 
                MU_SLIDER_FMT, 
                ctx->style->control_font_size, 
                1, 
                MU_OPT_ALIGNCENTER
            );
        }                
        else mu_textbox_ex(ctx, "n/a", 3, ctx->style->control_font_size, MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT);
    }

    mu_vertical_space(ctx,5);
    mu_layout_row(ctx, 2, (int[]){45,200}, 0);
    mu_label(ctx,"Name:",ctx->style->control_font_size);
    static char newRegionName[256];
    mu_textbox(ctx, newRegionName, 256);
    mu_space(ctx);
    if (mu_button(ctx, "Save region preset")){
        if ((*closetSelectedBone) != NULL){
            AddRegion(
                newRegionName, 
                (*closetSelectedBone)->skin.rect, 
                (*closetSelectedBone)->skin.pointA, 
                (*closetSelectedBone)->skin.pointB
            );
        }
        else{
            PushLog("Cant Add Region, no bone is selected!");
        }
    }

    mu_space(ctx);
    if (mu_button(ctx, "Open RegionPresets table")){
        OpenViewportByName("RegionPresets");
    }
}

void ClosetTopPanel(Viewport *v, mu_Context *ctx){
    mu_layout_row(ctx, 3, (int[]){100,100,100}, 0);
        if (mu_radiobutton(ctx, "Workshop", ctx->style->control_font_size, (int*) &closetSelectorOpts, 0)){
            closetSelectedPuppet = &onEditPuppet;
            closetSelectedBone = &onEditSelectedBone;
        }
        if (mu_radiobutton(ctx, "Theather", ctx->style->control_font_size, (int*) &closetSelectorOpts, 1)){
            closetSelectedPuppet = &theatreTargetPuppet;
            closetSelectedBone = &theatreTargetBone;
        }
}

void ClosetRender(Viewport *v){
    if ((*closetSelectedPuppet) == NULL || (*closetSelectedPuppet)->atlas == NULL) return;
    
    Texture *t = &(*closetSelectedPuppet)->atlas->texture;
    DrawTexture(*t,0,0,WHITE);

    if ((*closetSelectedBone) == NULL) return;
    Bone *b = (*closetSelectedBone);
    DrawRectangleLines(
        b->skin.rect.x, b->skin.rect.y, 
        b->skin.rect.width, b->skin.rect.height, 
        WHITE
    );

    DrawLine(b->skin.pointA.x,b->skin.pointA.y,b->skin.pointB.x,b->skin.pointB.y,WHITE);
    DrawCircle(b->skin.pointB.x, b->skin.pointB.y, HINGE_RADIUS/v->camera.zoom, YELLOW);
    DrawCircle(b->skin.pointA.x, b->skin.pointA.y, HINGE_RADIUS/v->camera.zoom, GREEN);
}

void ClosetRenderOverlay(Viewport *v){
    if ((*closetSelectedPuppet) == NULL){
        DrawTextInCenter(
            -2, -1,
            v->size.width, v->size.height *-1, 
            (char *[]){
                "There is no puppet",
                "selected",
                NULL
            }
        );
        return;
    }

    if ((*closetSelectedPuppet)->atlas == NULL){
        DrawTextInCenter(
            -2, -1,
            v->size.width, v->size.height *-1, 
            (char *[]){
                "Current puppet doesn't",
                "have an atlas loaded",
                NULL
            }
        );
        return;
    }
}

/* <== Module =========================================> */

void AtlasGarbageCollector(Module *m){
    while (true){
        bool masterBreak = true;
        for (Atlas *a = atlasCache.head; a != NULL; a = a->next){
            if (a->refCount == 0){
                printf("removing atlas '%lu'\n",a->id);
                RemoveAtlas(a);
                masterBreak = false;
                break;
            }
        }
        if (masterBreak) break;
    }
}