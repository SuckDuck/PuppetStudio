#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "config.h"
#include "microui.h"
#include "raylib.h"
#include "raymath.h"
#include "viewports.h"
#include "puppets.h"
#include "theater.h"
#include "utils.h"
#include "mjpegw.h"

#define FORCE_CLOSE_IF_PLAYING (state == PLAYING_ANIMATION ? MU_OPT_FORCE_CLOSE : 0)
#define TIMELINE_FRAME_DISTANCE 10
#define TIMELINE_HEIGHT 100
#define TIMELINE_SCROLLBAR_HEIGHT 10

typedef enum State {
    IDLE,
    MOVING_PUPPET,
    MOVING_BONE,
    MOVING_CAMERA,
    ROTATING_CAMERA,
    RESIZING_CAMERA,
    PLAYING_ANIMATION,
    ON_TIMELINE,
    MOVING_SCROLLBAR
} State;

typedef enum VideoFormat {
    MJPEG_AVI
} VideoFormats;

typedef enum CameraModes {
    CAMERA_EDITOR_MODE,
    CAMERA_PREVIEW_MODE
} CameraModes;

static Viewport *thisViewport;
static State state;
static Vector2 mousePosition;
static Vector2 mousePositionOverlay;
static Vector2 grabOffset;
static int renderBones;
static int autoMovementSettings = 1;
static int blockRange = 1;
static int propagateRotation = 1;
static float frameDelay = 120.0; //ms
static float frameTimer;
static float frameTimerTarget;
static int animationLoop = 1;
static CameraModes cameraMode;
static Camera2D savedEditorCamera;
static Vector2 virtualCameraCorners[5];
static int onionSkinsTrace = 3;
static float timelineOffset = 0;
static int timelineHoverFrame = -1;
static int scrollbarThumbWidth = 0;
static int scrollbarThumboOffset = 0;
static int frameToCopy = -1;
static Puppet *puppetsToDelete[16] = {0};
static int puppetsToDeleteQ = 0;
static VirtualCameraSnapshot copiedCamera = {.zoom = 1};
static Color copiedColor = {51, 51, 54, 255};
static const char *commands[] = {
    NULL
};

PuppetLinkedList puppetsCache;
Timeline timeline = {0};
Puppet *theatreTargetPuppet;
Bone *theatreTargetBone;
VirtualCamera camera;
VideoFormats outputFormat;

/* <== Utilities ======================================> */

bool PupppetIsOnList(Puppet *puppet, PuppetLinkedList *list){
    for (Puppet *p = list->head; p != NULL; p = p->next){
        if (p == puppet) return  true;
    }
    return false;
}

bool PuppetNameIsOnList(char *name, PuppetLinkedList *list){
    for (Puppet *p = list->head; p != NULL; p = p->next){
        if (strcmp(name, p->name) == 0) return true;
    }
    return false;
}

Puppet *GetPuppetByName(char *name, PuppetLinkedList *list){
    for (Puppet *p = list->head; p != NULL; p = p->next){
        if (strcmp(name, p->name) == 0) return p;
    }
    return NULL;
}

void NewFrame(Timeline *t, bool copylast){
    Frame *f = calloc(1,sizeof(Frame));
    f->cameraPos.zoom = 1;
    if (t->currentFrame == NULL){
        f->bgColor[0] = f->bgColor[1] = 51;
        f->bgColor[2] = 54;
    }
    else {
        f->bgColor[0] = timeline.currentFrame->bgColor[0];
        f->bgColor[1] = timeline.currentFrame->bgColor[1];
        f->bgColor[2] = timeline.currentFrame->bgColor[2];
    }
    
    // LINK THE LIST
    if (t->tail != NULL){
        if (t->tail == t->currentFrame){
            t->tail->next = f;
            f->prev = t->tail;
            t->tail = f;
        }
        
        else {
            f->next = t->currentFrame->next;
            f->prev = t->currentFrame;
            t->currentFrame->next = f;
            f->next->prev = f;
        }
    }
    
    // FIRST ELEMENT
    if (t->head == NULL){
        t->head = t->tail = f;
    }

    t->frameCount++;
    if (t->currentFrameIndex < 0){
        t->currentFrameIndex = 0;
        t->currentFrame = t->head;
    }
    else{
        if (t->currentFrame->next != NULL && copylast)
            CopyFrame(t->currentFrame, t->currentFrame->next);
        SwitchFrame(t->currentFrameIndex+1, t);
    }
}

PuppetSnapshot *PuppetIsOnFrame(Puppet *p, Frame *f){
    for (PuppetSnapshot *s=f->head; s != NULL; s = s->next){
        if (s->puppet == p)
            return s;
    }
    return NULL;
}

void CalculateBoundaries(PuppetSnapshot *p){
    if (p == NULL) return;
    
    float rightMargin = 0, leftMargin = 0, topMargin = 0, bottomMargin = 0;
    for (int i=0; i<p->puppet->descendantsQ; i++){
        Bone *b = p->puppet->descendants[i];
        
        float scale = (b->len*b->root->scale) / Vector2Length(Vector2Subtract(b->skin.pointA, b->skin.pointB));
        Rectangle dst = (Rectangle){
            b->parent->position.x,
            b->parent->position.y,
            b->skin.rect.width * scale,
            b->skin.rect.height * scale
        };

        Vector2 A = b->skin.pointA;
        Vector2 B = b->skin.pointB;
        Vector2 org = (Vector2){
            (A.x - b->skin.rect.x)*scale,
            (A.y - b->skin.rect.y)*scale
        };

        float angle = VectorToDegrees(b->direction)-b->skin.angle;
        Vector2 corners[4];
        GetRectCorners(
            dst, 
            org, 
            1, 
            angle, 
            &corners[0], 
            &corners[1], 
            &corners[2], 
            &corners[3]
        );

        for (int o=0; o<4; o++){
            float hDelta = corners[o].x - p->puppet->position.x;
            if (hDelta > rightMargin) rightMargin = hDelta;
            if (hDelta < leftMargin) leftMargin = hDelta;

            float vDelta = corners[o].y - p->puppet->position.y;
            if (vDelta > bottomMargin) bottomMargin = vDelta;
            if (vDelta < topMargin) topMargin = vDelta;
        }
    }

    p->boundaries.height = bottomMargin + topMargin*-1;
    p->boundaries.width = rightMargin + leftMargin*-1;
    p->boundaries.y = (p->position.y + topMargin);
    p->boundaries.x = (p->position.x + leftMargin);
}

void GenerateOnionSkin(PuppetSnapshot *p){
    if (p->onionSkin.id != 0){
        UnloadRenderTexture(p->onionSkin);
    }
    
    CalculateBoundaries(p);
    p->onionSkin = LoadRenderTexture(p->boundaries.width, p->boundaries.height);
    BeginTextureMode(p->onionSkin);
        ClearBackground((Color){0,0,0,0});
        DrawPuppetSkinTo(
            p->puppet,
            Vector2Subtract(
                p->puppet->position,
                (Vector2){
                    p->boundaries.x,
                    p->boundaries.y
                }
            )
        );
    EndTextureMode();
}

void DrawOnionSkin(PuppetSnapshot *s, float opacity){
    if (s == NULL) return;
    if (s->onionSkin.id == 0) return;
    if (opacity <= 0) return;

    DrawTextureRec(
        s->onionSkin.texture,
        (Rectangle){
            0,0,
            s->boundaries.width,
            s->boundaries.height*-1
        },
        (Vector2){
            s->boundaries.x, 
            s->boundaries.y
        },
        (Color){255,255,255,opacity} );
    EndBlendMode();
}

void DeleteBoneSnapshot(BoneSnapshot *s, BoneSnapshotLinkedList *list){
    if (s == NULL) return;
    if (list == NULL) return;
    if (s == list->head) list->head = s->next;
    if (s == list->tail) list->tail = s->prev;
    if (s->prev != NULL) s->prev->next = s->next;
    if (s->next != NULL) s->next->prev = s->prev;
    free(s);
    list->snapshotsQ--;
}

void DeletePuppetSnapshot(PuppetSnapshot *s, Frame *list){
    if (s == NULL) return;
    if (list == NULL) return;
    for (BoneSnapshot *bs = s->bonesSnapshots.head; bs != NULL; bs = bs->next)
        if (bs->prev != NULL) DeleteBoneSnapshot(bs->prev, &s->bonesSnapshots);
    DeleteBoneSnapshot(s->bonesSnapshots.tail, &s->bonesSnapshots);

    if (s == list->head) list->head = s->next;
    if (s == list->tail) list->tail = s->prev;
    if (s->prev != NULL) s->prev->next = s->next;
    if (s->next != NULL) s->next->prev = s->prev;
    free(s);
    list->snapshotsQ--;
}

void CleanFrame(Frame *f){
    for (PuppetSnapshot *p=f->head; p!=NULL; p=p->next)
        if (p->prev != NULL) DeletePuppetSnapshot(p->prev, f);
    if (f->tail != NULL) DeletePuppetSnapshot(f->tail, f);
}

