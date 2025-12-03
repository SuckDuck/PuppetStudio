#ifndef __MJPEGW_H__
#define __MJPEGW_H__

#include <stddef.h>
#include <stdint.h>

//-----------------------------------------------------------------------------------------------------------------------------
// Memory interface for user-custom allocators
typedef struct mjpegw_mem_interface
{
    void*  (*malloc_fn)(size_t size, void* user);
    void*  (*realloc_fn)(void* old_ptr, size_t old_size, size_t new_size, void* user);
    void   (*free_fn)(void* ptr, void* user);
    void*   user;
} mjpegw_mem_interface;

struct mjpegw_context;

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------------------------------------------------------
// Opens a new AVI file
//          [filename]          Name of the file, overwritten if it already exists
//          [width, height]     Resolution of the video, all frame *must* have this resolution
//          [fps]               Frame per second
//          [mem]               Custom allocator, if NULL stdlib will be used (alloc/realloc/free)
//
//  Returns a context to be used in the following function calls
struct mjpegw_context* mjpegw_open(const char *filename, uint32_t width, uint32_t height, uint32_t fps, mjpegw_mem_interface* mem);


//-----------------------------------------------------------------------------------------------------------------------------
// Adds a new frame to the video
//          [ctx]               Previous created context
//          [pixels]            Pointer to R8G8B8A8 data (32 bits per pixel)
//          [quality]           JPEG Compresion setring 
//                                  3: Highest. Compression varies wildly (between 1/3 and 1/20).
//                                  2: Very good quality. About 1/2 the size of 3.
//                                  1: Noticeable. About 1/6 the size of 3, or 1/3 the size of 2.
void mjpegw_add_frame(struct mjpegw_context *ctx, const void* pixels, const int quality);


//-----------------------------------------------------------------------------------------------------------------------------
// Finalizes and closes the AVI file
//          [ctx]               Previous created context
void mjpegw_close(struct mjpegw_context *ctx);


#ifdef __cplusplus
}
#endif


#endif