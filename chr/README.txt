chr.hpp is library for conversion of graphics files from older consoles (NES, SNES, ...).
These kind of graphics were called characters, hence the name.
The library supports any bpp (bits per pixel) value between 1-8 and two "data modes"
(refers to how bytes are laid out in a tile): planar (more straightforward, used for example
by the NES) and interwined (used by the SNES).
It also offers some palette support.
Although the library is mostly finished, I plan in the future to research other consoles' formats
and support them.
