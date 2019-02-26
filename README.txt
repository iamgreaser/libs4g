screenie4gamez / libs4g: a library for window capture suitable for capturing game output
Copyright (c) Ben "GreaseMonkey" Russell.
See the LICENCE.txt for the licence, or if you know what the zlib licence is it's basically that.

NOTE: Has only been tested on Linux on two systems, both running Void Linux and both using LXDE, though the GPUs are different.

Supported platforms:

- X11 (tested on Linux, theoretically works on various BSDs but nobody's tried yet)

Not quite supported platforms:

- Windows (tested on Linux via 32-bit Wine, currently only supports full screen capture)

Building is pretty bare, take a look at the build.sh script and modify it to your liking.

Or for Windows look at winbuild.sh.

If you want to write a version which works for a non-X11, non-Windows OS it's probably best to have that merged in. Take note that this is an extremely platform-specific library under the hood, so it will probably not share code with the X11 version or the Windows version, and after all those versions don't share code with each other.

This probably doesn't work on macOS.

I doubt this will ever work on Wayland due to the way it's architected, unless you want to make something using LD_PRELOAD...
