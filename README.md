Raspberry Pi Neopixel Command line tool
==========

The bulk of this code was shamelessly stolen from the https://github.com/rpi-ws281x/
project from which the majority of the code and the following sections of the README
were taken.

This is a simple command line tool to allow you to set a number of pixels to colors.

Run like:

`./neosign -x 3 ff0000 00ff00 0000ff`

A command (like above) will issue a "one-shot" color sequence to the pixels, then exit. But use of Pipe Command allows a sequence to continue to animate until completion.

Pipe option allows you to specify a named pipe (`-p filename`). Subsequent writes to the named
pipe with update the display with the given text.

# Important flags
For a complete list of flags, use `--help`. But important ones are:

`-x` Number of pixels in array
`-p` Create and listen on named pipe for new text. (echo or write to pipe to update text)
`-c` Clear display on exit
`-m` Start with this pipe command (continue to animate)

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
| 4 | Fade |  Dark walking spot |
| 5 | Sine |  Sine wave |
| q | Exit |  Terminate Program |

# Building

```
git submodule update --init --recursive
cmake .
make
```

# Pins

Important notes from C code:

```
        PWM0, which can be set to use GPIOs 12, 18, 40, and 52.
        Only 12 (pin 32) and 18 (pin 12) are available on the B+/2B/3B
        PWM1 which can be set to use GPIOs 13, 19, 41, 45 and 53.
        Only 13 is available on the B+/2B/PiZero/3B, on pin 33
        PCM_DOUT, which can be set to use GPIOs 21 and 31.
        Only 21 is available on the B+/2B/PiZero/3B, on pin 40.
        SPI0-MOSI is available on GPIOs 10 and 38.
        Only GPIO 10 is available on all models.

        The library checks if the specified gpio is available
        on the specific model (from model B rev 1 till 3B)
```

