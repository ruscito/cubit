<p align="center">
  <img src="assets/cubit_logo_512px.png" alt="Cubit Engine Logo" width="400"/>
</p>

## About

**Cubit** is a lightweight 3D game engine built from scratch in C. It provides a clean API for rendering, input handling, and camera control.

## Features

- 🎮 Simple and intuitive API
- 📦 Minimal dependencies (GLFW, OpenGL)
- 🎥 Flexible camera system (free and target modes)
- ⌨️ Complete input handling (keyboard, mouse)
- 🔺 Mesh rendering with custom shaders
- 🧊 3D transformations and matrix math

## Quick Start

```c
#include "cubit.h"

void application_config(app_config* cfg) {
    cfg->width = 1600;
    cfg->height = 900;
    cfg->title = "My Cubit Game";
}

void application_init(void) {
    // Initialize your game objects
}

void application_update(double dt) {
    fill_screen((color_t){0.2f, 0.3f, 0.3f, 1.0f});
    // Update and render your game
}

void application_shutdown(void) {
    // Clean up resources
}
```

## Building

```bash
# Coming soon
```

## License

MIT License - See LICENSE file for details

## Status

🚧 **Early Development** - API subject to change

---

<p align="center">
  Made with ❤️ for game developers
</p>
