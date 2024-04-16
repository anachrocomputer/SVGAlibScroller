# SVGAlibScroller #

This is an experimental program to generate a VGA display
resembling the game "Defender".
Apart from having a parallax-scrolling background of fields and clouds, that is.
Maybe it's more like "Scramble", but I've only played the Vectrex version of that game.

I wrote it in February 2012 as a second step towards a fully interactive program that could
display directly on the PC's screen.
An earlier iteration generated numerous PPM format still images and then assembled them,
along with a soundtrack, into a video file by using 'ffmpeg'.
But this version writes directly to the VGA screen and the ALSA sound subsystem.

Problem is, SVGAlib is long obsolete and superceded by SDL2,
the Simple DirectMedia Layer.
SVGAlib had several shortcomings,
and was only really useful on Linux systems that ran in text mode (i.e. without a GUI).
This repo is here as an example of how we wrote code back in the day,
and as a bit of a fossil (like myself).

The code never got as far as accepting user input in the function `game_logic()`.
I really wanted to use a joystick,
and that's what led me to move to SDL2 which supports input as well as output.
The program only runs for eight seconds and then terminates.

## Compilation Options ##

There's a #define in the code called MODE13H which will make the program run in
320x200, 256-colour mode.
If that #define is not defined, the program will run in 640x480 resolution.

## Compiling and Building ##

To compile this code, you'll need the usual 'build-essential' package:

`sudo apt install build-essential`

along with the SVGAlib libraries and header files:

`https://www.svgalib.org/`

And the ALSA sound subsystem libraries :

`https://www.alsa-project.org/wiki/Main_Page`

Once those are installed, you can simply run 'make':

`make`