void NewBoneSnapshot(Bone *b, PuppetSnapshot *p){
    // if there is an snapshot of this bone already
    for (BoneSnapshot *s=p->bonesSnapshots.head; s != NULL; s = s->next){
        if (s->bone == b){
            DeleteBoneSnapshot(s, &p->bonesSnapshots);
            break;
        }
    }

    BoneSnapshot *s = calloc(1, sizeof(BoneSnapshot));
    s->bone = b;
    s->direction = b->direction;
    s->length = b->len;
    s->skin = b->skin;

    // LINK THE LIST
    if (p->bonesSnapshots.tail != NULL){
        p->bonesSnapshots.tail->next = s;
        s->prev = p->bonesSnapshots.tail;
        p->bonesSnapshots.tail = s;
    }
    
    if (p->bonesSnapshots.head == NULL){
        p->bonesSnapshots.head = p->bonesSnapshots.tail = s;
    }

    p->bonesSnapshots.snapshotsQ++;
}

void NewPuppetSnapshot(Puppet *p, Frame *f){
    if (p->root != NULL) p = p->root;
    
    // if there is an snapshot of this puppet already
    for (PuppetSnapshot *s=f->head; s != NULL; s = s->next){
        if (s->puppet == p){
            DeletePuppetSnapshot(s,f);
            break;
        }
    }

    PuppetSnapshot *s = calloc(1,sizeof(PuppetSnapshot));
    s->puppet = p;
    s->position = p->position;
    s->scale = p->scale;

    // GENERATES THE BONES SNAPSHOTS
    for (int i=0; i<p->descendantsQ; i++){
        Bone *b = p->descendants[i];
        NewBoneSnapshot(b, s);
    }
    
    // LINK THE LIST
    if (f->tail != NULL){
        f->tail->next = s;
        s->prev = f->tail;
        f->tail = s;
    }
    
    if (f->head == NULL){
        f->head = f->tail = s;
    }

    GenerateOnionSkin(s);
    f->snapshotsQ++;
}

void ApplyPuppetSnapshot(PuppetSnapshot *p){
    p->puppet->position = p->position;
    p->puppet->scale = p->scale;
    for (BoneSnapshot *s = p->bonesSnapshots.head; s != NULL; s = s->next){
        s->bone->direction = s->direction;
        s->bone->len = s->length;
        s->bone->skin = s->skin;
    }
}

void ApplyCameraSnapshot(VirtualCameraSnapshot *s){
    if (s == NULL) return;
    if (!cameraMode == CAMERA_PREVIEW_MODE) return;
    thisViewport->camera.target = (Vector2){s->x, s->y};
    thisViewport->camera.zoom = s->zoom;
    thisViewport->camera.rotation = s->rotation;
}

void UpdateVirtualCameraCorners(VirtualCameraSnapshot *camPos, Vector2 *points){
    // mantain the virtual camera preview frame updated (mainly for rotation)
    Rectangle cameraRect = (Rectangle){
        camPos->x,
        camPos->y,
        camera.w,
        camera.h
    };

    GetRectCorners(
        cameraRect,
        (Vector2){
            cameraRect.width/2,
            cameraRect.height/2}, 
        camPos->zoom, 
        camPos->rotation, 
        &points[0], 
        &points[1], 
        &points[2], 
        &points[3]
    );
    points[4] = points[0];
}

void SwitchFrame(int frame, Timeline *t){
    if (t->frameCount <= 0) return;
    if (frame >= t->frameCount) return;
    if (frame < 0) return;

    if (frame > t->currentFrameIndex){
        for (Frame *f = t->currentFrame; f != NULL; f = f->next){
            if (t->currentFrameIndex == frame){
                t->currentFrame = f;
                break;
            }

            t->currentFrameIndex++;
        }
    }

    if (frame < t->currentFrameIndex){
        for (Frame *f = t->currentFrame; f != NULL; f = f->prev){
            if (t->currentFrameIndex == frame){
                t->currentFrame = f;
                break;
            }

            t->currentFrameIndex--;
        }
    }

    
    ApplyCameraSnapshot(&timeline.currentFrame->cameraPos);
    UpdateVirtualCameraCorners(&timeline.currentFrame->cameraPos, virtualCameraCorners);
    for (PuppetSnapshot *s = t->currentFrame->head; s != NULL; s = s->next){
        ApplyPuppetSnapshot(s);
        UpdateDescendantsPos(s->puppet, s->puppet->position, true);
    }
}

void CopyFrame(Frame *src, Frame *dst){
    if (src == NULL || dst == NULL) return;
    CleanFrame(dst);
    dst->cameraPos = src->cameraPos;

    for (PuppetSnapshot *srcp = src->head; srcp != NULL; srcp = srcp->next){
        PuppetSnapshot *newp = calloc(1,sizeof(PuppetSnapshot));
        newp->puppet = srcp->puppet;
        newp->position = srcp->position;
        newp->scale = srcp->scale;
        
        int bonesCount = 0;
        for (BoneSnapshot *srcb = srcp->bonesSnapshots.head; srcb != NULL; srcb = srcb->next){
            bonesCount++;
            BoneSnapshot *newb = calloc(1,sizeof(BoneSnapshot));
            newb->bone = srcb->bone;
            newb->direction = srcb->direction;
            newb->length = srcb->length;
            newb->skin = srcb->skin;

            // LINK THE BONE SNAPSHOTS LIST
            if (newp->bonesSnapshots.tail == NULL){
                newp->bonesSnapshots.head = newb;
            }
            else{
                newp->bonesSnapshots.tail->next = newb;
                newb->prev = newp->bonesSnapshots.tail;
            }
            newp->bonesSnapshots.tail = newb;
        }

        newp->bonesSnapshots.snapshotsQ = bonesCount;

        // LINK THE PUPPET SNAPSHOTS LIST
        if (dst->tail == NULL){
            dst->head = newp;
        }
        else{
            dst->tail->next = newp;
            newp->prev = dst->tail;
        }
        dst->tail = newp;
        dst->snapshotsQ =  src->snapshotsQ;
        dst->bgColor[0] = src->bgColor[0];
        dst->bgColor[1] = src->bgColor[1];
        dst->bgColor[2] = src->bgColor[2];
    }

}

void RemoveFrame(Frame *f, Timeline *t){
    if (f == NULL) return;
    if (f->prev == NULL && f->next == NULL) return;
    
    if (f == t->currentFrame){
        if (f->next != NULL){
            SwitchFrame(t->currentFrameIndex+1, t);
            t->currentFrameIndex--;
        }
        else SwitchFrame(t->currentFrameIndex-1, t);
    }

    for (PuppetSnapshot *p=f->head; p != NULL; p = p->next)
        DeletePuppetSnapshot(p->prev, f);
    DeletePuppetSnapshot(f->tail, f);

    if (f == t->head) t->head = f->next;
    if (f == t->tail) t->tail = f->prev;
    if (f->prev != NULL) f->prev->next = f->next;
    if (f->next != NULL) f->next->prev = f->prev;
    free(f);

    t->frameCount--;
    if (t->currentFrameIndex >= t->frameCount)
        t->currentFrameIndex = t->frameCount-1;

    if (frameToCopy >= t->currentFrameIndex)
        frameToCopy = -1;
    
}

void CopyPuppetToList(Puppet *puppet, PuppetLinkedList *list, char* name){
    RebuildDescendants(puppet);
    RebuildDescendantsIndex(puppet);
    
    Puppet *newPuppet = NewPuppet();
    Bone *bones[256];
    bones[0] = newPuppet;


    for (int i=0; i<puppet->descendantsQ; i++){
        Bone *b = puppet->descendants[i];
        bones[i+1] = AddBoneVector(bones[b->parent->index],b->direction,b->len, b->range, b->skin.zIndex,b->skin);
    }
    
    RebuildDescendants(newPuppet);
    RebuildDescendantsIndex(newPuppet);
    UpdateDescendantsPos(newPuppet, newPuppet->position, true);
    
    newPuppet->atlas = puppet->atlas;
    newPuppet->atlas->refCount++;
    
    newPuppet->name = calloc(strlen(name)+1, sizeof(char));
    strcpy(newPuppet->name,name);
    
    // LINK THE LIST
    if (list->tail != NULL){
        list->tail->next = newPuppet;
        newPuppet->prev = list->tail;
        list->tail = newPuppet;
    }
    
    if (list->head == NULL){
        list->head = list->tail = newPuppet;
    }

    list->puppetsQ++;
}

void RemovePuppetFromList(Puppet *puppet, PuppetLinkedList *list){
    if (puppet == NULL) return;
    if (puppet == list->head) list->head = puppet->next;
    if (puppet == list->tail) list->tail = puppet->prev;
    if (puppet->prev != NULL) puppet->prev->next = puppet->next;
    if (puppet->next != NULL) puppet->next->prev = puppet->prev;
    DeletePuppet(puppet);
    list->puppetsQ--;
}

