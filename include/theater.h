#ifndef THEATER_H
#define THEATER_H

#include "puppets.h"

typedef struct VirtualCamera{
    float w, h;
} VirtualCamera;

typedef struct VirtualCameraSnapshot{
    float x, y, zoom, rotation;
} VirtualCameraSnapshot;

typedef struct PuppetLinkedList{
    int puppetsQ;
    Puppet *head;
    Puppet *tail;
} PuppetLinkedList;

typedef struct BoneSnapshot{
    Bone *bone;
    Vector2 direction;
    float length;
    Skin skin;
    struct BoneSnapshot *next;
    struct BoneSnapshot *prev;
} BoneSnapshot;

typedef struct BoneSnapshotLinkedList{
    int snapshotsQ;
    BoneSnapshot *head;
    BoneSnapshot *tail;
} BoneSnapshotLinkedList;

typedef struct PuppetSnapshot{
    Puppet *puppet;
    Vector2 position;
    float scale;
    Rectangle boundaries;
    RenderTexture onionSkin;
    BoneSnapshotLinkedList bonesSnapshots;
    struct PuppetSnapshot *next;
    struct PuppetSnapshot *prev;
} PuppetSnapshot;

typedef struct PuppetSnapshotLinkedList{
    PuppetSnapshot *head;
    PuppetSnapshot *tail;
    int snapshotsQ;
    VirtualCameraSnapshot cameraPos;
    float bgColor[3];
    struct PuppetSnapshotLinkedList *next;
    struct PuppetSnapshotLinkedList *prev;
} PuppetSnapshotLinkedList;

typedef PuppetSnapshotLinkedList Frame;

typedef struct FrameLinkedList{
    int frameCount;
    int currentFrameIndex;
    Frame *currentFrame;
    Frame *head;
    Frame *tail;
} FrameLinkedList;

typedef FrameLinkedList Timeline;

bool PupppetIsOnList(Puppet *puppet, PuppetLinkedList *list);
bool PuppetNameIsOnList(char *name, PuppetLinkedList *list);
void CopyPuppetToList(Puppet *puppet, PuppetLinkedList *list, char* name);
void NewPuppetSnapshot(Puppet *p, Frame *f);
void SwitchFrame(int frame, Timeline *t);
void CopyFrame(Frame *src, Frame *dst);

extern PuppetLinkedList puppetsCache;
extern Timeline timeline;
extern Puppet *theatreTargetPuppet;
extern Bone *theatreTargetBone;

#endif