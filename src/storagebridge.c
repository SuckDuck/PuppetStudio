#if  __wasm__
#include <emscripten.h>
#include <fcntl.h>
#include <raylib.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "config.h"
#include "microui.h"
#include "utils.h"
#include "viewports.h"
#include "sampleProject.h"
#include "samplePuppets.h"
#include "miniz.h"

#define IMPORT_PATH "/" PROJECT_TITLE "/"

extern void OpenDir(char *path);

static const char *commands[] = {
    NULL
};

void CopySampleProject(){
    // extract sampleProject.zip files
    mz_zip_archive zipArchive;
    memset(&zipArchive, 0, sizeof(zipArchive));
    mz_zip_reader_init_mem(&zipArchive, sampleProject_zip, sampleProject_zip_len, 0);

    // For every file inside zip
    mkdir("sampleProject", 0777);
    for (int i=0; i<(int)mz_zip_reader_get_num_files(&zipArchive); i++){
        mz_zip_archive_file_stat file_stat;
        mz_zip_reader_file_stat(&zipArchive, i, &file_stat);        

        // create directories
        if (mz_zip_reader_is_file_a_directory(&zipArchive, i)){
            mkdir(file_stat.m_filename,0777);
            continue;
        }

        // uncompress files
        mz_zip_reader_extract_to_file(&zipArchive, file_stat.m_file_index, file_stat.m_filename, 0);
    }
    mz_zip_reader_end(&zipArchive);
}

void CopySamplePuppets(){
    // extract puppets.zip files
    mz_zip_archive zipArchive;
    memset(&zipArchive, 0, sizeof(zipArchive));
    mz_zip_reader_init_mem(&zipArchive, samplePuppets_zip, samplePuppets_zip_len, 0);

    // For every file inside zip
    mkdir("puppets", 0777);
    for (int i=0; i<(int)mz_zip_reader_get_num_files(&zipArchive); i++){
        mz_zip_archive_file_stat file_stat;
        mz_zip_reader_file_stat(&zipArchive, i, &file_stat);        

        // create directories
        if (mz_zip_reader_is_file_a_directory(&zipArchive, i)){
            mkdir(file_stat.m_filename,0777);
            continue;
        }

        // uncompress files
        mz_zip_reader_extract_to_file(&zipArchive, file_stat.m_file_index, file_stat.m_filename, 0);
    }
    mz_zip_reader_end(&zipArchive);
}

void StorageBridgeInit(Viewport *v){
    SetViewportPanelsDimensions(v, 360, 0, 0, 0);
    mkdir(PROJECT_TITLE,0777);
    chdir(PROJECT_TITLE); 
    CopySampleProject();
    CopySamplePuppets();
    OpenDir(".");
}

const char **StorageBridgeGetCommands(){
    return commands;
}

void StorageBridgeExecCmd(Viewport *v, int argc, char **argv){
}

void StorageBridgeLeftPanel(Viewport *v, mu_Context *ctx){
    mu_layout_row(ctx, 3, (int[]){100, -40, -1},0);
    mu_label(ctx, "ImportPath:", ctx->style->control_font_size);
    static char importRoute[PATH_MAX] = IMPORT_PATH;
    mu_textbox(ctx, importRoute, PATH_MAX);
    long exploreID = 1853963465;
    mu_push_id(ctx, &exploreID, sizeof(long));
    if (mu_button(ctx, "...")) OpenExplorer(importRoute, PATH_MAX);
    mu_pop_id(ctx);
    mu_layout_row(ctx, 3, (int[]){-241, -120, -1},0);
    mu_space(ctx);
    if (mu_button(ctx, "Import Files")){
        EM_ASM({ImportFIles(UTF8ToString($0));}, importRoute);
    }
    
    if (mu_button(ctx, "Import Folder")){
        EM_ASM({ImportFolder(UTF8ToString($0));}, importRoute);
    }

    mu_vertical_space(ctx, 3);

    mu_layout_row(ctx, 3, (int[]){100, -40, -1},0);
    mu_label(ctx, "ExportSrc:", ctx->style->control_font_size);
    static char exportSource[PATH_MAX];
    mu_textbox(ctx, exportSource, PATH_MAX);
    exploreID = 364389758393;
    mu_push_id(ctx, &exploreID, sizeof(long));
    if (mu_button(ctx, "...")) OpenExplorer(exportSource, PATH_MAX);
    mu_pop_id(ctx);
    mu_layout_row(ctx, 3, (int[]){-241, -120, -1},0);
    mu_space(ctx);
    if (mu_button(ctx, "ExportFile")){
        if (!FileExists(exportSource)){PushLog("File doesn't exists!"); return;}
        if (!IsPathFile(exportSource)){PushLog("Path should be a file!"); return;}
        const char *filename = GetFileName(exportSource);        
        EM_ASM({ExportFile(UTF8ToString($0), UTF8ToString($1));}, exportSource, filename);
    }
    if (mu_button(ctx, "ExportFolder")){
        if (!DirectoryExists(exportSource)){PushLog("Dir doesn't exists!"); return;}
        if (IsPathFile(exportSource)){PushLog("Path should be a dir!"); return;}
        const char *filename = GetFileName(exportSource);
        EM_ASM({ExportDir(UTF8ToString($0), UTF8ToString($1));}, exportSource, filename);
    }


    mu_vertical_space(ctx, 3);
    mu_layout_row(ctx, 3, (int[]){100, -40, -1},0);
    mu_label(ctx, "DeleteTarget:", ctx->style->control_font_size);
    static char deleteTarget[PATH_MAX];
    mu_textbox(ctx, deleteTarget, PATH_MAX);
    exploreID = 138568120385;
    mu_push_id(ctx, &exploreID, sizeof(long));
    if (mu_button(ctx, "...")) OpenExplorer(deleteTarget, PATH_MAX);
    mu_pop_id(ctx);
    mu_layout_row(ctx, 3, (int[]){-241, -120, -1},0);
    mu_space(ctx);
    
    if (mu_button(ctx, "DeleteFile")){
        if (!FileExists(deleteTarget)){PushLog("File doesn't exists!"); return;}
        if (!IsPathFile(deleteTarget)){PushLog("Path should be a file!"); return;}
        remove(deleteTarget);
        PushLog("file '%s' deleted!", deleteTarget);
        OpenDir(".");
    }

    if (mu_button(ctx, "DeleteFolder")){
        if (!DirectoryExists(deleteTarget)){PushLog("Dir doesn't exists!"); return;}
        if (IsPathFile(deleteTarget)){PushLog("Path should be a dir!"); return;}
        RemoveDir(deleteTarget);
        PushLog("dir '%s' deleted!", deleteTarget);
        OpenDir(".");
    }
}

#endif