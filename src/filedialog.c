#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include "utils.h"
#include "viewports.h"
#include "config.h"

static Viewport *explorer;
static Vector2 mousePosition;
static char PWD[PATH_MAX];
static char **files;
static int filesQ = 0;
static Rectangle hoverFileRect;
static float scroll = 0;
static char *outBuffer;
static int outBufferLen;

static const char *commands[] = {
    NULL
};

void OpenDir(char *path){
    DIR *dir;
    struct dirent *entry;
    
    // Open the directory
    if (chdir(path) != 0){
        PushLog("Unable to set '%s' as pwd",path);
        return;
    }
    
    dir = opendir(".");
    if (dir == NULL) {
        PushLog("Unable to open directory: %s",path);
        chdir(PWD);
        return;
    }

    getcwd(PWD,PATH_MAX);

    // Release previous memory
    if (files != NULL){
        for (int i=0; i<filesQ; i++)
            free(files[i]);
        free(files);
    }
    
    // Count the files on the dir
    filesQ = 0;
    while ((entry = readdir(dir)) != NULL)
        filesQ++;
    
    files = calloc(filesQ,sizeof(char*));

    rewinddir(dir);
    int i=0;
    while ((entry = readdir(dir)) != NULL) {
        int strLen = strlen(entry->d_name);
        files[i] = calloc(strLen+2,sizeof(char));
        strcpy(files[i++], entry->d_name);
    }

    closedir(dir);
    scroll = 0;
}

void OpenExplorer(char *out, int len){
    outBuffer = out;
    outBufferLen = len;
    if (explorer->hidden){
        ToggleViewport(explorer);
    }
}

/* <== Callbacks ======================================> */

void FileDialogInit(Viewport *v){
    explorer = v;
    getcwd(PWD, PATH_MAX);
    OpenDir(".");
    v->camera.offset = (Vector2){0,0};
}

void FileDialogUpdate(Viewport *v){
    mousePosition = GetMouseViewportPosition(v);
    int yOffset = DEFAULT_FONT_SIZE;
    for (int i=0; i<filesQ; i++){
        Rectangle fileRect = (Rectangle){0,yOffset,v->size.width, DEFAULT_FONT_SIZE+2};
        if (IsPointOnRect(mousePosition,fileRect)){
            hoverFileRect = fileRect;
            // NAVIGATION
            if (IsMouseButtonPressedFocusSafe(MOUSE_LEFT_BUTTON)){
                hoverFileRect = (Rectangle){0,0,0,0};
                OpenDir(files[i]);
                break;
            }

            // SELECTION
            if (IsMouseButtonPressedFocusSafe(MOUSE_RIGHT_BUTTON)){
                snprintf(outBuffer, outBufferLen, "%s/%s", PWD, files[i]);
                ToggleViewport(v);
                return;
            }
        }
        yOffset += DEFAULT_FONT_SIZE + 2;
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

const char **FileDialogGetCommands(){
    return commands;
}

void FileDialogExecCmd(Viewport *v, int argc, char **argv){
}

void FileDialogRender(Viewport *v){
    if (filesQ <= 0) return;
    DrawRectangle(hoverFileRect.x, hoverFileRect.y, hoverFileRect.width, hoverFileRect.height, FILE_HOVER_C);
    int yOffset = DEFAULT_FONT_SIZE;
    for (int i=0; i<filesQ; i++){
        DrawTextEx(inconsolata,files[i],(Vector2){5,yOffset},DEFAULT_FONT_SIZE,2,TEXT_C);
        yOffset += DEFAULT_FONT_SIZE + 2;
    }

}

void FileDialogRenderOverlay(Viewport *v){
    DrawRectangle(0, 0, v->size.width, DEFAULT_FONT_SIZE, CMD_BAR_C);
    DrawTextEx(inconsolata,PWD,(Vector2){5,0},DEFAULT_FONT_SIZE,2,TEXT_C);
    DrawTextInCorner(v,(Color){0,0,0,0},"LClick:Navigate  |  RClick:Select");
}