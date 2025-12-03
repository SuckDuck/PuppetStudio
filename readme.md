# PuppetStudio
A lightweight, simple, and portable 2D skeletal frame-by-frame animation software inspired by Pivot Animator.

Features:
- Create, save, load and edit puppets
- Create, save, load and edit animations
- Support for multiple puppets in a single scene
- Edit puppet poses, positions and skins per frame
- Virtual camera support per frame (position, zoom, rotation)
- Background color per frame
- Copy, paste and delete frames
- Onion-skin support (up to 5 previous frames)
- Export animations to AVI (MJPEG)
- Small, portable C codebase 
- single native executable with minimal runtime dependencies (libc, libm, OpenGL)
- Runs on Linux and WebAssembly

## Example

This is a brief example of what you can do with this software

![example](media/exampleAnimation.gif)

You can also try the browser version at https://puppetstudio.app

## Build / Installation

### Native (Linux)
- Build and install **raylib** (preferably static) following the official guide: [Build raylib using Make on GNU/Linux](https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux#build-raylib-using-make)
- Run `make` to build the native version

### Web (WebAssembly)
- Set up **Emscripten** and build raylib for WebAssembly following the official guide: [Working for Web (HTML5)](https://github.com/raysan5/raylib/wiki/Working-for-Web-(HTML5))
- Run `make web` to build the web version

## Usage

Shift + Enter opens the command bar, from which you can open viewports.
Navigate the bar with the arrow keys, select a viewport to open with Enter, search for viewports by typing their name, press Tab to copy the currently highlighted viewport name into the search field, and press Esc to close the command bar.  
In the Theater and the Workshop, click any bone end to move it. Use the editor flags to modify how the bone moves toward the mouse pointer.  
In the file dialog, use left-click to enter a folder, and right-click to select a folder or a file. Select `.` to reload the dir, select `..`to go back  
In the RegionPresets viewport, use left-click to apply a skin to the currently selected bone in the Closet, and right-click to delete a skin from the table.
A quick video tutorial is available [here](https://youtu.be/gmVuYbRK1vo)

## Notes

PuppetStudio is made possible thanks to the following outstanding open-source projects and the talented developers behind them:

- **raylib** by raysan5 – https://www.raylib.com/  
- **microui** by rxi – https://github.com/rxi/microui  
- **mjpegw** by Geolm – https://github.com/Geolm/mjpegw  
- **miniz** by richgel999 – https://github.com/richgel999/miniz  
