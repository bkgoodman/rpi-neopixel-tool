Raspberry Pi Neopixel Command line tool
==========

The bulk of this code was shamelessly stolen from the https://github.com/rpi-ws281x/
project from which the majority of the code and the following sections of the README
were taken.

This is a simple command line tool to allow you to set a number of pixels to colors.

Run like:

`./neosign -x 3 ff0000 00ff00 0000ff`

Mos

# Important flags
For a complete list of flags, use `--help`. But important ones are:

`-x` Number of pixels in array
`-c` Clear display on exit

# Building

```
git submodule update --init --recursive
cmake .
make
```

