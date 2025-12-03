#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "microui.h"
#include "theater.h"
#include "viewports.h"
#include "puppets.h"
#include "utils.h"
#include "config.h"

typedef enum State {
    NEW_BONE_MODE,
    MOVING_BONE
} State;

static State state;
static Vector2 mousePosition;
static Texture2D referenceImage;
static float referenceImageScale = 1.0;
static float referenceImageOpacity = 255;
static int lastZIndex = -1; //because it does ++lastZIndex later...
static int renderBones = 1;
static int absoluteMovement = 0;
static int editRange = 1;
static int disbleMove = 0;
static int propagateRotation = 1;

static const char *commands[] = {
    "SavePuppet",
    "LoadPuppet",
    NULL
};

void DeleteSelectedBone(){
    if (onEditSelectedBone == NULL) return;
    if (onEditSelectedBone == onEditPuppet) return;
    Bone *b = onEditSelectedBone->parent;
    DeleteBone(onEditSelectedBone);
    RebuildDescendants(onEditPuppet);
    lastZIndex = RebuildZIndex(onEditPuppet);
    onEditSelectedBone = b;
}

void DeleteEditPuppet(){
    if (onEditPuppet == NULL) return;
    DeletePuppet(onEditPuppet);
    onEditSelectedBone = NULL;
}

void NewEditPuppet(){
    DeleteEditPuppet();
    onEditPuppet = NewPuppet();
    lastZIndex = -1;
}

void SaveEditPuppet(char *filename){
    if (onEditPuppet == NULL){
        PushLog("There is no puppet to save!");
        return;
    }

    if (!IsFileExtension(filename, ".puppet")){
        PushLog("filename should contain the '.puppet' extension");
        return;
    }
    
    int rs = SavePuppet(onEditPuppet,filename);
    if (rs == 0){
        PushLog("Puppet '%s' succesfully saved!",filename);
    }
    else{
        PushLog("Error %i when saving puppet!", rs);
    }
}

void OpenEditPuppet(char *filename){
    if (!IsFileExtension(filename, ".puppet")){
        PushLog("filename should contain the '.puppet' extension");
        return;
    }
    
    Puppet *newPuppet = LoadPuppet(filename);
    if (newPuppet == NULL){
        PushLog("Puppet name is invalid!");
        return;
    }

    DeleteEditPuppet();    
    onEditPuppet = newPuppet;
    lastZIndex = onEditPuppet->descendantsQ-1;
    editRange = false;
    PushLog("Puppet '%s' succesfully loaded!",filename);
}

void ImportReferenceImage(char *filename){
    if (!FileExists(filename)){
        PushLog("Reference image filename is invalid!");
        return;
    }

    if (referenceImage.width != 0){
        UnloadTexture(referenceImage);
        memset(&referenceImage,0,sizeof(Texture2D));
    }

    Image i = LoadImage(filename);
    if (i.data == NULL){
        PushLog("image '%s' could not be loaded!",filename);
        return;
    }

    referenceImage = LoadTextureFromImage(i);
    UnloadImage(i);
}

void UnloadReferenceImage(){
    if (referenceImage.width == 0) return;
    UnloadTexture(referenceImage);
    memset(&referenceImage,0,sizeof(Texture2D));
}

/* <== States =========================================> */

static char* GetStateStr(State s){
    switch (state) {
        case NEW_BONE_MODE: return "NEW_BONE_MODE";
        case MOVING_BONE:     return "MOVING_BONE";
    }

    return NULL;
}

