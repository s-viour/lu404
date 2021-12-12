small win32 program to start `legouniverse.exe` and simultaneously host a local HTTP server that responds with `404 Not Found` to all TCP requests.

# usage
we have a wonderful tutorial located [here](https://github.com/s-viour/lu404/wiki/Quick-Guide)! it explains disclaimers, instructions and what to expect when setting up `lu404`.

# why use this?
no idea, but a friend of mine needed something like this because it made Lego Universe and [DarkflameServer](https://github.com/DarkflameUniverse/DarkflameServer) work!

# building
`lu404` is designed to be built on Windows, and requires C++17. build steps are more-or-less a standard CMake build process:
```
❯ mkdir build
❯ cmake --build build --config Release
```
