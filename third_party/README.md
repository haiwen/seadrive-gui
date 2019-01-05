# How to build the MPMessagePack framework

We use the MPMessagePack library for the xpc between the main seadrive-gui program and the com.alphabox.alphadrive.helper helper tool.

The helper tool is a standalone executable, so we need to compile MPMessagePack as a static framework. Here is how to do that.

* Build the framework as a static lib
```sh
git clone git@github.com:haiwen/MPMessagePack.git
cd MPMessagePack
./build_static.sh
```

* Copy to "seadrive-gui/third_party" folder
```
rsync -a --delete build/Release/MPMessagePack.framework /path/to/seadrive-gui/third_party/
```