static void NewBoneModeState(Viewport *v){
    if (onEditPuppet == NULL) return;
    if (IsKeyPressed(KEY_ESCAPE)) onEditSelectedBone = NULL;

    // SELECTS THE PUPPET ROOT
    if (IsPointOnCircle(mousePosition, onEditPuppet->position, HINGE_RADIUS/v->camera.zoom)){
        ChangeCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressedFocusSafe(MOUSE_BUTTON_LEFT)){
            onEditSelectedBone = onEditPuppet;
            return;
        }
    }

    // SELECTS AND EDIT AN ALREADY EXISTING BONE
    for (int i=0; i<onEditPuppet->descendantsQ; i++){
        Bone *child = onEditPuppet->descendants[i];
        if (IsPointOnCircle(mousePosition, child->position, HINGE_RADIUS/v->camera.zoom)){
            ChangeCursor(MOUSE_CURSOR_POINTING_HAND);
            if (IsMouseButtonPressedFocusSafe(MOUSE_BUTTON_LEFT)){
                onEditSelectedBone = child;
                if (!disbleMove)
                    state = MOVING_BONE;
                return;    
            }
        }
    }

    
    // ADD NEW BONE
    if (onEditSelectedBone == NULL) return;
    if (IsMouseButtonPressedFocusSafe(MOUSE_BUTTON_LEFT)){
        onEditSelectedBone = AddBoneToPoint(onEditSelectedBone, mousePosition,++lastZIndex,(Skin){0});
        RebuildDescendants(onEditPuppet);
        state = MOVING_BONE;
    }
    
}

static void MovingBoneState(Viewport *v){
    ChangeCursor(MOUSE_CURSOR_POINTING_HAND);
    if (absoluteMovement) MoveBoneEndPoint(onEditSelectedBone,mousePosition);
    else RotateBonesTowards(onEditSelectedBone, mousePosition,editRange, propagateRotation, false);
    if (IsMouseButtonReleasedFocusSafe(MOUSE_BUTTON_LEFT)){
        state = NEW_BONE_MODE;
    }
}

/* <== Callbacks ======================================> */

void WorkShopInit(Viewport *v){
    SetViewportPanelsDimensions(v, 0, 400, 0, 0);
}

void WorkShopUpdate(Viewport *v){
    mousePosition = GetMouseViewportPosition(v);
    ViewportUpdateZoom(v);
    ViewportUpdatePan(v);

    switch (state) {
        case NEW_BONE_MODE: NewBoneModeState(v); break;
        case MOVING_BONE:   MovingBoneState(v);  break;
    }
    
    if (onEditPuppet){
        UpdateDescendantsPos(onEditPuppet, onEditPuppet->position, true);
    }
}

const char **WorkShopGetCommands(){
    return commands;
}

void WorkShopExecCmd(Viewport *v, int argc, char **argv){
    char *cmd = argv[0];
    char *filename = argv[1];
    if (strcmp(cmd, "SavePuppet") == 0){
        if (argc != 2){
            PushLog("Usage: SavePuppet [puppet name].puppet");
            return;
        }
        
        SaveEditPuppet(filename);
    }

    if (strcmp(cmd, "LoadPuppet") == 0){
        if (argc != 2){
            PushLog("Usage: LoadPuppet [puppet name].puppet");
            return;
        }
        
        OpenEditPuppet(filename);
    }

}

