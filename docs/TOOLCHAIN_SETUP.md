# Toolchain Setup

This repository currently needs two local toolchains:

- Python 3.10+ with `pip`, `torch`, and `pytest`
- A C++17 compiler toolchain with CMake

## Windows Python setup

1. Install Python 3.11 or newer from `python.org`.
2. During install, enable `Add python.exe to PATH`.
3. Open a new terminal and verify:

```powershell
python --version
python -m pip --version
```

4. Install the project and test dependency:

```powershell
python -m pip install --upgrade pip
python -m pip install -e ".[dev]"
```

5. Or run the helper script from the repo root:

```powershell
.\scripts\windows\prepare-dev.ps1 -InstallPythonDeps -RunPyTests
```

6. Run the Python tests:

```powershell
python -m pytest -q
```

The root `pytest.ini` limits collection to `tests/`, so this command only runs
the active suite.

## Windows C++ setup

Recommended option:

1. Install "Visual Studio Build Tools 2022".
2. Select the workload "Desktop development with C++".
3. Make sure these components are included:
   - MSVC v143 toolset
   - Windows 10/11 SDK
   - CMake tools for Windows

Then open the "x64 Native Tools Command Prompt for VS 2022" and verify:

```powershell
cl
cmake --version
```

Build and test the C++ reference:

```powershell
cmake --preset vs2022-x64
cmake --build --preset release
cpp\build\Release\dmc_test.exe
```

## If you prefer Ninja

Install Ninja and use it from the same developer prompt:

```powershell
cmake -S cpp -B cpp\build -G Ninja
cmake --build cpp\build
cpp\build\dmc_test.exe
```

## Repo helper script

From the repo root, after the toolchains are installed:

```powershell
.\scripts\windows\prepare-dev.ps1 -InstallPythonDeps -BuildCpp -RunPyTests -RunCppTests
```

To configure the local coding app and LAN runtime together:

```powershell
.\scripts\windows\setup-continue.ps1
.\scripts\windows\start-llama-lan.ps1 -Use64KContext
```

Use `-Use128KContext` only when you specifically want to stress-test the
larger runtime preset.

## What was missing on this machine

- `python` was not available in `PATH`
- `py` was not available in `PATH`
- `winget` was not available in `PATH`
- `cl` was not available in `PATH`
- `msbuild` was not available in `PATH`
- `g++` was not available in `PATH`

## Minimal fix list

1. Install Python 3.11+ and add it to `PATH`.
2. Install `pytest` via `python -m pip install -e ".[dev]"`.
3. Install Visual Studio Build Tools 2022 with the C++ workload.
4. Open the VS developer prompt before building C++.
5. Re-run `python -m pytest -q`.
6. Re-run the C++ build and `dmc_test.exe`.
