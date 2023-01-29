# Near-Compute-Log (compute-side-log)

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
```bash
sudo apt install maven libcppunit-dev -y
cd zookeeper/zookeeper-jute && mvn compile && cd ..
cd zookeeper-client/zookeeper-client-c
autoreconf -if
./configure --with-syncapi
make && sudo make install
```

## Install Zookeeper Server
```bash
wget https://archive.apache.org/dist/zookeeper/zookeeper-3.6.3/apache-zookeeper-3.6.3-bin.tar.gz
tar -zxvf apache-zookeeper-3.6.3-bin.tar.gz
```

## Build
```bash
sudo cp src/csl.h /usr/include
cmake -S . -B build
cmake --build build
```