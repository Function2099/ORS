# `ignore/` — local-only (not committed)

Everything under this directory is gitignored except this README.

## Suggested layout

```
ignore/
├── README.md          # this file (tracked)
├── build/             # CMake build tree: cmake -B ignore/build -S .
├── out/               # optional install / package output
├── ide/               # optional: copy or symlink IDE caches if you want them here
└── local/             # personal notes, scratch files, secrets — never commit
```

## Rules

1. Do **not** put source code that should be shared in `ignore/`.
2. Prefer generating build artifacts into `ignore/build` (not a root `build/`).
3. If a tool writes files outside this folder, move them here or adjust that tool’s output path.
