# basic pixel art tool

The idea is to provide a simple image making tool with relatively short loading times for slow potatoes.


- uses raylib + raygui and tinyfiledialogs
- runs on Linux (/Unix?) and Windows
- all dependencies are included
- **build**:
  - `make`: build with provided raylib version.
  - use `make build_from_global` to use globally installed raylib version.

  (make is not configured to run on Windows)
- to run: `make run` or `./imfap`

---

- supported formats: `.png` `.bmp` `.qoi` `.raw (rgba)`
- can load image from command line argument


## preconfigured for ease of use:

![a screenshot showing the default configuration](screenshot.png?raw=true)

## why?

[trying to ease the suffering](https://youtu.be/K7hWqxC_7Mw?t=6303)

