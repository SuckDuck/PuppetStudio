#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "puppets.h"
#include "utils.h"
#include "config.h"

Puppet *onEditPuppet;
Bone *onEditSelectedBone;
AtlasLinkedList atlasCache;

Atlas *LoadAtlas(char *path){
    Image newImage = LoadImage(path);
    if (newImage.data == NULL){
        PushLog("Atlas '%s' could not be loaded",path);
        return NULL;
    }

    unsigned long hash = djb2Hash(newImage.data,  GetPixelDataSize(newImage.width, newImage.height, newImage.format));
    for (Atlas *a = atlasCache.head; a != NULL; a = a->next){
        if (a->id == hash){
            UnloadImage(newImage);
            a->refCount++;
            return a;
        }   
    }
    
    Atlas *newAtlas = calloc(1,sizeof(Atlas));
    newAtlas->id = hash;
    newAtlas->texture = LoadTextureFromImage(newImage);
    newAtlas->prev = newAtlas->next = NULL;
    UnloadImage(newImage);

    //link the list
    if (atlasCache.tail != NULL){
        atlasCache.tail->next = newAtlas;
        newAtlas->prev = atlasCache.tail;
        atlasCache.tail = newAtlas;
    }
    
    if (atlasCache.head == NULL){
        atlasCache.head = atlasCache.tail = newAtlas;
    }

    newAtlas->refCount = 1;
    return newAtlas;
}

void RemoveAtlas(Atlas *a){
    if (a == NULL) return;
    if (a == atlasCache.head) atlasCache.head = a->next;
    if (a == atlasCache.tail) atlasCache.tail = a->prev;
    if (a->prev != NULL) a->prev->next = a->next;
    if (a->next != NULL) a->next->prev = a->prev;
    UnloadTexture(a->texture);
    free(a);
}

void LoadAtlasToPuppet(Puppet *p, char *path){
    if (p == NULL){
        PushLog("Can´t load an atlas, there is no puppet!",path);
        return;
    }

    Atlas *a = LoadAtlas(path);
    if (a == NULL){
        PushLog("Atlas '%s' could not be loaded",path);
        return;
    }

    if (p->atlas != NULL){
        p->atlas->refCount--;
    }
    
    p->atlas = a;
    PushLog("Atlas '%s' loaded succesfully!",path);
}

void SetSkinAngle(Skin *s){
    if (s == NULL) return;

    Vector2 A = (Vector2){
        s->xFlip ? s->pointB.x : s->pointA.x,
        s->pointA.y
    };

    Vector2 B = (Vector2){
        s->xFlip ? s->pointA.x : s->pointB.x,
        s->pointB.y
    };
    
    s->angle = VectorToDegrees(Vector2Normalize(Vector2Subtract(A, B))) + 180;
}

void XFlipSkin(Skin *s){
    if (s == NULL) return;
    s->xFlip = !s->xFlip;
    SetSkinAngle(s);
}

void YFlipSkin(Skin *s){
    if (s == NULL) return;
    Vector2 A = s->pointA;
    Vector2 B = s->pointB;

    Vector2 center = (Vector2){
        s->rect.x + (s->rect.width/2),
        s->rect.y + (s->rect.height/2)
    };
    
    Vector2 aDelta = (Vector2){
        A.x-center.x,
        A.y-center.y
    };

    Vector2 bDelta = (Vector2){
        B.x-center.x,
        B.y-center.y
    };

    s->pointA = (Vector2){
        center.x - aDelta.x,
        center.y - aDelta.y,
    };

    s->pointB = (Vector2){
        center.x - bDelta.x,
        center.y - bDelta.y,
    };
    
    SetSkinAngle(s);
}

void XFlipPuppet(Puppet *p){
    if (p == NULL) return;
    if (p->root != NULL) p = p->root;

    for (int i=0; i<p->descendantsQ; i++){
        Bone *b = p->descendants[i];
        float xDistance = b->position.x - p->position.x;
        b->position.x -= xDistance*2;
        b->direction = Vector2Normalize(Vector2Subtract(b->position, b->parent->position));
        XFlipSkin(&b->skin);
    }

}

void YFlipPuppet(Puppet *p){
    if (p == NULL) return;

    for (int i=0; i<p->descendantsQ; i++){
        Bone *b = p->descendants[i];
        float yDistance = b->position.y - p->position.y;
        b->position.y -= yDistance*2;
        b->direction = Vector2Normalize(Vector2Subtract(b->position, b->parent->position));
        XFlipSkin(&b->skin);
    }
}

