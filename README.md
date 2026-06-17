# PracticeDawn

## Native build

```sh
cmake -S . -B build -G Ninja
cmake --build build
```

## Web build

Activate Emscripten first:

```sh
export EMSDK_QUIET=1
source /Users/sungminwoo/Documents/GitHub/emsdk/emsdk_env.sh
```

Then configure and build:

```sh
emcmake cmake -S . -B build-web -G Ninja
cmake --build build-web
```

Serve the generated app:

```sh
cd build-web/app
python3 -m http.server 8080
```

Open this URL in a WebGPU-capable browser:

```text
http://localhost:8080/PracticeDawn.html
```
