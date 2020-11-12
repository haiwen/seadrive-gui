## seadrive-gui 

GUI part of seafile drive.

## Internationalization

You are welcome to add translation in your language.

### Contribute your translation

Please submit translations via Transifex: https://www.transifex.com/projects/p/seadrive-gui

Steps:

1. Create a free account on Transifex (https://www.transifex.com/).
2. Send a request to join the language translation.
3. After accepted by the project maintainer, you can translate online.

## Build

### Linux

#### Preparation

The following list is what you need to install on your development machine. You should install all of them before you build seadrive-gui.

Package names are according to Ubuntu 14.04. For other Linux distros, please find their corresponding names yourself.

* autoconf/automake/libtool
* libevent-dev ( 2.0 or later )
* libcurl4-openssl-dev  (1.0.0 or later)
* libgtk2.0-dev ( 2.24 or later)
* uuid-dev
* intltool (0.40 or later)
* libsqlite3-dev (3.7 or later)
* valac  (only needed if you build from git repo)
* libjansson-dev
* qtchooser
* qtbase5-dev
* libqt5webkit5-dev
* qttools5-dev
* qttools5-dev-tools
* valac
* cmake
* python-simplejson
* libssl-dev

```bash
sudo apt-get install autoconf automake libtool libevent-dev libcurl4-openssl-dev libgtk2.0-dev uuid-dev intltool libsqlite3-dev valac libjansson-dev cmake qtchooser qtbase5-dev libqt5webkit5-dev qttools5-dev qttools5-dev-tools libssl-dev
```

For a fresh Fedora or CentOS installation, the following will install all dependencies via YUM:

```bash
sudo yum install wget gcc libevent-devel openssl-devel gtk2-devel libuuid-devel sqlite-devel jansson-devel intltool cmake libtool vala gcc-c++ qt5-qtbase-devel qt5-qttools-devel qt5-qtwebkit-devel libcurl-devel openssl-devel
```

#### Building

1. First you should get the latest source of libsearpc/seadrive/seadrive-gui,
Download the source tarball of the latest tag from

* https://github.com/haiwen/libsearpc/tags (use v3.2-latest)
* https://github.com/haiwen/seadrive-fuse/tags
* https://github.com/haiwen/seadrive-gui/tags

For example, if the latest released seadrive gui client is 2.0.6, then just use the v2.0.6 tags of the three  projects. You should get three tarballs:

* libsearpc-v3.2-latest.tar.gz
* seadrive-fuse-2.0.6.tar.gz
* seadrive-gui-2.0.6.tar.gz

```bash
shopt -s expand_aliases
export version=2.0.6
alias wget='wget --content-disposition -nc'
wget https://github.com/haiwen/libsearpc/archive/v3.2-latest.tar.gz
wget https://github.com/haiwen/seadrive-fuse/archive/v${version}.tar.gz
wget https://github.com/haiwen/seadrive-gui/archive/v${version}.tar.gz
```

2. Now uncompress them

```bash
tar xf libsearpc-3.2-latest.tar.gz
tar xf seadrive-fuse-${version}.tar.gz
tar xf seadrive-gui-${version}.tar.gz
```

3. Build Seadrive gui

To build seadrive gui client you need first build libsearpc seadrive-fuse. To compile seadrive-fuse please refer to the link https://github.com/haiwen/seadrive-fuse/blob/master/README.md

```bash
set paths
export PREFIX=/usr
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH"
export PATH="$PREFIX/bin:$PATH"

cd seadrive-gui-${version}
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$PREFIX .
make
sudo make install
cd ..
```
