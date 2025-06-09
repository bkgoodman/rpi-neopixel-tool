Raspberry Pi Neopixel Command line tool
==========

The bulk of this code was shamelessly stolen from the https://github.com/rpi-ws281x/
project from which the majority of the code and the following sections of the README
were taken.

This is a simple command line tool to allow you to set a number of pixels to colors.

Run like:

`./neosign -x 3 ff0000 00ff00 0000ff`

Pipe option allows you to specify a named pipe (`-p filename`). Subsequent writes to the named
pipe with update the display with the given text.

# Important flags
For a complete list of flags, use `--help`. But important ones are:

`-x` Number of pixels in array
`-p` Create and listen on named pipe for new text. (echo or write to pipe to update text)
`-c` Clear display on exit

# Pipe Commands

When you run with `-p filename` mode, you create a named pipe which you can write subsequent commands to. Pipe Commands:

| Prefix | Value | Description |
|--------|-------|-------------|
| ! | Decimal Number | Animation Delay in uSec | 
| @ | Decimal Digit | Animation Mode | 
| None | 6 digit hex value | RGB Pixel color |


Specify as many pixel colors as you want. 

Example:

`echo -n '@3 !80000  200000' >> testpipe`

## Animation Modes

| Number | Name | Description |
|--------|------|-------------|
| 0 | Normal | Static - Does not move - Optimized for Low CPU Usage | 
| 1 | Fade |  Turns on, then fades out |
| 2 | Blink |  Fades in and out |
| 3 | Wink |  Fader Animation |
| 3 | Fade |  Dark walking spot |

# Building

```
git submodule update --init --recursive
cmake .
make
```