void RebuildDescendants(Puppet *p){
    // TODO: Replace static recursion state with parameters
    static int callCount = 0;
    static int totalChildQ = 0;
    static Bone *root;
    static Bone **descendants = NULL;
    bool firstCallFlag = false;
    
    if (descendants != NULL){
        p->root = root;
        *descendants = p;
        descendants++;
    }
    
    if (totalChildQ == 0){
        firstCallFlag = true;
    }

    for (int i=0; i<p->childsQ; i++){
        totalChildQ++;
        p->childs[i]->parent = p;
        RebuildDescendants(p->childs[i]);
    }

    if (firstCallFlag){
        p->descendantsQ = totalChildQ;
        if (p->descendants != NULL)
            free(p->descendants);
        p->descendants = (Bone**) malloc(sizeof(Bone*)*p->descendantsQ);
        root = p;
        descendants = p->descendants;

        for (int i=0; i<p->childsQ; i++){
            RebuildDescendants(p->childs[i]);
        }
        
        totalChildQ = 0;
        callCount = 0;
        descendants = NULL;
        root = NULL;
    }

}

void RebuildDescendantsIndex(Puppet *p){
    for (int i=0; i<p->descendantsQ; i++){
        p->descendants[i]->index = i+1;
    }
}

int RebuildZIndex(Puppet *p){
    Bone *bones[p->descendantsQ];
    for (int i=0; i<p->descendantsQ; i++){
        bones[i] = p->descendants[i];
    }

    // BUBBLE SORT
    while (true){
        bool repeat = false;
        for (int i=0; i<p->descendantsQ-1; i++){
            Bone *b0 = bones[i];
            Bone *b1 = bones[i+1];
        
            if (b1->skin.zIndex < b0->skin.zIndex){
                bones[i] = b1;
                bones[i+1] = b0;
                repeat = true;
            }
        }

        if (!repeat) break;
    }

    // ASSIGN NEW Z-INDEX
    for (int i=0; i<p->descendantsQ; i++){
        bones[i]->skin.zIndex = i;
    }

    return p->descendantsQ-1;
}

void MoveBoneUpZIndex(Bone *b){
    if (b == NULL) return;
    if (b->root == NULL) return;
    if (b->skin.zIndex == 0) return;
    for (int i=0; i<b->root->descendantsQ; i++){
        Bone *bi = b->root->descendants[i];
        if (bi->skin.zIndex == b->skin.zIndex-1){
            bi->skin.zIndex++;
            b->skin.zIndex--;
            break;
        }
    }
}

void MoveBoneDownZIndex(Bone *b){
    if (b == NULL) return;
    if (b->root == NULL) return;
    if (b->skin.zIndex == b->root->descendantsQ-1) return;
    for (int i=0; i<b->root->descendantsQ; i++){
        Bone *bi = b->root->descendants[i];
        if (bi->skin.zIndex == b->skin.zIndex+1){
            bi->skin.zIndex--;
            b->skin.zIndex++;
            break;
        }
    }
}

void UpdateDescendantsPos(Puppet *p, Vector2 pos, bool firstCall){
    Vector2 endPoint;

    if (!firstCall){
        endPoint = Vector2Add(pos, Vector2Multiply(p->direction, (Vector2){p->len * p->root->scale,p->len * p->root->scale}));
        p->position = endPoint;
    }
    else endPoint = Vector2Add(pos, Vector2Multiply(p->direction, (Vector2){p->len,p->len}));

    for (int i=0; i<p->childsQ; i++){
        UpdateDescendantsPos(p->childs[i], endPoint, false);
    }
}

void RotateBonesDegrees(Bone *b, float degrees, bool relative){
    float currentDegrees = VectorToDegrees(b->direction);
    b->direction = DegreesToVector(degrees + (relative ? currentDegrees : 0));
    for (int i=0; i<b->childsQ; i++){
        RotateBonesDegrees(b->childs[i], relative ? degrees : degrees - currentDegrees, true);
    }
}

