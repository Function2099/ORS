# Building ORS

> Placeholder — environment setup and build steps will be added in Phase 0.

## Requirements (planned)

- Windows 10 1903+ or Windows 11
- Visual Studio 2022 (Desktop development with C++)
- CMake 3.20+
- Qt 6.5+ (MSVC 64-bit kit)

## Build output location

Put the CMake build tree under `ignore/` so it is never committed:

```powershell
cmake -B ignore/build -S . -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build ignore/build --config Release
```

See `ignore/README.md` for the local-only folder convention.