void RemovePuppetFromCache(Puppet *p){
    if (p == NULL) return;
    if (p == theatreTargetBone) theatreTargetBone = NULL;
    for (Frame *f = timeline.head; f != NULL; f = f->next){
        for (PuppetSnapshot *s = f->head; s != NULL; s = s->next){
            if (s->prev != NULL && s->prev->puppet == p){
                DeletePuppetSnapshot(s->prev, f);
            }
        }

        if (f->tail != NULL && f->tail->puppet == p)
            DeletePuppetSnapshot(f->tail, f);
    }

    RemovePuppetFromList(p, &puppetsCache);
}

static bool IsMouseInTimeline(){
    Rectangle timelineRect = (Rectangle){
        0,
        (thisViewport->size.height*-1)-TIMELINE_HEIGHT-TIMELINE_SCROLLBAR_HEIGHT,
        thisViewport->size.width,
        TIMELINE_HEIGHT+TIMELINE_SCROLLBAR_HEIGHT
    };
    
    return IsPointOnRect(mousePositionOverlay, timelineRect);
}

static void SetTimelineOffset(int frameIndex){
    int timelineY = (thisViewport->size.height*-1)-TIMELINE_HEIGHT;
    int frameMargin = TIMELINE_FRAME_DISTANCE;
    int frameDimension = TIMELINE_HEIGHT - TIMELINE_FRAME_DISTANCE*2;
    
    float allFramesWidth = (frameDimension+TIMELINE_FRAME_DISTANCE) * (timeline.frameCount);
    if (allFramesWidth > thisViewport->size.width){
        timelineOffset = frameIndex * (frameDimension+TIMELINE_FRAME_DISTANCE) *-1;
        float minMargin = (allFramesWidth-thisViewport->size.width+TIMELINE_FRAME_DISTANCE) *-1;
        if (timelineOffset < minMargin) timelineOffset = minMargin;
        if (timelineOffset > 0) timelineOffset = 0;
    }
    else timelineOffset = 0;
}

static void OffsetUpdate(Viewport *v){
    v->camera.offset = (Vector2){
        v->size.width/2,
        (v->size.height*-1-TIMELINE_HEIGHT - TIMELINE_SCROLLBAR_HEIGHT)/2
    };
}

static void CleanProject(){
    //Delete every frame (except first one)
    for (Frame *f=timeline.tail; f != NULL; f=f->prev)
        if (f->next !=  NULL) RemoveFrame(f, &timeline);
    
    //Delete every puppet
    for (Puppet *p=puppetsCache.head; p!=NULL; p=p->next)
        if (p->prev != NULL) RemovePuppetFromCache(p->prev);
    if (puppetsCache.tail != NULL) RemovePuppetFromCache(puppetsCache.tail);

    timeline.currentFrame->cameraPos = (VirtualCameraSnapshot){0,0,1,0};
    UpdateVirtualCameraCorners(&timeline.currentFrame->cameraPos, virtualCameraCorners);
    timeline.currentFrame->bgColor[0] =  51;
    timeline.currentFrame->bgColor[1] =  51;
    timeline.currentFrame->bgColor[2] =  54;

    frameToCopy = -1;
}

static int SaveProject(PuppetLinkedList *puppets, char *filename){
    if (puppets->puppetsQ <= 0 ){
        PushLog("There are no puppets in project!");
        return -1;
    }

    if (!IsFileExtension(filename, ".stage")){
        PushLog("filename should contain the '.stage' extension");
        return -2;
    }

    char dirName[PATH_MAX];
    strcpy(dirName, GetDirectoryPath(filename));
    char path[PATH_MAX] = {0};

    // create the puppets directory
    sprintf(path, "%s/%s", dirName, "puppets");
    RemoveDir(path);
    if (mkdir(path,0777) != 0 && errno != EEXIST){
        PushLog("Project puppets dir could not be created");
        return -3;
    }
        
    // create each puppet directoy and save each puppet
    for (Puppet *p=puppets->head; p != NULL; p = p->next){
        memset(path, '\0', PATH_MAX);
        sprintf(path, "%s/%s/%s", dirName, "puppets", p->name);
        if (mkdir(path,0777) != 0 && errno != EEXIST){
            PushLog("'%s' puppet folder could not be created", path);
            return -4;
        }

        memset(path, '\0', PATH_MAX);
        sprintf(path, "%s/%s/%s/%s.puppet", dirName, "puppets", p->name, p->name);
        if (SavePuppet(p, path) != 0){
            PushLog("'%s' puppet could not be created", path);
            return -5;
        }
    }

    int fd = open(filename,O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd < 0){
        return fd;
    }
    
    //save version
    write(fd,"PUPPET_STUDIO_V",sizeof(char)*15);

    int version = PROJECT_VERSION;
    write(fd,&version,sizeof(int));
    write(fd, &camera, sizeof(VirtualCamera));
    write(fd, &frameDelay, sizeof(float));

    int framesQ = timeline.frameCount;
    write(fd, &framesQ, sizeof(int));

    for (Frame *f=timeline.head; f != NULL; f = f->next){ //write each frame
        write(fd, &f->cameraPos, sizeof(VirtualCameraSnapshot));
        write(fd, f->bgColor, sizeof(float)*3);
        write(fd, &f->snapshotsQ, sizeof(int));

        // write puppets Q
        for (PuppetSnapshot *s=f->head; s != NULL; s=s->next){
            int nameLen = strlen(s->puppet->name); //write each puppet
            write(fd, &nameLen, sizeof(int));
            write(fd, s->puppet->name, nameLen);
            write(fd, &s->position, sizeof(Vector2));
            write(fd, &s->scale, sizeof(float));
            write(fd, &s->bonesSnapshots.snapshotsQ, sizeof(int)); //write each bone       
            for (BoneSnapshot *b=s->bonesSnapshots.head; b != NULL; b=b->next){
                write(fd, &b->bone->index, sizeof(int));
                write(fd, &b->direction, sizeof(Vector2));
                write(fd, &b->length, sizeof(float));
                write(fd, &b->skin, sizeof(Skin));
            }
        }
    }
    
    close(fd);
    chmod(filename,0666);
    PushLog("Project '%s' succesfully saved!", filename);

    return 0;
}

void GenerateAllOnionSkins(Frame *f){
    for (PuppetSnapshot *s=f->head; s!=NULL; s=s->next){
        GenerateOnionSkin(s);
    }
}