void RotateBonesTowards(Bone *b, Vector2 to, bool stretch, bool propagation, bool blockRange){
    Vector2 newDirection = Vector2Subtract(to, b->parent->position);
    Vector2 newDirectionNormalized = Vector2Normalize(newDirection);
    float toDistance = blockRange ? b->len : (Vector2Length(newDirection)/b->root->scale);
    
    if (stretch){
        b->len = toDistance;
        b->range = toDistance;
    }
        
    float newDirectionDegrees = VectorToDegrees(newDirectionNormalized);
    float oldDirectionDegrees = VectorToDegrees(b->direction);
    float delta = newDirectionDegrees - oldDirectionDegrees;
    
    if (delta != 0){
        b->direction = newDirectionNormalized;
        b->len = b->range;
        if (toDistance <= b->range){
            b->len = toDistance;
        }
            
        // rotation propagation
        if (propagation){
            for (int i=0; i<b->childsQ; i++){
                RotateBonesDegrees(b->childs[i], delta, true);
            }
        }
    }
}

void AdjustBonesToPosition(Bone *b, Vector2 from){
    Vector2 newDirection = Vector2Subtract(b->position, from);
    b->direction = Vector2Normalize(newDirection);
    b->len = Vector2Length(newDirection);
    if (b->range < b->len)
        b->range = b->len;
    for (int i=0; i<b->childsQ; i++){
        AdjustBonesToPosition(b->childs[i],b->position);
    }
}

void MoveBoneEndPoint(Bone *b, Vector2 to){
    Vector2 newDirection = Vector2Subtract(to, b->parent->position);
    Vector2 newDirectionNormalized = Vector2Normalize(newDirection);
    b->direction = newDirectionNormalized;
    b->len = Vector2Length(newDirection);
    b->range = b->len;

    for (int i=0; i<b->childsQ; i++){
        AdjustBonesToPosition(b->childs[i],to);
    }
    
    
}

Bone *AddBoneVector(Bone *b, Vector2 dir, float len, float range, int zindex, Skin s){
    b->childs[b->childsQ++] = (Bone*) calloc(1,sizeof(Bone));
    b->childs[b->childsQ-1]->direction = Vector2Normalize(dir);
    b->childs[b->childsQ-1]->len = len;
    b->childs[b->childsQ-1]->range = range;
    b->childs[b->childsQ-1]->skin = s;
    b->childs[b->childsQ-1]->skin.zIndex = zindex;
    return b->childs[b->childsQ-1];
}

Bone *AddBoneToPoint(Bone *b, Vector2 point, int zindex, Skin s){
    Vector2 dir = Vector2Subtract(point, b->position);
    float len = Vector2Length(dir);
    return AddBoneVector(b, dir, len, len, zindex, s);
}

Bone *AddBoneAngle(Bone *b, float degrees, float len, int zindex, Skin s){
    return AddBoneVector(b, DegreesToVector(degrees), len, len, zindex, s);
}

void DeleteBone(Bone *b){
    for (int i=b->childsQ-1; i>=0; i--){
        DeleteBone(b->childs[i]);
    }

    // get the bone index in the parent's childs array
    int boneIndex = 0;
    while (true){
        Bone *c = b->parent->childs[boneIndex];
        if (b == c)
            break;
        boneIndex++;
    }
    
    // reorganize the parent's childs array
    b->parent->childsQ--;
    for (int i=0; i<b->parent->childsQ-boneIndex; i++){
        b->parent->childs[boneIndex+i] = b->parent->childs[boneIndex+i+1];
    }
    
    b->parent->childs[b->parent->childsQ] = NULL;
    free(b);
}

void DeletePuppet(Puppet *p){
    if (p->childsQ>0){
        DeleteBone(p->childs[0]);
    }

    if (p->descendants)
        free(p->descendants);
    
    if (p->atlas != NULL)
        p->atlas->refCount--;
    
    if (p->name != NULL)
        free(p->name);
    
    free(p);
}

Puppet *NewPuppet(){
    Puppet *p = (Puppet*) calloc(1,sizeof(Puppet));
    p->scale = 1;
    return p;
}

int SavePuppet(Puppet *p, char* path){
    RebuildDescendants(p);
    RebuildDescendantsIndex(p);

    int fd = open(path,O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd < 0) return fd;

    //save version
    write(fd,"PUPPET_STUDIO_V",sizeof(char)*15);
    int version = PROJECT_VERSION;
    write(fd,&version,sizeof(int));

    write(fd,&p->descendantsQ,sizeof(p->descendantsQ));
    for (int i=0; i<p->descendantsQ; i++){
        Bone *pi = p->descendants[i];
        write(fd,&pi->index,sizeof(pi->index));
        write(fd,&pi->parent->index,sizeof(pi->parent->index));
        write(fd,&pi->direction,sizeof(pi->direction));
        write(fd,&pi->len,sizeof(pi->len));
        write(fd,&pi->range,sizeof(pi->len));
        write(fd,&pi->skin,sizeof(pi->skin));
    }

    close(fd);
    chmod(path,0666);

    if (p->atlas == NULL) return 0;
    char atlasPath[PATH_MAX] = {0};
    sprintf(atlasPath, "%s/%s", GetDirectoryPath(path),"atlas.png");
    Image im = LoadImageFromTexture(p->atlas->texture);
    ExportImage(im, atlasPath);
    UnloadImage(im);

    return 0;
}

