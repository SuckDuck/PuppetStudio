#ifndef MODULES_H
#define MODULES_H

typedef struct Module{
    void (*Init)(struct Module*);
    void (*Update)(struct Module*);
    struct Module *prev;
    struct Module *next;
} Module;

typedef struct ModuleLinkedList{
    Module *head;
    Module *tail;
} ModuleLinkedList;

extern ModuleLinkedList modules;

void LoadModule(void (*Init)(Module*), void (*Update)(Module*));
void CleanUpModulesPool();
int RegisterModules();

//designroom.c
void AtlasGarbageCollector(Module *m);

#endif