int LoadProject(char *filename){
    if (!IsFileExtension(filename, ".stage")){
        PushLog("filename should contain the '.stage' extension");
        return -1;
    }

    int fd = open(filename, O_RDONLY);
    if (fd < 0){
        PushLog("Project filename is invalid!");
        return -2;
    }

    //I should add some corroboration here
    char header[15] = {0};
    read(fd, &header, sizeof(char)*15);
    
    //Because of future changes, maybe...
    int version;
    read(fd, &version, sizeof(int));

    // Load each puppet
    char dirName[PATH_MAX];
    strcpy(dirName, GetDirectoryPath(filename));
    char path[PATH_MAX] = {0};
    
    sprintf(path, "%s/%s", dirName, "puppets");    
    DIR *allPuppetsDir = opendir(path);
    struct dirent *entry;

    if (allPuppetsDir == NULL){
        PushLog("Unable to open project's puppet directory");
        close(fd);
        return -3;
    }

    CleanProject();
    while ((entry = readdir(allPuppetsDir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0)   continue;
        if (strcmp(entry->d_name, "..") == 0)  continue;
        
        sprintf(path, "%s/%s/%s/%s.puppet", dirName, "puppets", entry->d_name, entry->d_name);
        Puppet *newPuppet = LoadPuppet(path);
        CopyPuppetToList(newPuppet, &puppetsCache,entry->d_name);
        DeletePuppet(newPuppet);
    }

    closedir(allPuppetsDir);

    read(fd, &camera, sizeof(VirtualCamera));
    read(fd, &frameDelay, sizeof(float));

    // For each frame
    int framesQ;
    read(fd, &framesQ, sizeof(int));
    for (int k=0; k<framesQ; k++){
        if (k>0) NewFrame(&timeline, false);

        read(fd, &timeline.currentFrame->cameraPos, sizeof(VirtualCameraSnapshot));
        read(fd, timeline.currentFrame->bgColor, sizeof(float)*3);

        // For each snapshot in frame
        int snapshotsQ;
        read(fd, &snapshotsQ, sizeof(int));
        timeline.currentFrame->snapshotsQ = snapshotsQ;
        for (int q=0; q<snapshotsQ; q++){
            PuppetSnapshot *newPuppetSnapshot = calloc(1, sizeof(PuppetSnapshot));

            int nameLen = 0;
            read(fd, &nameLen, sizeof(int));
            char puppetName[nameLen+1];
            read(fd, puppetName, nameLen);
            puppetName[nameLen] = '\0';
            newPuppetSnapshot->puppet = GetPuppetByName(puppetName, &puppetsCache);
            read(fd, &newPuppetSnapshot->position, sizeof(Vector2));
            read(fd, &newPuppetSnapshot->scale,  sizeof(float));

            // For each boneSnapshot in the puppetSnapshot
            int boneSnapshotsQ;
            read(fd, &boneSnapshotsQ,  sizeof(int));
            newPuppetSnapshot->bonesSnapshots.snapshotsQ = boneSnapshotsQ;
            for (int o=0; o<boneSnapshotsQ; o++){
                BoneSnapshot *newBoneSnapshot = calloc(1, sizeof(BoneSnapshot));
                int boneIndex;
                read(fd, &boneIndex, sizeof(int));
                boneIndex--;
                newBoneSnapshot->bone = newPuppetSnapshot->puppet->descendants[boneIndex];
                read(fd, &newBoneSnapshot->direction, sizeof(Vector2));
                read(fd, &newBoneSnapshot->length, sizeof(float));
                read(fd, &newBoneSnapshot->skin, sizeof(Skin));

                //link the boneSnapshots list
                if (newPuppetSnapshot->bonesSnapshots.tail != NULL){
                    newPuppetSnapshot->bonesSnapshots.tail->next = newBoneSnapshot;
                    newBoneSnapshot->prev = newPuppetSnapshot->bonesSnapshots.tail;
                    newPuppetSnapshot->bonesSnapshots.tail = newBoneSnapshot;
                }
                
                if (newPuppetSnapshot->bonesSnapshots.head == NULL){
                    newPuppetSnapshot->bonesSnapshots.head = newPuppetSnapshot->bonesSnapshots.tail = newBoneSnapshot;
                }
            }

            //link the puppetSnapshots list
            if (timeline.tail->tail != NULL){
                timeline.tail->tail->next = newPuppetSnapshot;
                newPuppetSnapshot->prev = timeline.tail->tail;
                timeline.tail->tail = newPuppetSnapshot;
            }
            
            if (timeline.tail->head == NULL){
                timeline.tail->head = timeline.tail->tail = newPuppetSnapshot;
            }
        }
    }

    // Generate all onion skins
    for (int i=0; i<framesQ; i++){
        SwitchFrame(i, &timeline);
        for (PuppetSnapshot *s = timeline.currentFrame->head; s != NULL; s = s->next){
            GenerateOnionSkin(s);
        }
    }

    close(fd);
    SwitchFrame(0, &timeline);
    thisViewport->camera.target = (Vector2){timeline.currentFrame->cameraPos.x, timeline.currentFrame->cameraPos.y};
    return 0;
}

static void RenderProjectMjpegAvi(Image *frames, int framesQ, char *fiename){
    struct mjpegw_context *ctx = mjpegw_open(fiename, camera.w, camera.h, 1000.0/frameDelay, NULL);
    for (int i=0; i<framesQ; i++){
        ImageFormat(&frames[i], PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        mjpegw_add_frame(ctx, frames[i].data, 3);
    }
    mjpegw_close(ctx);
}

static void RenderProject(VideoFormats format, char *filename){
    switch (format){
        case MJPEG_AVI: 
        if (!IsFileExtension(filename, ".avi")){
            PushLog("filename should contain the '.avi' extension");
            return;
        }
    }

    RenderTexture framebuffer = LoadCustomRenderTexture(camera.w, camera.h);
    RenderTexture framebuffer2 = LoadCustomRenderTexture(camera.w, camera.h);
    Camera2D framebufferCamera;
    
    // CAMERA INIIALIZATION
    framebufferCamera.target = (Vector2){0,0};
    framebufferCamera.offset = (Vector2){camera.w/2, camera.h/2};
    framebufferCamera.target = (Vector2){0,0};
    framebufferCamera.rotation = 0;
    framebufferCamera.zoom = 1;
    
    Image *outputImages = calloc(timeline.frameCount, sizeof(Image));
    for (int i=0; i<timeline.frameCount; i++){
        SwitchFrame(i, &timeline);

        framebufferCamera.target = (Vector2){
            timeline.currentFrame->cameraPos.x,
            timeline.currentFrame->cameraPos.y
        };

        framebufferCamera.zoom = timeline.currentFrame->cameraPos.zoom;
        framebufferCamera.rotation = timeline.currentFrame->cameraPos.rotation;

        // DRAW SECCTION
        BeginTextureMode(framebuffer);
        BeginMode2D(framebufferCamera);
            ClearBackground((Color){
                timeline.currentFrame->bgColor[0],
                timeline.currentFrame->bgColor[1], 
                timeline.currentFrame->bgColor[2],
                255
            });

            for (PuppetSnapshot *s = timeline.currentFrame->head; s != NULL; s = s->next){
                DrawPuppetSkin(s->puppet);
            }
        EndMode2D();
        EndTextureMode();
        
        //framebuffer here is y-flipped, so...
        BeginTextureMode(framebuffer2);
            DrawTexturePro(
                framebuffer.texture, 
                (Rectangle){0,0,camera.w, camera.h}, 
                (Rectangle){0,0,camera.w, camera.h*-1}, 
                (Vector2){0,0}, 
                0, 
                WHITE
            );
        EndTextureMode();
        outputImages[i] = LoadImageFromTexture(framebuffer2.texture);
    }

    // ENCODING PHASE
    switch (format){
        case  MJPEG_AVI: RenderProjectMjpegAvi(outputImages, timeline.frameCount, filename); break;
    }
    
    // CLEAN RESOURCES
    for (int i=0; i<timeline.frameCount; i++)
        UnloadImage(outputImages[i]);
    free(outputImages);
    UnloadRenderTexture(framebuffer);
    UnloadRenderTexture(framebuffer2);
    PushLog("Project succesfully exported to: '%s'", filename);
}

static void CalcScrollBar(int *thumbSize, int *offset){
    int frameDimension = TIMELINE_HEIGHT - TIMELINE_FRAME_DISTANCE*2;
    float allFramesWidth = (frameDimension+TIMELINE_FRAME_DISTANCE) * (timeline.frameCount);
    *thumbSize = thisViewport->size.width * (thisViewport->size.width / allFramesWidth);
    *offset = (timelineOffset*-1) * (thisViewport->size.width / allFramesWidth);
}

/* <== States =========================================> */

void IdleState(Viewport *v){
    if (timeline.currentFrameIndex < 0) return;
    if (IsMouseInTimeline()){
        state = ON_TIMELINE;
        return;
    }
     
    for (PuppetSnapshot *s = timeline.currentFrame->head; s != NULL; s = s->next){
        // PUPPET ROOT
        if (IsPointOnCircle(mousePosition, s->puppet->position, HINGE_RADIUS/v->camera.zoom)){
            ChangeCursor(MOUSE_CURSOR_POINTING_HAND);
            if (IsMouseButtonPressedFocusSafe(MOUSE_BUTTON_LEFT)){
                theatreTargetBone = s->puppet;
                theatreTargetPuppet = s->puppet;
                state = MOVING_PUPPET;
                return;
            }
        }
        
        // PUPPET BONES
        for (int i=0; i<s->puppet->descendantsQ; i++){
            Bone *b = s->puppet->descendants[i];
            if (IsPointOnCircle(mousePosition, b->position, HINGE_RADIUS/v->camera.zoom)){
                ChangeCursor(MOUSE_CURSOR_POINTING_HAND);
                if (IsMouseButtonPressedFocusSafe(MOUSE_BUTTON_LEFT)){
                    theatreTargetBone = b;
                    theatreTargetPuppet = b->root;
                    state = MOVING_BONE;
                    return;
                }
            }
        }
    }

    // CAMERA MOVE
    if (IsPointOnCircle(mousePosition, virtualCameraCorners[0], HINGE_RADIUS/v->camera.zoom)){
        ChangeCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressedFocusSafe(MOUSE_BUTTON_LEFT)){
            Vector2 camPos = (Vector2){
                timeline.currentFrame->cameraPos.x,
                timeline.currentFrame->cameraPos.y
            };
            grabOffset = Vector2Subtract(virtualCameraCorners[2], camPos);
            state = MOVING_CAMERA;
            return;
        }
    }

    // CAMERA ROTATION
    if (IsPointOnCircle(mousePosition, virtualCameraCorners[2], HINGE_RADIUS/v->camera.zoom)){
        ChangeCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressedFocusSafe(MOUSE_BUTTON_LEFT)){
            Vector2 cameraPos = (Vector2){
                timeline.currentFrame->cameraPos.x,
                timeline.currentFrame->cameraPos.y
            };

            // grabOffset.x here is the original angle
            Vector2 v1 = Vector2Subtract(Vector2Lerp(virtualCameraCorners[1], virtualCameraCorners[2], 0.5), cameraPos);
            Vector2 v2 = Vector2Subtract(virtualCameraCorners[2],cameraPos);
            grabOffset.x = AngleBetweenVectors(v1, v2);
            state = ROTATING_CAMERA;
            return;
        }
    }

    // CAMERA RESIZE
    if (IsPointOnCircle(mousePosition, virtualCameraCorners[3], HINGE_RADIUS/v->camera.zoom)){
        ChangeCursor(MOUSE_CURSOR_POINTING_HAND);
        if (IsMouseButtonPressedFocusSafe(MOUSE_BUTTON_LEFT)){
            Vector2 cameraPos = (Vector2){
                timeline.currentFrame->cameraPos.x,
                timeline.currentFrame->cameraPos.y
            };

            // grabOffset :
            // x -> og_distance   y-> og_zoom
            grabOffset.x = Vector2Distance(cameraPos, mousePosition);
            grabOffset.y = timeline.currentFrame->cameraPos.zoom;
            state = RESIZING_CAMERA;
            return;
        }
    }

    // DESELECT BONE
    if (IsMouseButtonPressedFocusSafe(MOUSE_BUTTON_LEFT)){
        theatreTargetBone = theatreTargetPuppet = NULL;
        return;
    }

}

