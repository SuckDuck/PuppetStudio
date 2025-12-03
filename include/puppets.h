#ifndef PUPPETS_H
#define  PUPPETS_H
#include <raylib.h>

#define HINGE_RADIUS 5

typedef struct Atlas{
    unsigned long id;
    Texture2D texture;
    struct Atlas *prev;
    struct Atlas *next;
    unsigned int refCount;
} Atlas;

typedef struct AtlasLinkedList{
    Atlas *head;
    Atlas *tail;
} AtlasLinkedList;

typedef struct Skin{
    Rectangle rect;
    Vector2 pointA;
    Vector2 pointB;
    float angle;
    int zIndex;
    bool xFlip;
    bool yFlip;
} Skin;

typedef struct Bone{
    // Bone Variables
    int index;
    Vector2 direction;
    float range;
    float len;
    Skin skin;
    struct Bone *root;
    struct Bone *parent;
    int childsQ;
    struct Bone *childs[32];

    // Puppet Variables
    char *name;
    Vector2 position;
    float scale;
    Atlas *atlas;
    int descendantsQ;
    struct Bone **descendants;
    struct Bone *next;
    struct Bone *prev;
} Bone;

typedef Bone Puppet;

extern Puppet *onEditPuppet;
extern Bone *onEditSelectedBone;
extern AtlasLinkedList atlasCache;

Atlas *LoadAtlas(char *path);
void LoadAtlasToPuppet(Puppet *p, char *path);
void RemoveAtlas(Atlas *a);
void SetSkinAngle(Skin *s);
void XFlipSkin(Skin *s);
void YFlipSkin(Skin *s);

void XFlipPuppet(Puppet *p);
void YFlipPuppet(Puppet *p);
void RebuildDescendants(Puppet *p);
void RebuildDescendantsIndex(Puppet *p);
int RebuildZIndex(Puppet *p);
void MoveBoneUpZIndex(Bone *b);
void MoveBoneDownZIndex(Bone *b);
void UpdateDescendantsPos(Puppet *p, Vector2 pos, bool firstCall);
void RotateBonesDegrees(Bone *b, float degrees, bool relative);
void RotateBonesTowards(Bone *b, Vector2 to, bool stretch, bool propagation, bool blockRange);
void MoveBoneEndPoint(Bone *b, Vector2 to);
Bone *AddBoneVector(Bone *b, Vector2 dir, float len, float range, int zindex, Skin s);
Bone *AddBoneToPoint(Bone *b, Vector2 point, int zindex, Skin s);
Bone *AddBoneAngle(Bone *b, float degrees, float len, int zindex, Skin s);
void DeleteBone(Bone *b);
void DeletePuppet(Puppet *p);
Puppet *NewPuppet();
int SavePuppet(Puppet *p, char* path);
Puppet *LoadPuppet(char* path);
void DrawBones(Bone *b, Vector2 pos, float hingeRadius, bool drawLines);
void DrawPuppetSkin(Puppet *p);
void DrawPuppetSkinTo(Puppet *p, Vector2 pos);
void DrawPuppetSkeleton(Puppet *p, float zoom, bool drawLines);

#endif