void WorkShopRightPanel(Viewport *v, mu_Context *ctx){
        
    /* <== Puppet =========================================> */
    if (mu_header(ctx, "Puppet")){
        mu_layout_row(ctx, 2, (int[]) { 20,-1 }, 0);
        mu_space(ctx);
        if (mu_button(ctx, "New Puppet")) NewEditPuppet();
        
        mu_layout_row(ctx, 5, (int[]) { 20,-150, -120, -60, -1 }, 0);
        mu_space(ctx);
        static char puppetFilename[PATH_MAX];
        mu_textbox(ctx, puppetFilename, PATH_MAX);
        long exploreID = 1853963465;
        mu_push_id(ctx, &exploreID, sizeof(long));
        if (mu_button(ctx, "...")) OpenExplorer(puppetFilename, PATH_MAX);
        mu_pop_id(ctx);
        if (mu_button(ctx, "Save")) SaveEditPuppet(puppetFilename);
        if (mu_button(ctx, "Open")) OpenEditPuppet(puppetFilename);

        mu_layout_row(ctx, 3, (int[]) { 20,-150, -1 }, 0);
        mu_space(ctx);
        static char puppetName[256];
        mu_textbox(ctx, puppetName, 256);
        if (mu_button(ctx, "Add To Project")){ //i'm not proud of this one
            if (onEditPuppet != NULL){
                if (strcmp(puppetName,"") == 0) PushLog("Can't add a noname puppet to project!");
                else if (PuppetNameIsOnList(puppetName,&puppetsCache)) PushLog("A puppet with that name already exists!");
                else{
                    CopyPuppetToList(onEditPuppet, &puppetsCache, puppetName);
                    PushLog("'%s' succesfully added to project!", puppetName);
                    memset(puppetName, '\0', 256);
                }
            }
            else PushLog("Can't add puppet to project, there is no puppet open!");
        }
    }

    mu_vertical_space(ctx,5);
    
    /* <== Atlas ==========================================> */
    if (mu_header(ctx, "Atlas")){
        mu_layout_row(ctx, 4, (int[]) { 20,-90, -60, -1 }, 0);
        mu_space(ctx);
        static char atlasFilename[PATH_MAX];
        mu_textbox(ctx, atlasFilename, PATH_MAX);
        long exploreID = 364389758393;
        mu_push_id(ctx, &exploreID, sizeof(long));
        if (mu_button(ctx, "...")) OpenExplorer(atlasFilename, PATH_MAX);
        mu_pop_id(ctx);
        if (mu_button(ctx, "Load")) LoadAtlasToPuppet(onEditPuppet, atlasFilename);
        mu_layout_row(ctx, 3, (int[]) { 20,-150, -1 }, 0);
        mu_space(ctx); mu_space(ctx);
        if (mu_button(ctx, "Open Closet"))
            OpenViewportByName("Closet");
    }

    mu_vertical_space(ctx,5);

    /* <== Bones ==========================================> */
    if (mu_header(ctx, "Bones")){
        char buf[64];
        mu_layout_row(ctx, 3, (int[]) {20, 125, 125}, 0);
            mu_space(ctx);
            if (mu_button(ctx, "FlipPuppetX")) XFlipPuppet(onEditPuppet);
            if (mu_button(ctx, "FlipPuppetY")) YFlipPuppet(onEditPuppet);
            mu_space(ctx);
            if (mu_button(ctx, "FlipBoneX") && onEditSelectedBone) XFlipSkin(&onEditSelectedBone->skin);
            if (mu_button(ctx, "FlipBoneY") && onEditSelectedBone) YFlipSkin(&onEditSelectedBone->skin);

        mu_layout_row(ctx, 2, (int[]) {20, 252}, 0);
            mu_space(ctx); if (mu_button(ctx, "Delete Bone")) DeleteSelectedBone();
            

        //Z-INDEX
        mu_layout_row(ctx, 5, (int[]) {20, 60,60,60,60 }, 0);
            mu_space(ctx);
            mu_label(ctx,"zIndex:",ctx->style->control_font_size);
            if (onEditSelectedBone != NULL && onEditSelectedBone->root != NULL)
                sprintf(buf,"%i", onEditSelectedBone->skin.zIndex);
            else strcpy(buf, "n/a");
            mu_textbox_ex(ctx, buf, 64, ctx->style->control_font_size, MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT);
            if (mu_button(ctx, "Up")) MoveBoneUpZIndex(onEditSelectedBone);
            if (mu_button(ctx, "Down")) MoveBoneDownZIndex(onEditSelectedBone);

        //ANGLE
        mu_layout_row(ctx, 5, (int[]) {20, 60,60,60,60 }, 0);
            static float boneAngle = 0.0;
            MuNumberORNa(ctx, "Angle:", &boneAngle, true, true);
            if (mu_button(ctx, "Get")){
                if (onEditSelectedBone != NULL)
                    boneAngle = VectorToDegrees(onEditSelectedBone->direction);
            };

            if (mu_button(ctx, "Set")){
                if (onEditSelectedBone != NULL)
                    RotateBonesDegrees(onEditSelectedBone, boneAngle, false);
            }

        //LENGTH
        mu_layout_row(ctx, 5, (int[]) {20, 60, 60, 60, 60}, 0);
            MuNumberORNa(ctx, "Length:", &onEditSelectedBone->len, onEditSelectedBone != NULL, true);
            MuNumberORNa(ctx, "Range:", &onEditSelectedBone->range, onEditSelectedBone != NULL, false);
            if (onEditPuppet != NULL && onEditSelectedBone != NULL && onEditSelectedBone->len > onEditSelectedBone->range){
                onEditSelectedBone->len = onEditSelectedBone->range;
            }
            
    }

    mu_vertical_space(ctx,5);
    
    /* <== Editor =========================================> */
    if (mu_header_ex(ctx, "Editor", ctx->style->control_font_size, MU_OPT_EXPANDED)){
        mu_layout_row(ctx, 5, (int[]) { 20, 90,-90, -60, -1 }, 0);
            mu_space(ctx);
            mu_label(ctx,"Reference:",ctx->style->control_font_size);
            static char referenceImageFilename[PATH_MAX];
            mu_textbox(ctx, referenceImageFilename, PATH_MAX);
            long exploreID = 352846289273;
            mu_push_id(ctx, &exploreID, sizeof(long));
            if (mu_button(ctx, "...")) OpenExplorer(referenceImageFilename, PATH_MAX);
            mu_pop_id(ctx);
            if (mu_button(ctx, "Import")) ImportReferenceImage(referenceImageFilename);

        if (referenceImage.width != 0){
            mu_layout_row(ctx, 4, (int[]) { 20, 70, 100, 70}, 0);
            mu_space(ctx);
            mu_label(ctx,"Scale:",ctx->style->control_font_size);
            mu_slider(ctx, &referenceImageScale, 0.2, 10);
            if (mu_button(ctx, "Reset")) referenceImageScale = 1;
            
            mu_layout_row(ctx, 3, (int[]) { 20, 70, 100}, 0);
            mu_space(ctx);
            mu_label(ctx,"Opacity:",ctx->style->control_font_size);
            mu_slider_ex(ctx, &referenceImageOpacity, 0, 255, 1, "%.0f", ctx->style->control_font_size, MU_OPT_ALIGNCENTER);
            
            mu_layout_row(ctx, 2, (int[]) { 20, 174}, 0);
            mu_space(ctx);
            if (mu_button(ctx, "Unload")) UnloadReferenceImage();
        }
        
        mu_layout_row(ctx, 2, (int[]) {20,-1 }, 0);
            mu_space(ctx); mu_checkbox(ctx, "Render Bones", ctx->style->control_font_size, &renderBones);
            mu_space(ctx); mu_checkbox(ctx, "Absolute Movement", ctx->style->control_font_size, &absoluteMovement);
            mu_space(ctx); mu_checkbox(ctx, "Disable Movement", ctx->style->control_font_size, &disbleMove);
            mu_space(ctx); mu_checkbox(ctx, "Edit Range", ctx->style->control_font_size, &editRange);
            mu_space(ctx); mu_checkbox(ctx, "Rotation Propagation", ctx->style->control_font_size, &propagateRotation);
    }
}

