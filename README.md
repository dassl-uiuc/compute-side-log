# compute-side-log

## Get
```bash
git clone git@github.com:dassl-uiuc/compute-side-log.git --recurse-submodules
```

## Install Dep
```bash
git submodule init
git submodule update
./install-deps.sh
```

## Install Zookeeper Lib
See https://zookeeper.apache.org/doc/r3.6.3/zookeeperProgrammers.html#Installation

## Build
```bash
cmake -S . -B build
cmake --build build
```