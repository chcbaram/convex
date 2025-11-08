# convex-qmk
- Custom keyboard firmware 
- Key input processing is based on QMK firmware

## Specification
- ST STM32U5A5RJT6
- Polling rate 8Khz
- Scan rate 8Khz or higher
- VIA support (via JSON)

## Development environment
- CMake
- Make
- GCC ARM
  - Requires setting environment variables (Windows)
    - ARM_TOOLCHAIN_DIR
    - ex) D:/tools/gcc-arm-none-eabi-9-2020-q2-update-win32/bin
- Python


## How To Build
### CMake Configure (Mac/Linux)
```
cmake -S . -B build -DKEYBOARD_PATH='/keyboards/convex'
```

### CMake Configure (Windows)
```
cmake -S . -B build -G "MinGW Makefiles" -DKEYBOARD_PATH='/keyboards/convex'
```

### CMake Build
```
cmake --build build -j10
```


