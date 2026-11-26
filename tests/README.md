The host smoke test builds the production SDL material independently of Switch services:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -Iprojects/uMenu/include \
  tests/liquid_glass.cpp projects/uMenu/source/ul/menu/ui/ui_LiquidGlass.cpp \
  $(sdl2-config --cflags --libs) -o /tmp/liquid-glass-smoke
SDL_VIDEODRIVER=offscreen /tmp/liquid-glass-smoke
```

Requires SDL 2.0.18+ and an accelerated backend. Linux Mesa's offscreen backend works. Checks restored renderer target, viewport, clip, scale, draw color and blend mode; repeated blur changes; rounded corner coverage; and reduced high-frequency contrast at maximum blur. This is a correctness check, not a Switch performance measurement. No framebuffer readback occurs in production; the test uses readback for assertions.
