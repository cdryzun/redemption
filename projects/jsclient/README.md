# Install Emscripten

https://emscripten.org/docs/getting_started/downloads.html

```sh
git clone https://github.com/emscripten-core/emsdk.git &&
cd emsdk &&
./emsdk install latest &&
./emsdk activate latest
```
<!-- version: 2.0.9 -->

## Test

```cpp
// test.cpp
#include <cstdio>
int main()
{
    puts("ok");
}
```

```bash
source ./emsdk_env.sh
em++ test.cpp -o test.js
node test.js
```


# Compile RdpClient

## Configure boost path for test targets

    mkdir system_include/
    ln -s /usr/include/boost system_include/

## Configure Emscripten

    source $EMSDK_PATH/emsdk_env.sh

## Run bjam

    bjam -j7 js_client

Debug mode:

    bjam -j7 debug js_client

And tests with

    bjam -j7 install_node_modules
    bjam -j7

If you get some undefined symbol error, run `bjam targets.jam`.

## Debug with source-map

    npm install source-map-support

    bjam debug -s debug-symbols-source-map=on ....


## Install

    bjam -j7 -s copy_application_mode=symbolic rdpclient

Set module name with `-s JS_MODULE_NAME=xxx`

Set source map with `-s JS_SOURCE_MAP='prefix//'`

By default `copy_application_mode` is equal to `copy`


# Open client

## Enable websocket

Inner `rdpproxy.ini`.

```ini
[globals]
enable_websocket=yes
```

Or use [websocat](https://github.com/vi/websocat) (websocket to tcp).

```
websocat --binary ws-l:127.0.0.1:8080 tcp:127.0.0.1:3389
```

## Open rdpclient.html

Create a server at build directory (see `bjam cwd`).

    cd "$(bjam cwd | sed '/^CWD/!d;s/^CWD: //')"
    python3 -m http.server 7453 --bind 127.0.0.1
    xdg-open http://localhost:7453/client.html

Or run

    ./tools/open_client.sh
    # or
    ./tools/open_client.sh debug

Or with parameters

    BROWSER=... WS=... USER=... PASSWORD=... PORT=... ./tools/open_client.sh
