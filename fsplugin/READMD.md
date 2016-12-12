## About

Finder plugin for seadrive app.

## Install the fsplugin during development

If your system are macos sierra (a.k.a. 10.12), you need to do the following to
make the finder plugin work.

```
cd seadrive-gui/fsplugin
./build.sh

mkdir -p /Applications/SeaDrive.app/Contents/Plugins/
cp -a "SeaDrive FinderSync.appex" /Applications/SeaDrive.app/Contents/Plugins/
pluginkit -a /Applications/SeaDrive.app/Contents/Plugins/SeaDrive\ FinderSync.appex/
```