void MovingPuppetState(Viewport *v){
    ChangeCursor(MOUSE_CURSOR_POINTING_HAND);
    theatreTargetBone->position = mousePosition;
    UpdateDescendantsPos(theatreTargetBone, theatreTargetBone->position, true);
    
    if (IsMouseButtonReleasedFocusSafe(MOUSE_BUTTON_LEFT)){
        NewPuppetSnapshot(theatreTargetBone, timeline.currentFrame);
        state = IDLE;
    }
}

void MovingBoneState(Viewport *v){
    ChangeCursor(MOUSE_CURSOR_POINTING_HAND);
    if (autoMovementSettings){
        bool c = theatreTargetBone->skin.rect.width > 0;
        RotateBonesTowards(theatreTargetBone, mousePosition, false, c, c);
    }
    else RotateBonesTowards(theatreTargetBone, mousePosition, false, propagateRotation, blockRange);
    UpdateDescendantsPos(theatreTargetBone->root, theatreTargetBone->root->position, true);
    
    if (IsMouseButtonReleasedFocusSafe(MOUSE_BUTTON_LEFT)){
        NewPuppetSnapshot(theatreTargetBone, timeline.currentFrame);
        state = IDLE;
    }
}

void MovingCameraState(Viewport *v){
    ChangeCursor(MOUSE_CURSOR_POINTING_HAND);
    timeline.currentFrame->cameraPos.x = mousePosition.x + grabOffset.x;
    timeline.currentFrame->cameraPos.y = mousePosition.y + grabOffset.y;
    UpdateVirtualCameraCorners(&timeline.currentFrame->cameraPos, virtualCameraCorners);
    if (IsMouseButtonReleasedFocusSafe(MOUSE_BUTTON_LEFT)){
        state = IDLE;
    }
}

void RotatingCameraState(Viewport *v){
    ChangeCursor(MOUSE_CURSOR_POINTING_HAND);
    
    Vector2 cameraPos = (Vector2){
        timeline.currentFrame->cameraPos.x,
        timeline.currentFrame->cameraPos.y
    };
    float degrees = VectorToDegrees(Vector2Subtract(mousePosition, cameraPos));
    
    // grabOffset.x here is the original angle
    timeline.currentFrame->cameraPos.rotation = degrees-grabOffset.x;

    UpdateVirtualCameraCorners(&timeline.currentFrame->cameraPos, virtualCameraCorners);
    if (IsMouseButtonReleasedFocusSafe(MOUSE_BUTTON_LEFT)){
        state = IDLE;
    }
}

void ResizingCameraState(Viewport *v){
    ChangeCursor(MOUSE_CURSOR_POINTING_HAND);

    // grabOffset :
    // x -> og_distance   y-> og_zoom
    Vector2 cameraPos = (Vector2){
        timeline.currentFrame->cameraPos.x,
        timeline.currentFrame->cameraPos.y
    };

    float actualDistance = Vector2Distance(mousePosition, cameraPos);
    timeline.currentFrame->cameraPos.zoom = grabOffset.y*grabOffset.x / actualDistance;
    
    UpdateVirtualCameraCorners(&timeline.currentFrame->cameraPos, virtualCameraCorners);
    if (IsMouseButtonReleasedFocusSafe(MOUSE_BUTTON_LEFT)){
        state = IDLE;
    }
}

void PlayingAnimationState(Viewport *v){
    frameTimer += GetFrameTime();
    if (frameTimer >= frameTimerTarget){
        frameTimer = 0;
        if (timeline.currentFrameIndex == timeline.frameCount-1){
            if (animationLoop){
                SwitchFrame(0, &timeline);
                return;
            }
            
            v->updateAlways = false;
            state = IDLE;
            return;
        }
        SwitchFrame(timeline.currentFrameIndex+1, &timeline);
    }
}

void OnTimelineState(Viewport *v){
    if (!IsMouseInTimeline()){
        timelineHoverFrame = -1;
        state = IDLE;
        return;
    }

    int timelineY = (v->size.height*-1)-TIMELINE_HEIGHT;
    int frameMargin = TIMELINE_FRAME_DISTANCE;
    int frameDimension = TIMELINE_HEIGHT - TIMELINE_FRAME_DISTANCE*2;
    float allFramesWidth = (frameDimension+TIMELINE_FRAME_DISTANCE) * (timeline.frameCount);
    float minMargin = (allFramesWidth-v->size.width+TIMELINE_FRAME_DISTANCE) *-1;
    
    if (allFramesWidth > v->size.width){        
        if (mousePositionOverlay.y < (v->size.height*-1)-TIMELINE_HEIGHT){
            if (IsMouseButtonPressedFocusSafe(MOUSE_BUTTON_LEFT)){
                state = MOVING_SCROLLBAR;
                return;
            }
        }        
        
        timelineOffset += GetMouseWheelMove() * SCROLL_SPEED;
        if (timelineOffset < minMargin) timelineOffset = minMargin;
        if (timelineOffset > 0) timelineOffset = 0;
    }
    else timelineOffset = 0;

    for (int i=0; i<timeline.frameCount; i++){
        Rectangle frameRect = (Rectangle){
            frameMargin + timelineOffset, 
            timelineY+TIMELINE_FRAME_DISTANCE, 
            frameDimension, 
            frameDimension, 
        };

        if (IsPointOnRect(mousePositionOverlay, frameRect)){
            if (IsMouseButtonPressedFocusSafe(MOUSE_BUTTON_LEFT)){
                SwitchFrame(i, &timeline);
                return;
            }
            timelineHoverFrame = i;
        }
        
        frameMargin += frameDimension+TIMELINE_FRAME_DISTANCE;
    }

    CalcScrollBar(&scrollbarThumbWidth, &scrollbarThumboOffset);
}

void MovingScrollbarState(Viewport *v){
    if (IsMouseButtonReleasedFocusSafe(MOUSE_BUTTON_LEFT)){
        state = IDLE;
        return;
    }
    
    CalcScrollBar(&scrollbarThumbWidth, &scrollbarThumboOffset);
    int frameDimension = TIMELINE_HEIGHT - TIMELINE_FRAME_DISTANCE*2;
    float allFramesWidth = (frameDimension+TIMELINE_FRAME_DISTANCE) * (timeline.frameCount);
    float minMargin = (allFramesWidth-v->size.width+TIMELINE_FRAME_DISTANCE) *-1;
    float offsetFactor = (mousePositionOverlay.x-scrollbarThumbWidth/2)/v->size.width;
    
    timelineOffset = (allFramesWidth*offsetFactor)*-1;
    if (timelineOffset < minMargin) timelineOffset = minMargin;
    if (timelineOffset > 0) timelineOffset = 0;
}

/* <== Callbacks ======================================> */

void TheatreInit(Viewport *v){
    thisViewport = v;
    v->renderAlways = true;
    v->disableOffsetUpdate = true;
    OffsetUpdate(v);
    timeline.currentFrameIndex = -1;
    SetViewportPanelsDimensions(v, 350, 300, 0, 40);
    v->leftPanel.resizable = true;
    v->rightPanel.resizable = true;
    camera = (VirtualCamera){640,360};
    NewFrame(&timeline, false);
    UpdateVirtualCameraCorners(&timeline.currentFrame->cameraPos, virtualCameraCorners);
    OpenViewportByName("Theater");
}

