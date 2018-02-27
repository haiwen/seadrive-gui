# How to install Cocoa dependencies

* Install carthage
```sh
wget https://github.com/Carthage/Carthage/releases/download/0.28.0/Carthage.pkg
open Carthage.pkg
```

* Install deps:
```sh
Carthage/carthage_static_build.sh
```

* Copy to thirdparty folder
```
mkdir -p third_party/carthage
rsync -a --delete Carthage/Build/Mac/ third_party/carthage
```
