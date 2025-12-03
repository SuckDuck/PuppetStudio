#include <stdlib.h>
#include "modules.h"

ModuleLinkedList modules;

void LoadModule(void (*Init)(Module*), void (*Update)(Module*)){
    Module *v = calloc(1, sizeof(Module));
    v->Init = Init;
    v->Update = Update;
    
    // LINK THE LIST
    if (modules.tail == NULL){
        modules.head = v;
    }
    else{
        modules.tail->next = v;
        v->prev = modules.tail;
    }

    modules.tail = v;
}

void CleanUpModulesPool(){
    for(Module *m = modules.head; m != NULL; m = m->next){
        if (m->prev != NULL)
            free(m->prev);
    }
    free(modules.tail);
    modules.head = NULL;
    modules.tail = NULL;
}

int RegisterModules(){
    //designroom.c
    LoadModule(NULL, &AtlasGarbageCollector);
    return 0;
}