void TheatreUpdate(Viewport *v){
    // [DELETE_BUTTON_WORKAROUND]
    // Deleting puppets changes the UI layout, but microui keeps the previous frame's input state.
    // This caused the "Delete" button to auto-trigger when a new widget reused the same ID.
    // We force a clean UI frame to flush the cached input before actually removing puppets.
    if (puppetsToDeleteQ > 0){
        CleanViewportUIInput(v);
        ProcessViewportUI(v);
        for (int i=0; i<puppetsToDeleteQ; i++)
            RemovePuppetFromCache(puppetsToDelete[i]);
        puppetsToDeleteQ = 0;
        memset(puppetsToDelete, 0, 16);
        v->updateAlways = false;
    }

    mousePosition = GetMouseViewportPosition(v);
    mousePositionOverlay = GetMouseOverlayPosition(v);

    if (cameraMode == CAMERA_EDITOR_MODE && state != ON_TIMELINE){
        ViewportUpdateZoom(v);
        ViewportUpdatePan(v);
    }

    switch (state){
        case IDLE:              IdleState(v);             break;
        case MOVING_PUPPET:     MovingPuppetState(v);     break;
        case MOVING_BONE:       MovingBoneState(v);       break;
        case MOVING_CAMERA:     MovingCameraState(v);     break;
        case ROTATING_CAMERA:   RotatingCameraState(v);   break;
        case RESIZING_CAMERA:   ResizingCameraState(v);   break;
        case PLAYING_ANIMATION: PlayingAnimationState(v); break;
        case ON_TIMELINE:       OnTimelineState(v);       break;
        case MOVING_SCROLLBAR:  MovingScrollbarState(v);  break;
    }

}

void TheatreOnResize(Viewport *v){
    OffsetUpdate(v);
}

const char **TheatreGetCommands(){
    return commands;
}

void TheatreExecCmd(Viewport *v, int argc, char **argv){
    if (strcmp(argv[0],"max") == 0){
        MaximizeViewport(v);
    }

    if (strcmp(argv[0], "clean") == 0){
        CleanProject();
    }

    if (strcmp(argv[0], "render") == 0){
        RenderProject(MJPEG_AVI, "output.avi");
    }
}

void TheatreLeftPanel(Viewport *v, mu_Context *ctx){
    if (mu_header_ex(ctx, "Project", ctx->style->control_font_size, MU_OPT_EXPANDED | FORCE_CLOSE_IF_PLAYING)){
        mu_layout_row(ctx, 2, (int[]) { 20, -1 }, 0);
        mu_space(ctx);
        if (mu_button(ctx, "New Project")) CleanProject();
        mu_layout_row(ctx, 5, (int[]) { 20,-150, -120, -60, -1 }, 0);
        mu_space(ctx);
        static char projectFilename[PATH_MAX];
        mu_textbox(ctx, projectFilename, PATH_MAX);
        long exploreID = 1853963465;
        mu_push_id(ctx, &exploreID, sizeof(long));
        if (mu_button(ctx, "...")) OpenExplorer(projectFilename, PATH_MAX);
        mu_pop_id(ctx);
        if (mu_button(ctx, "Save")) SaveProject(&puppetsCache, projectFilename);
        if (mu_button(ctx, "Open")) LoadProject(projectFilename);

        mu_vertical_space(ctx, 5);

        mu_layout_row(ctx, 2, (int[]) { 20,-1 }, 0);
        mu_space(ctx); mu_label(ctx, "Export video:", ctx->style->control_font_size);
        mu_layout_row(ctx, 5, (int[]) { 20,-150, -120, -1 }, 0);
        mu_space(ctx);
        static char outputVideoFilename[PATH_MAX];
        mu_textbox(ctx, outputVideoFilename, PATH_MAX);
        exploreID = 294628475829;
        mu_push_id(ctx, &exploreID, sizeof(long));
        if (mu_button(ctx, "...")) OpenExplorer(outputVideoFilename, PATH_MAX);
        mu_pop_id(ctx);
        if (mu_button(ctx, "Export")) RenderProject(outputFormat, outputVideoFilename);

        mu_layout_row(ctx, 2, (int[]) { 20,  -1 }, 0);
        mu_space(ctx);
        mu_label(ctx, "Output format:", ctx->style->control_font_size);
        mu_layout_row(ctx, 3, (int[]) { 20, 20, -1 }, 0);
        mu_space(ctx); mu_space(ctx); mu_radiobutton(ctx, "MJPEG-AVI", ctx->style->control_font_size, (int*) &outputFormat, MJPEG_AVI);
        
        
    }

    mu_vertical_space(ctx,2);

    
    /* <== Puppets Tree ===================================> */
    if (mu_header_ex(ctx, "Puppets Tree", ctx->style->control_font_size, MU_OPT_EXPANDED | FORCE_CLOSE_IF_PLAYING)){
        if (puppetsCache.head == NULL){
            mu_layout_row(ctx, 1, (int[]) {-1}, 0);
            mu_label(ctx, "There're no puppets in the project!", ctx->style->control_font_size);
            mu_label(ctx, "Go to workshop and load one!", ctx->style->control_font_size);
            mu_layout_row(ctx, 2, (int[]) {20,125}, 0);
            mu_space(ctx);
            if (mu_button(ctx, "Open Workshop")) OpenViewportByName("WorkShop");
            return;
        }

        mu_layout_row(ctx, 4, (int[]) { 20,-100,-75, -1 }, 0);
        for (Puppet *p=puppetsCache.head; p != NULL; p = p->next){
            mu_space(ctx);
            mu_label(ctx, p->name, ctx->style->control_font_size);
            
            if (timeline.currentFrame != NULL){
                mu_push_id(ctx, p->name, sizeof(char)*strlen(p->name));
                
                /*This here, is horrible and inneficient, i should cache this shit*/
                PuppetSnapshot *s = PuppetIsOnFrame(p, timeline.currentFrame);
                int showState = s != NULL;
                if (mu_showbox(ctx, "", ctx->style->control_font_size, &showState)){
                    if (!showState) DeletePuppetSnapshot(s, timeline.currentFrame);
                    else NewPuppetSnapshot(p, timeline.currentFrame);
                }
                mu_pop_id(ctx);
            }
            else mu_space(ctx);
            
            mu_push_id(ctx, p->name, sizeof(char)*strlen(p->name));
            if (mu_button(ctx, "Delete")){
                // [DELETE_BUTTON_WORKAROUND] (see TheatreUpdate)
                puppetsToDelete[puppetsToDeleteQ++] = p;
                v->updateAlways = true;
            }
            mu_pop_id(ctx);    
        }
    }

}

