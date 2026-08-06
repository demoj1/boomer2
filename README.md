Zoom/Screenshot tool for linux, dependencies: libX11, raylib, xclip

Wayland (X11 doesn't work there): build with `make wayland`. Captures the screen
via `grim` and copies to the clipboard via `wl-copy`, so it also needs `grim` and
`wl-clipboard` installed. Area selection still happens inside the app window.
Building it additionally needs `wayland-scanner` and `wayland-protocols`.

With more than one monitor each output is captured separately and composed onto a
canvas in the compositor's logical coordinates, so the capture matches how the
monitors are physically arranged and an unscaled monitor stays pixel exact. A bare
`grim` would instead render the whole layout at the largest scale factor in use,
blurring every lower-scale monitor. The layout is read from the compositor over
`xdg-output`; if that is unavailable it falls back to a single full `grim` capture.

Left mouse for drag&drop  
Mouse wheel for zoom in/out  
Shift + mouse wheel for fine zoom  
Right mouse for select screenshot area  
Esc for reset active tools  
Enter or C to save area into clipboard  

Tools:  
  * How use tools:  
    * When tool is activated (tool key is down), press left mouse for persist on screen.  
    * When tool is activated (tool key is down), press right mouse for delete last persistent item.
  * Tools key:
    * Crosshair (hotkey F)  
    * Line (hotkey S)  
    * Rectangle (hotkey R)  
    * Arrow (hotkey A)
    * Clip window for selection box (hotkey Z)
    * Blur rectangle (hotkey B): hides an area behind a strong gaussian blur of
      its content (blurred on the GPU once at startup, so dragging it stays fast)
    * Color picker (hotkey X)
    * Numbered marker (hotkeys 1-9): press digits to type a number up to 99, a
      circle with it follows the cursor; left mouse places it, right mouse (while
      active) removes the previously placed one, Esc cancels

Features:
  * Good for screencast (zoom, crosshair)
  * Runs at the monitor's refresh rate (144Hz and up included)
  * Simple screenshot annotation tool (without any bullshit steps for save screenshot, just run, select, press C or Enter and your screenshot copy into clipboard)
  * No UI

Example:  

https://github.com/demoj1/boomer2/assets/10886027/ede19261-2742-4aea-aa2b-a9d87a0ad36c

Idea take from https://github.com/tsoding/boomer