void WorkShopRenderUnderlay(Viewport *v){
    DrawGridTexture(v);
    if (referenceImage.width != 0){
        
        DrawTexturePro(
            referenceImage, 
            (Rectangle){0, 0, referenceImage.width, referenceImage.height}, 
            (Rectangle){
                v->size.width/2, 
                v->size.height*-1/2, 
                referenceImage.width*referenceImageScale, 
                referenceImage.height*referenceImageScale
            }, 
            (Vector2){
                (referenceImage.width*referenceImageScale)/2, 
                (referenceImage.height*referenceImageScale)/2
            },
            0, 
            (Color){255,255,255,referenceImageOpacity}
        );
    }
}

void WorkShopRender(Viewport *v){
    if (onEditPuppet != NULL){
        DrawPuppetSkin(onEditPuppet);
        if (onEditSelectedBone != NULL){
            DrawCircle(onEditSelectedBone->position.x, onEditSelectedBone->position.y, (HINGE_RADIUS+2)/v->camera.zoom, PINK);
        }
        
        DrawPuppetSkeleton(onEditPuppet,v->camera.zoom, renderBones);

        if (onEditSelectedBone != NULL && onEditSelectedBone != onEditPuppet && renderBones){
            DrawCircleLines(
                onEditSelectedBone->parent->position.x,
                onEditSelectedBone->parent->position.y,
                onEditSelectedBone->range,RED
            );
        }
    }
}

void WorkShopRenderOverlay(Viewport *v){
}