void TheatreRightPanel(Viewport *v, mu_Context *ctx){
    /* <== Toolbox ========================================> */
    if (mu_header_ex(ctx, "Editor", ctx->style->control_font_size, MU_OPT_EXPANDED)){
        mu_layout_row(ctx, 2, (int[]) {20,-1 }, 0);
            mu_space(ctx); mu_checkbox(ctx, "Render Bones", ctx->style->control_font_size, &renderBones);
            mu_space(ctx); mu_checkbox(ctx, "Auto Movement Settings", ctx->style->control_font_size, &autoMovementSettings);
            if (!autoMovementSettings){
                mu_space(ctx); mu_checkbox(ctx, "Block Range", ctx->style->control_font_size, &blockRange);
                mu_space(ctx); mu_checkbox(ctx, "Propagate Rotation", ctx->style->control_font_size, &propagateRotation);
            }

        mu_layout_row(ctx, 3, (int[]) {20, 122,122 }, 0);
            mu_space(ctx);
            if (mu_radiobutton(ctx, "EditorView", ctx->style->control_font_size, (int*) &cameraMode, 0)){
                v->camera = savedEditorCamera;
            }
                
            if (mu_radiobutton(ctx, "VirtualCam", ctx->style->control_font_size, (int*) &cameraMode, 1)){
                savedEditorCamera = v->camera;
                ApplyCameraSnapshot(&timeline.currentFrame->cameraPos);
                OffsetUpdate(v);
            }

        // ONION SKIN LAYERS
        mu_layout_row(ctx, 5, (int[]) {20, 60,60,60,60 }, 0);
            mu_space(ctx);
            mu_label(ctx,"Trace:",ctx->style->control_font_size);
            
            float tmp = (float) onionSkinsTrace;
            mu_number_ex(
                ctx,
                &tmp,
                0,
                "%.0f",
                ctx->style->control_font_size,
                1,
                MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT
            );
            
            if (mu_button(ctx, "-")){
                if(onionSkinsTrace > 0) onionSkinsTrace--;
            }
            
            if (mu_button(ctx, "+")){
                if(onionSkinsTrace < 5) onionSkinsTrace++;
            }
    }

    if (mu_header_ex(ctx, "Background Color", ctx->style->control_font_size, MU_OPT_EXPANDED | FORCE_CLOSE_IF_PLAYING)){
        mu_layout_row(ctx, 2, (int[]) {125, 125}, 0);
        if (mu_button(ctx, "CopyColor")){
            copiedColor = (Color){timeline.currentFrame->bgColor[0],timeline.currentFrame->bgColor[1],timeline.currentFrame->bgColor[2]};
            PushLog("Color (r:%i g:%i b:%i) copied!", copiedColor.r, copiedColor.g, copiedColor.b);
        }

        if (mu_button(ctx, "PasteColor")){
            timeline.currentFrame->bgColor[0] = copiedColor.r;
            timeline.currentFrame->bgColor[1] = copiedColor.g;
            timeline.currentFrame->bgColor[2] = copiedColor.b;
            PushLog("Color (r:%i g:%i b:%i) pasted!", copiedColor.r, copiedColor.g, copiedColor.b);
        }
        
        mu_layout_row(ctx, 2, (int[]) { -78, -1 }, 74);
        /* sliders */
        mu_layout_begin_column(ctx);
        mu_layout_row(ctx, 2, (int[]) { 46, -1 }, 0);
        
        mu_label(ctx, "Red:", ctx->style->control_font_size);
        mu_slider_ex(ctx, &timeline.currentFrame->bgColor[0], 0, 255, 0.1, "%.3f", ctx->style->control_font_size, 0);
        mu_label(ctx, "Green:", ctx->style->control_font_size);
        mu_slider_ex(ctx, &timeline.currentFrame->bgColor[1], 0, 255, 0.1, "%.3f", ctx->style->control_font_size, 0);
        mu_label(ctx, "Blue:", ctx->style->control_font_size);
        mu_slider_ex(ctx, &timeline.currentFrame->bgColor[2], 0, 255, 0.1, "%.3f", ctx->style->control_font_size, 0);
        mu_layout_end_column(ctx);
        /* color preview */
        mu_Rect r = mu_layout_next(ctx);
        mu_draw_rect(ctx, r, mu_color(timeline.currentFrame->bgColor[0], timeline.currentFrame->bgColor[1], timeline.currentFrame->bgColor[2], 255));
        char buf[32];
        sprintf(buf, "#%02X%02X%02X", (int) timeline.currentFrame->bgColor[0], (int) timeline.currentFrame->bgColor[1], (int) timeline.currentFrame->bgColor[2]);
        mu_draw_control_text(ctx, buf, ctx->style->control_font_size, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
    }

    if (mu_header_ex(ctx, "Puppet / Bone", ctx->style->control_font_size, MU_OPT_EXPANDED | FORCE_CLOSE_IF_PLAYING)){
        //TRANSFORM
        mu_layout_row(ctx, 5, (int[]) {20, 60,60,60,60 }, 0);
            if (MuNumberORNa(ctx, "PosX:", &theatreTargetPuppet->position.x, theatreTargetPuppet != NULL, true)){
                if (theatreTargetBone->root != NULL) UpdateDescendantsPos(theatreTargetBone->root, theatreTargetBone->root->position, true);
                else UpdateDescendantsPos(theatreTargetBone, theatreTargetBone->position, true);
                NewPuppetSnapshot(theatreTargetBone, timeline.currentFrame);
            }
        
            if (MuNumberORNa(ctx, "PosY:", &theatreTargetPuppet->position.y, theatreTargetPuppet != NULL, false)){
                if (theatreTargetBone->root != NULL) UpdateDescendantsPos(theatreTargetBone->root, theatreTargetBone->root->position, true);
                else UpdateDescendantsPos(theatreTargetBone, theatreTargetBone->position, true);
                NewPuppetSnapshot(theatreTargetBone, timeline.currentFrame);
            }
            
        mu_layout_row(ctx, 3, (int[]) {20, 60,60 }, 0);
            if (MuNumberORNa(ctx, "Scale:", &theatreTargetPuppet->scale, theatreTargetPuppet != NULL, true)){
                if (theatreTargetBone->root != NULL) UpdateDescendantsPos(theatreTargetBone->root, theatreTargetBone->root->position, true);
                else UpdateDescendantsPos(theatreTargetBone, theatreTargetBone->position, true);
                NewPuppetSnapshot(theatreTargetBone, timeline.currentFrame);
            }
         
        //FLIP BUTTONS
        mu_layout_row(ctx, 3, (int[]) {20, 125, 125}, 0);
            mu_space(ctx);
            if (mu_button(ctx, "FlipPuppetX") && theatreTargetBone){
                XFlipPuppet(theatreTargetBone);
                NewPuppetSnapshot(theatreTargetBone, timeline.currentFrame);
            }
            
            if (mu_button(ctx, "FlipPuppetY") && theatreTargetBone){ 
                YFlipPuppet(theatreTargetBone);
                NewPuppetSnapshot(theatreTargetBone, timeline.currentFrame);
            }

            mu_space(ctx);
            if (mu_button(ctx, "FlipBoneX") && theatreTargetBone){
                XFlipSkin(&theatreTargetBone->skin);
                NewPuppetSnapshot(theatreTargetBone, timeline.currentFrame);
            }

            if (mu_button(ctx, "FlipBoneY") && theatreTargetBone){
                YFlipSkin(&theatreTargetBone->skin);
                NewPuppetSnapshot(theatreTargetBone, timeline.currentFrame);
            }

        //Z-INDEX
        char buf[64];
        mu_layout_row(ctx, 5, (int[]) {20, 60,60,60,60 }, 0);
            mu_space(ctx);
            mu_label(ctx,"zIndex:",ctx->style->control_font_size);
            if (theatreTargetBone != NULL && theatreTargetBone->root != NULL)
                sprintf(buf,"%i", theatreTargetBone->skin.zIndex);
            else strcpy(buf, "n/a");
            mu_textbox_ex(ctx, buf, 64, ctx->style->control_font_size, MU_OPT_ALIGNCENTER | MU_OPT_NOINTERACT);
            if (mu_button(ctx, "Up")){
                MoveBoneUpZIndex(theatreTargetBone);
                NewPuppetSnapshot(theatreTargetBone, timeline.currentFrame);
            }
            if (mu_button(ctx, "Down")){
                MoveBoneDownZIndex(theatreTargetBone);
                NewPuppetSnapshot(theatreTargetBone, timeline.currentFrame);
            }

        //ANGLE
        mu_layout_row(ctx, 5, (int[]) {20, 60,60,60,60 }, 0);
            static float boneAngle = 0.0;
            MuNumberORNa(ctx, "Angle:", &boneAngle, true, true);
            
            if (mu_button(ctx, "Get")){
                if (theatreTargetBone != NULL && theatreTargetBone->root != NULL)
                    boneAngle = VectorToDegrees(theatreTargetBone->direction);
            };

            if (mu_button(ctx, "Set")){
                if (theatreTargetBone != NULL && theatreTargetBone->root != NULL){
                    RotateBonesDegrees(theatreTargetBone, boneAngle, false);
                    UpdateDescendantsPos(theatreTargetBone->root, theatreTargetBone->root->position, true);
                    NewPuppetSnapshot(theatreTargetBone, timeline.currentFrame);
                }
            }

        //LENGTH
        mu_layout_row(ctx, 5, (int[]) {20, 60, 60, 60, 60}, 0);
            if (MuNumberORNa(ctx, "Length:", &theatreTargetBone->len, theatreTargetBone != NULL && theatreTargetBone->root != NULL, true)){
                NewPuppetSnapshot(theatreTargetBone, timeline.currentFrame);
                UpdateDescendantsPos(theatreTargetBone->root, theatreTargetBone->root->position, true);
            }

            if (theatreTargetBone != NULL && theatreTargetBone->root != NULL && theatreTargetBone->len > theatreTargetBone->range){
                theatreTargetBone->len = theatreTargetBone->range;
                NewPuppetSnapshot(theatreTargetBone, timeline.currentFrame);
                UpdateDescendantsPos(theatreTargetBone->root, theatreTargetBone->root->position, true);
            }

    }
    
    if (mu_header_ex(ctx, "Camera", ctx->style->control_font_size, MU_OPT_EXPANDED | FORCE_CLOSE_IF_PLAYING)){
        mu_layout_row(ctx, 3, (int[]) {20, 125, 125}, 0);
        mu_space(ctx);
        if (mu_button(ctx, "CopyCamera")){
            copiedCamera = timeline.currentFrame->cameraPos;
            PushLog("Camera (x:%f y:%f zoom:%f rotation:%f) copied!", copiedCamera.x, copiedCamera.y, copiedCamera.zoom, copiedCamera.rotation);
        }
        
        if (mu_button(ctx, "PasteCamera")){
            timeline.currentFrame->cameraPos = copiedCamera;
            ApplyCameraSnapshot(&copiedCamera);
            UpdateVirtualCameraCorners(&copiedCamera, virtualCameraCorners);
            PushLog("Camera (x:%f y:%f zoom:%f rotation:%f) pasted!", copiedCamera.x, copiedCamera.y, copiedCamera.zoom, copiedCamera.rotation);
        }
        
        mu_layout_row(ctx, 5, (int[]) {20, 60,60,60,60 }, 0);
        if (MuNumberORNa(ctx, "CamW:", &camera.w, true, true) ||    
            MuNumberORNa(ctx, "CamH:", &camera.h, true, false) ||
            MuNumberORNa(ctx, "CamX:", &timeline.currentFrame->cameraPos.x, timeline.frameCount > 0, true) ||
            MuNumberORNa(ctx, "CamY:", &timeline.currentFrame->cameraPos.y, timeline.frameCount > 0, false) ||
            MuNumberORNa(ctx,"Zoom:",&timeline.currentFrame->cameraPos.zoom,timeline.frameCount > 0,true) ||
            MuNumberORNa(ctx,"Rotation:",&timeline.currentFrame->cameraPos.rotation,timeline.frameCount > 0,false)){
                UpdateVirtualCameraCorners(&timeline.currentFrame->cameraPos, virtualCameraCorners);
            }
    }

    if (mu_header_ex(ctx, "Animation", ctx->style->control_font_size, MU_OPT_EXPANDED)){
        mu_layout_row(ctx, 3, (int[]) {20, 125, 125}, 0);
            mu_space(ctx);
            if (mu_button(ctx, "Play")){
                frameTimer = 0;
                frameTimerTarget = frameDelay/1000;
                v->updateAlways = true;
                theatreTargetBone = theatreTargetPuppet = NULL;
                state = PLAYING_ANIMATION;
            }
            
            if (mu_button(ctx, "Stop")){ 
                frameTimer = 0;
                v->updateAlways = false;
                state = IDLE;
            }
        
        mu_layout_row(ctx, 3, (int[]) {20, 55,55},0);
            MuNumberORNa(ctx, "FrameDelay:", &frameDelay, true, true);

        mu_layout_row(ctx, 2, (int[]) {20,-1 }, 0);
            mu_space(ctx); mu_checkbox(ctx, "Loop:", ctx->style->control_font_size, &animationLoop);
    }
    
}

void TheatreBottomPanel(Viewport *v, mu_Context *ctx){
    mu_layout_row(ctx, 5, (int[]) {80, 80, 90, 90, 90}, 28);
        if (mu_button(ctx, "NewFrame")){
            timelineHoverFrame = -1;
            NewFrame(&timeline, true);
            SetTimelineOffset(timeline.currentFrameIndex);
            CalcScrollBar(&scrollbarThumbWidth, &scrollbarThumboOffset);
        }
        if (mu_button(ctx, "DelFrame")){
            timelineHoverFrame = -1;
            RemoveFrame(timeline.currentFrame, &timeline);
            SetTimelineOffset(timeline.currentFrameIndex);
            CalcScrollBar(&scrollbarThumbWidth, &scrollbarThumboOffset);
        }
        if (mu_button(ctx, "CopyFrame")){
            frameToCopy = timeline.currentFrameIndex;
        }

        if (frameToCopy > -1){
            if (mu_button(ctx, "PasteFrame")){
                int i=0;
                for (Frame *f=timeline.head; f!=NULL; f=f->next){
                    if (i==frameToCopy){
                        CopyFrame(f, timeline.currentFrame);
                        SwitchFrame(timeline.currentFrameIndex, &timeline); //to apply every snapshot
                        break;
                    }
                    i++;
                }
                frameToCopy = -1;
            }
            if (mu_button(ctx, "Unselect")){
                frameToCopy = -1;
            }
        }
}

void TheatreRenderUnderlay(Viewport *v){   
    Color bgColor = (Color){
        timeline.currentFrame->bgColor[0],
        timeline.currentFrame->bgColor[1],
        timeline.currentFrame->bgColor[2],
        255,
    };

    ClearBackground(bgColor);
}

void TheatreRender(Viewport *v){
    if (timeline.currentFrame != NULL){
        Color virtualCamFrameColor = (Color){
            255 - timeline.currentFrame->bgColor[0],
            255 - timeline.currentFrame->bgColor[1],
            255 - timeline.currentFrame->bgColor[2],
            255
        };
        
        // RENDER ONION SKINS
        if (state != PLAYING_ANIMATION){
            float baseOpacity = 255 / (onionSkinsTrace + 1);
            int i=0;
            for (Frame *f = timeline.currentFrame->prev; f != NULL; f = f->prev){
                if (i++ >= onionSkinsTrace) break;
                float opacity = baseOpacity * (onionSkinsTrace - i + 1);
                for (PuppetSnapshot *s = f->head; s != NULL; s = s->next){
                    DrawOnionSkin(s, opacity);
                }

                // DRAW ONION FRAME
                if (cameraMode == CAMERA_EDITOR_MODE){
                    Color onionFrameColor = virtualCamFrameColor;
                    onionFrameColor.a = opacity;
                    Vector2 onionPoints[5];
                    UpdateVirtualCameraCorners(&f->cameraPos, onionPoints);
                    DrawSplineLinear(onionPoints,5,1/v->camera.zoom,onionFrameColor);
                }
            }
        }

        // RENDER PUPPETS
        for (PuppetSnapshot *s = timeline.currentFrame->head; s != NULL; s = s->next){
            DrawPuppetSkin(s->puppet);
            if (theatreTargetBone != NULL){
                DrawCircle(theatreTargetBone->position.x, theatreTargetBone->position.y, (HINGE_RADIUS+2)/v->camera.zoom, PINK);
            }
            if (state != PLAYING_ANIMATION)
                DrawPuppetSkeleton(s->puppet, v->camera.zoom, renderBones);
        }
    
        // RENDER CAMERA PREVIEW
        if (cameraMode == CAMERA_EDITOR_MODE){
            VirtualCameraSnapshot *camPos = &timeline.currentFrame->cameraPos;
            DrawSplineLinear(virtualCameraCorners,5,1/v->camera.zoom,virtualCamFrameColor);
            DrawCircle(virtualCameraCorners[0].x,virtualCameraCorners[0].y,HINGE_RADIUS/v->camera.zoom,BLUE);
            DrawCircle(virtualCameraCorners[2].x,virtualCameraCorners[2].y,HINGE_RADIUS/v->camera.zoom,ORANGE);
            DrawCircle(virtualCameraCorners[3].x,virtualCameraCorners[3].y,HINGE_RADIUS/v->camera.zoom,RED);
        }
    }
}

void TheatreRenderOverlay(Viewport *v){
    if (cameraMode == CAMERA_PREVIEW_MODE){
        VirtualCameraSnapshot *camPos = &timeline.currentFrame->cameraPos;
        float heightDelta = (v->size.height*-1-camera.h)/2;
        float widthDelta = (v->size.width-camera.w)/2;
        DrawRectangle(0,0,v->size.width,heightDelta-((TIMELINE_HEIGHT+TIMELINE_SCROLLBAR_HEIGHT)/2),BLACK);
        DrawRectangle(0,heightDelta+camera.h-((TIMELINE_HEIGHT+TIMELINE_SCROLLBAR_HEIGHT)/2),v->size.width,heightDelta,BLACK);
        DrawRectangle(0,0,widthDelta,v->size.height*-1,BLACK);
        DrawRectangle(widthDelta+camera.w,0,widthDelta,v->size.height*-1,BLACK);
    }

    // TIMELINE
    int timelineY = (v->size.height*-1)-TIMELINE_HEIGHT;
    DrawRectangle(-1, timelineY, v->size.width+2, TIMELINE_HEIGHT,  VIEWPORT_BG_C);
    DrawRectangleLines(-1, timelineY, v->size.width+2, TIMELINE_HEIGHT+1,  VIEWPORT_OUTLINE_C);
    

    int frameMargin = TIMELINE_FRAME_DISTANCE;
    int frameDimension = TIMELINE_HEIGHT - TIMELINE_FRAME_DISTANCE*2;
    for (int i=0; i<timeline.frameCount; i++){
        Color linesColor = VIEWPORT_OUTLINE_C;
        if (i == frameToCopy)
            linesColor = GREEN;
        else if (i == timeline.currentFrameIndex)
            linesColor = VIEWPORT_TITLE_C;
        else if (i == timelineHoverFrame)
            linesColor = WHITE;

        DrawRectangleLines(
            frameMargin + timelineOffset, 
            timelineY+TIMELINE_FRAME_DISTANCE, 
            frameDimension, 
            frameDimension, 
            linesColor
        );
        
        DrawTextCustom2(
            (Vector2){
                frameMargin + timelineOffset + 10,
                timelineY+TIMELINE_FRAME_DISTANCE + 10
            }, 
            "F%i", 
            i);

        frameMargin += frameDimension+TIMELINE_FRAME_DISTANCE;
    }

    int indicatorWidth = 100;
    int indicatorHeight = 20;
    DrawRectangle(v->size.width - indicatorWidth, timelineY, indicatorWidth, indicatorHeight, BG_C);
    DrawTextCustom2((Vector2){v->size.width - indicatorWidth, timelineY}, "F%i / F%i", timeline.currentFrameIndex, timeline.frameCount);

    // TIMELINE SCROLLBAR
    int barY = timelineY - TIMELINE_SCROLLBAR_HEIGHT;
    DrawRectangle(-1, barY, v->size.width+2, TIMELINE_SCROLLBAR_HEIGHT, *(Color*) &v->ctx.style->colors[MU_COLOR_SCROLLBASE]);
    DrawRectangle(-1 + scrollbarThumboOffset, barY, scrollbarThumbWidth, TIMELINE_SCROLLBAR_HEIGHT, *(Color*) &v->ctx.style->colors[MU_COLOR_SCROLLTHUMB]);
    
}