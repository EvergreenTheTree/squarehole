pixelsort
=========

A custom GEGL operation (and by extension GIMP filter) that implements a pixel
sorting effect.  To use it click "Tools->GEGL Operation" and choose "Pixel Sort"
in the dropdown.

This is primarily a port of the following processing script to a GEGL operation:
https://github.com/kimasendorf/ASDFPixelSort

## Install

### Linux
Download [https://github.com/EvergreenTheTree/pixelsort/releases/download/v1.0.0/pixelsort.so](https://github.com/EvergreenTheTree/pixelsort/releases/download/v1.0.0/pixelsort.so) and put it in `~/.local/share/gegl-0.4/plug-ins`

If you have an ancient GIMP / GEGL version you may need to put it in `~/.local/share/gegl-0.3/plug-ins`.

**NOTE:** as of writing this pixelsort seems to not work with gegl-0.3 in GIMP. It works
when run with the GEGL command line utility but for some reason none of the operation's
properties show up in the GEGL tool window.

## Windows
Download [https://github.com/EvergreenTheTree/pixelsort/releases/download/v1.0.0/pixelsort.dll](https://github.com/EvergreenTheTree/pixelsort/releases/download/v1.0.0/pixelsort.dll) and put it in one of these directories depending on how / where GIMP is installed:

- GIMP 3.xx installed locally: `C:\Users\$USER\AppData\Local\Programs\GIMP 3\lib\gegl-0.4\`
- GIMP 3.xx installed system-wide: `C:\Program Files\GIMP 3\lib\gegl-0.4\`
- GIMP 2.xx: `C:\Program Files\GIMP 2\lib\gegl-0.4\`

## Compile

### Linux

To compile and install you will need the GEGL header files (`libgegl-dev` on
Debian based distributions or `gegl` on Arch Linux), meson (`meson` on
most distributions), and some C compiler (`gcc`, `clang`, etc).

```bash
meson setup --buildtype=release build
ninja -C build
cp build/pixelsort.so ~/.local/share/gegl-0.4/plug-ins
# Or this for older GEGL (does not work in GIMP, just the GEGL command line tool)
cp build/pixelsort.so ~/.local/share/gegl-0.3/plug-ins
```

### Windows

The easiest way to compile this project on Windows is by using msys2.  Download
and install it from here: https://www.msys2.org/

Open a msys2 terminal with `C:\msys64\mingw64.exe`.  Run the following to
install required build dependencies:

```bash
pacman --noconfirm -S base-devel mingw-w64-clang-x86_64-toolchain mingw-w64-clang-x86_64-meson mingw-w64-clang-x86_64-python mingw-w64-clang-x86_64-gegl
```

Then build:

```bash
export PATH=/clang64/bin/:"$PATH"
meson setup --buildtype=release build
ninja -C build
```

To install it, copy the resulting DLL (`build/pixelsort.dll`) to the folder
where GIMP finds GEGL operations:

GIMP 3.xx:
```bash
# If GIMP 3.0 is installed locally (via chocolatey for instance)
cp build/pixelsort.dll '/c/Users/$USER/AppData/Local/Programs/GIMP 3/lib/gegl-0.4/'
# If GIMP 3.0 is installed system-wide
cp build/pixelsort.dll '/c/Program Files/GIMP 3/lib/gegl-0.4/'
# If GIMP 2.0 is installed
cp build/pixelsort.dll '/c/Program Files/GIMP 2/lib/gegl-0.4/
```

## Example Images

Original image:

![Original image](examples/example_1_original.png)

Vertical sorting with luminance as the mode:

![Original image](examples/example_1_luminance.png)

Horizontal sorting with white level as the mode:

![Original image](examples/example_1_white.png)

Complete vertical sorting by luminance (threshold of 0):

![Original image](examples/example_1_complete_sort.png)
