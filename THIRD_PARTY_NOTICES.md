# Third-party software

Compute Gallery is built with the following components. Each remains under its own license;
the full texts are at the linked locations, and the font licenses are also in `assets/fonts`.

| Component | Used for | License |
|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | Gallery user interface | MIT |
| [GLFW](https://github.com/glfw/glfw) | Windows, OpenGL contexts, input | zlib/libpng |
| [glad](https://github.com/Dav1dde/glad) | OpenGL function loader | MIT (generator); generated code derived from Khronos specifications under Apache 2.0 |
| [GLM](https://github.com/g-truc/glm) | Vector and matrix math | MIT (or The Happy Bunny License) |
| [stb_image / stb_image_write](https://github.com/nothings/stb) | Image loading and screenshots | MIT or public domain (Unlicense) |
| [nlohmann/json](https://github.com/nlohmann/json) | Reading demo manifests | MIT |
| [OpenCL-Headers](https://github.com/KhronosGroup/OpenCL-Headers) | OpenCL API declarations | Apache 2.0 |
| [SDL2](https://github.com/libsdl-org/SDL), [SDL2_image](https://github.com/libsdl-org/SDL_image) | Image demos | zlib |
| [Inter](https://github.com/rsms/inter) (The Inter Project Authors) | Interface font | SIL Open Font License 1.1 |
| [Lucide](https://lucide.dev) icons (Lucide Contributors) | Interface icons | ISC |
| [Cousine](https://fonts.google.com/specimen/Cousine) (Steve Matteson) | Console font | SIL Open Font License 1.1 |

Windows packages also contain DLLs from [MSYS2](https://www.msys2.org/) that these libraries
depend on; their license files are included in the package's `licenses/` folder.
