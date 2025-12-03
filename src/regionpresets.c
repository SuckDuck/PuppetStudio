#include <fcntl.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "microui.h"
#include "puppets.h"
#include "utils.h"
#include "viewports.h"
#include "config.h"
#include "theater.h"

typedef struct Region{
    char *name;
    Rectangle rect;
    Vector2 pointA;
    Vector2 pointB;
    struct Region *next;
    struct Region *prev;
} Region;

typedef struct RegionLinkedList{
    Region *head;
    Region *tail;
} RegionLinkedList;

static const char *commands[] = {
    NULL
};

RegionLinkedList regions;
static Vector2 mousePosition;
static Rectangle mouseHoverRect;
static float scroll = 0;

// closet.c
extern int closetSelectorOpts;
extern Puppet **closetSelectedPuppet;
extern Bone **closetSelectedBone;

void RemoveRegion(Region *r){
    if (r == NULL) return;
    if (r == regions.head) regions.head = r->next;
    if (r == regions.tail) regions.tail = r->prev;
    if (r->prev != NULL) r->prev->next = r->next;
    if (r->next != NULL) r->next->prev = r->prev;
    free(r->name);
    free(r);
}

void AddRegion(char *name, Rectangle rect, Vector2 A, Vector2 B){
    Region *r = NULL;
    
    for (Region *r1=regions.head; r1 != NULL; r1=r1->next){
        if (strcmp(name, r1->name) == 0){
            r = r1;
            break;
        }
    }

    if (r == NULL){
        r = calloc(1, sizeof(Region));
        
        // NAME
        int nameLen = strlen(name)+1;
        r->name = calloc(nameLen,1);
        r->name[nameLen-1] = '\0';
        strcpy(r->name, name);

        //link the list
        if (regions.tail != NULL){
            regions.tail->next = r;
            r->prev = regions.tail;
            regions.tail = r;
        }
        
        if (regions.head == NULL){
            regions.head = regions.tail = r;
        }
    }

    r->rect = rect;
    r->pointA = A;
    r->pointB = B;

    PushLog("Region '%s' succesfully added!",name);
}

void ApplyRegion(Region *r){
    if (r == NULL) return;
    if ((*closetSelectedBone) == NULL) return;
    (*closetSelectedBone)->skin.rect = r->rect;
    (*closetSelectedBone)->skin.pointA = r->pointA;
    (*closetSelectedBone)->skin.pointB = r->pointB;
    SetSkinAngle(&(*closetSelectedBone)->skin);
    if (closetSelectorOpts == 1){ //theater
        NewPuppetSnapshot(*closetSelectedPuppet, timeline.currentFrame);
    }
}

int ExportRegions(char *path){
    if (!IsFileExtension(path,".csv")){
        PushLog("file should contains the .csv extension!");
        return -1;
    }
    
    int fd = open(path,O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0){
        PushLog("file '%s' could not be created, error %i",path, fd);
        return fd;
    }
    
    for (Region *r = regions.head; r != NULL; r = r->next){
        char buf[256];
        sprintf(
            buf, 
            "%s,%f,%f,%f,%f,%f,%f,%f,%f\n", 
            r->name, 
            r->rect.x, r->rect.y, r->rect.width, r->rect.height, 
            r->pointA.x, r->pointA.y, 
            r->pointB.x, r->pointB.y
        );

        int size = strlen(buf);
        write(fd, buf, size);
    }

    close(fd);
    chmod(path,0666);
    
    PushLog("Region table '%s' exported succesfully!",path);
    return 0;
}

int ImportRegions(char *path){
    if (!IsFileExtension(path,".csv")){
        PushLog("file should contains the .csv extension!");
        return -1;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0){
        PushLog("Could not open '%s', error %i",path, fd);
        return fd;
    }

    char buf[256];
    for (int i=0; ; i++){
        int rs = read(fd,&buf[i],1);
        if (rs <= 0)
            break;

        if (buf[i] == '\n'){
            buf[i] = '\0';
            
            char *tokens[9];
            tokens[0] = strtok(buf, ",");
            for(int o=0; tokens[o] != NULL;)
                tokens[++o] = strtok(NULL, ",");
            
            AddRegion(
                tokens[0], 
                (Rectangle){atof(tokens[1]),atof(tokens[2]),atof(tokens[3]),atof(tokens[4])}, 
                (Vector2){atof(tokens[5]),atof(tokens[6])}, 
                (Vector2){atof(tokens[7]),atof(tokens[8])}
            );
            
            i = -1;
        }
    }
    
    close(fd);
    return 0;
}


