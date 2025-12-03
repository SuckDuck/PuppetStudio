#ifdef __wasm__
#include <emscripten.h>
#include "microui.h"
#include "puppets.h"
#include "raylib.h"
#include "theater.h"
#include "utils.h"
#include "viewports.h"
#include <stdio.h>
#include "config.h"

#define WIDTH 380
#define BUTTONS_WIDTH 200
#define SPACE_WIDTH (WIDTH-BUTTONS_WIDTH+4)/2

char *welcomeText[] = {
        "",
        "Hi! I'm Esteban, and this is my small,",
        "minimalist tool for frame-by-frame skeletal",
        "animation",
        "It's nothing fancy, but I made it with care.",
        "I hope you find it useful!",
        "",
        "Some basic usage:",
        "> Click any window to give it focus",
        "> Press Esc to close the focused window",
        "> Press Shift+Enter to open the command bar",
        "  and then open other windows",
        "",
        "Below are some ways to get started:",
        NULL
    };

static const char *commands[] = {
    NULL
};

extern int LoadProject(char *filename);

void WelcomeInit(Viewport *v){
    SetViewportPanelsDimensions(v, WIDTH, 0, 0, 0);
    OpenViewportByName("Welcome");
    v->pos.x = GetScreenWidth()/2 - WIDTH/2;
    v->pos.y = GetScreenHeight()/2 - (v->size.height*-1)/2;
}

const char **WelcomeGetCommands(){
    return commands;
}

void WelcomeExecCmd(Viewport *v, int argc, char **argv){
}

void WelcomeLeftPanel(Viewport *v, mu_Context *ctx){
    mu_layout_row(ctx, 1, (int[]){-1}, 0);
    mu_label(ctx, "Welcome!", 25);
    
    mu_layout_row(ctx, 1, (int[]){-1}, ctx->style->control_font_size);
    for (int i=0; welcomeText[i] != NULL; i++)
        mu_label(ctx, welcomeText[i], ctx->style->control_font_size);

    mu_vertical_space(ctx, 10);

    mu_layout_row(ctx, 2, (int[]){SPACE_WIDTH,BUTTONS_WIDTH}, 0);
    mu_space(ctx); if (mu_button(ctx, "Load Sample Project")){
        LoadProject("/"PROJECT_TITLE"/sampleProject/sample.stage");
        ToggleViewport(v);
    }

    mu_space(ctx); if (mu_button(ctx, "Load Sample Puppet")){
        Puppet *samplePuppet = LoadPuppet("/PuppetStudio/puppets/samplePuppet0/sample.puppet");
        CopyPuppetToList(samplePuppet, &puppetsCache, "SamplePuppet");
        DeletePuppet(samplePuppet);
        NewPuppetSnapshot(puppetsCache.head, timeline.currentFrame);
        ToggleViewport(v);
    }

    mu_space(ctx); if (mu_button(ctx, "Import your files")){
        OpenViewportByName("Storage Bridge");
        ToggleViewport(v);
    }

    mu_space(ctx); if (mu_button(ctx, "Create a puppet")){
        OpenViewportByName("WorkShop");
        ToggleViewport(v);
    }

    mu_space(ctx); if (mu_button(ctx, "Watch a tutorial")){
        EM_ASM({ OpenTutorial(); });
    }
}

#endif