Puppet *LoadPuppet(char* path){
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;

    Puppet *p = NewPuppet();
    Bone *bones[256];
    bones[0] = p;

    //I should add some corroboration here
    char header[15] = {0};
    read(fd, &header, sizeof(char)*15);

    //Because of future changes, maybe...
    int version;
    read(fd, &version, sizeof(int));

    int bonesQ;
    read(fd,&bonesQ,sizeof(int));

    for (int i=1; i<bonesQ+1; i++){
        int index, parentIndex;
        Vector2 direction;
        float len;
        float range;
        Skin s;

        read(fd,&index,sizeof(int));
        read(fd,&parentIndex,sizeof(int));
        read(fd,&direction,sizeof(Vector2));
        read(fd,&len,sizeof(float));
        read(fd,&range,sizeof(float));
        read(fd,&s,sizeof(Skin));

        bones[i] = AddBoneVector(bones[parentIndex],direction,len, range, s.zIndex,s);
    }
    
    RebuildDescendants(p);
    RebuildDescendantsIndex(p);
    UpdateDescendantsPos(p, p->position, true);
    
    char atlasPath[PATH_MAX] = {0};
    sprintf(atlasPath, "%s/%s", GetDirectoryPath(path),"atlas.png");
    LoadAtlasToPuppet(p, atlasPath);

    close(fd);
    return p;
}

void DrawBones(Bone *b, Vector2 pos, float hingeRadius, bool drawLines){
    Vector2 endPoint = Vector2Add(pos, Vector2Multiply(b->direction, (Vector2){b->len*b->root->scale,b->len*b->root->scale}));
    if (drawLines) DrawLine(pos.x,pos.y,endPoint.x,endPoint.y,WHITE);
    for (int i=0; i<b->childsQ; i++){
        DrawBones(b->childs[i], endPoint, hingeRadius, drawLines);
    }
    DrawCircle(endPoint.x,endPoint.y,hingeRadius,BLUE);
}

void DrawPuppetSkin(Puppet *p){
    if (p == NULL) return;
    if (p->atlas == NULL) return;

    //SORT THE BONES BY THEIR Z-INDEX
    Bone *bonesInOrder[p->descendantsQ];
    for (int i=0; i<p->descendantsQ; i++){
        Bone *b = p->descendants[i];
        bonesInOrder[b->skin.zIndex] = b;
    }

    //RENDER EACH BONE SKIN
    for (int i = p->descendantsQ - 1; i >= 0; i--){
        Bone *b = bonesInOrder[i];
        float scale = (b->len*b->root->scale) / Vector2Length(Vector2Subtract(b->skin.pointA, b->skin.pointB));
        
        Rectangle src = (Rectangle){
            b->skin.rect.x,
            b->skin.rect.y,
            b->skin.xFlip ? b->skin.rect.width*-1 : b->skin.rect.width,
            b->skin.rect.height
        };

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

        Vector2 center = (Vector2){
            dst.width/2,
            dst.height/2
        };

        if (b->skin.xFlip){
            float delta = org.x - center.x;
            org.x = center.x-delta;
        }

        float angle = VectorToDegrees(b->direction)-b->skin.angle;

        DrawTexturePro(
            p->atlas->texture,
            src,
            dst,
            org,
            angle,
            WHITE
        );
    }
}

void DrawPuppetSkinTo(Puppet *p, Vector2 pos){
    if (p == NULL) return;
    Vector2 ogPos = p->position;
    p->position = pos;
    UpdateDescendantsPos(p, pos, true);
    DrawPuppetSkin(p);
    p->position = ogPos;
    UpdateDescendantsPos(p, ogPos, true);
}

void DrawPuppetSkeleton(Puppet *p, float zoom, bool drawLines){
    for (int i=0; i<p->childsQ; i++){
        DrawBones(p->childs[i], p->position, HINGE_RADIUS/zoom, drawLines);
    }
    DrawCircle(p->position.x, p->position.y, HINGE_RADIUS/zoom, GREEN);
}