/* <== Callbacks ======================================> */

void RPInit(Viewport *v){
    v->camera.offset = (Vector2){0,0};
    v->renderAlways = true;
    SetViewportPanelsDimensions(v, 0, 0, 30, 32);
}

void RPUpdate(Viewport *v){
    mousePosition = GetMouseViewportPosition(v);
    mouseHoverRect = (Rectangle){0};
    int yOffset = DEFAULT_FONT_SIZE;
    for (Region *r = regions.head; r != NULL; r = r->next){
        Rectangle regionRect = (Rectangle){0,yOffset,v->size.width, (DEFAULT_FONT_SIZE*4)+10};
        if (IsPointOnRect(mousePosition,regionRect)){
            mouseHoverRect = regionRect;

            if (IsMouseButtonPressedFocusSafe(MOUSE_LEFT_BUTTON)){
                ApplyRegion(r);
                return;
            }

            if (IsMouseButtonPressedFocusSafe(MOUSE_RIGHT_BUTTON)){
                RemoveRegion(r);
                return;
            }
        }
        yOffset += (DEFAULT_FONT_SIZE*4) + 10;
    }

    // SCROLL
    if (v->size.height*-1 < yOffset){
        scroll += GetMouseWheelMove()*SCROLL_SPEED*-1;
        float maxOffset = yOffset+v->size.height;
        if (scroll < 0) scroll = 0;
        if (scroll > maxOffset) scroll = maxOffset;
    }
    else scroll = 0;
    v->camera.offset.y = scroll*-1;
}

const char** RPGetCmds(){
    return commands;
}

void RPExecCmd(Viewport *v, int argc, char **argv){
    if (strcmp(argv[0], "clean")){
        for (Region *r=regions.head; r!=NULL; r=r->next)
            if (r->prev != NULL) RemoveRegion(r->prev);
        if (regions.tail != NULL) RemoveRegion(regions.tail);
    }
}

void RPTopPanel(Viewport *v, mu_Context *ctx){
    mu_layout_row(ctx, 4, (int[]) {-150, -120, -60, -1 }, 0);
    static char filename[PATH_MAX];
    mu_textbox(ctx, filename, PATH_MAX);
    if (mu_button(ctx, "...")) OpenExplorer(filename, PATH_MAX);
    if (mu_button(ctx, "Import")) ImportRegions(filename);
    if (mu_button(ctx, "Export")) ExportRegions(filename);
}

void RPRenderUnderlay(Viewport *v){}

void RPRender(Viewport *v){
    int yOffset = DEFAULT_FONT_SIZE;
    bool dark = false;
    for (Region *r = regions.head; r != NULL; r = r->next){
        if (dark) DrawRectangle(0, yOffset, v->size.width, DEFAULT_FONT_SIZE*4+10, VIEWPORT_GRID_C);
        yOffset += (DEFAULT_FONT_SIZE*4) + 10;
        dark = !dark;
    }
    
    DrawRectangle(mouseHoverRect.x, mouseHoverRect.y, mouseHoverRect.width, mouseHoverRect.height, FILE_HOVER_C);

    yOffset = DEFAULT_FONT_SIZE;
    for (Region *r = regions.head; r != NULL; r = r->next){
        DrawTextCustom2((Vector2){5,yOffset},"%s",r->name);
        DrawTextCustom2((Vector2){85,yOffset},"w:%.2f \nh:%.2f \nx:%.2f \ny:%.2f",r->rect.width, r->rect.height, r->rect.x, r->rect.y);
        DrawTextCustom2((Vector2){165,yOffset},"x:%.2f \ny:%.2f",r->pointA.x,r->pointA.y);
        DrawTextCustom2((Vector2){235,yOffset},"x:%.2f \ny:%.2f",r->pointB.x,r->pointB.y);
        yOffset += (DEFAULT_FONT_SIZE*4) + 10;
    }
}

void RPRenderOverlay(Viewport *v){
    DrawRectangle(0, 0, v->size.width, DEFAULT_FONT_SIZE, CMD_BAR_C);
    DrawTextCustom2((Vector2){5,0}, "%s", "Name:");
    DrawTextCustom2((Vector2){85,0}, "%s", "Rect:");
    DrawTextCustom2((Vector2){165,0}, "%s", "A:");
    DrawTextCustom2((Vector2){235,0}, "%s", "B:");

    DrawTextInCorner(v,CMD_BAR_C,"LClick:Apply | RClick:Remove");
}