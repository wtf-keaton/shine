# Contributing to Shine

Thanks for contributing! This repository is still evolving, so small, focused PRs are the fastest path to getting changes merged.

## Before you start

- **Search existing issues/PRs** first.
- If you’re planning a bigger change, **open an issue** describing the approach and trade-offs.

## Development setup

### Prerequisites

- **CMake** 3.20+
- A **C++20** toolchain
  - Windows: Visual Studio (MSVC) or MinGW
- **Node.js** (for `create-shine-app` and frontend tooling)

### Repo layout

- `core/`: runtime (window, webview, IPC)
- `components/`: optional native components
- `create-shine-app/`: the scaffolding CLI
- `samples/`: example apps

## Building

### C++ (core + components)

Configure and build with CMake (example):

```bash
cmake -S . -B build
cmake --build build
```

### create-shine-app

Install deps:

```bash
cd create-shine-app
npm install
```

Run the generator locally (example):

```bash
node create-shine-app/index.js
```

## Style & conventions

- **Keep PRs small** and focused.
- Prefer **clear names** and straightforward control flow.
- Avoid introducing new dependencies unless necessary.

### C++ guidelines

- Use **C++20**.
- Prefer RAII and standard library types.
- Keep platform-specific code isolated under `core/src/engine/<platform>/`.

### JavaScript/Node guidelines

- Use modern Node (ESM).
- Keep scripts **cross-platform** when possible.

## Submitting a PR

- Include a short description of **what** changed and **why**.
- Add a simple **test plan** in the PR description (how you validated the change).
- If your change affects the generated template, verify by running the generator and building the resulting app.

