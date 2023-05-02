#!/bin/bash

# insatll cmake
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | sudo apt-key add -
sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ bionic main'
sudo apt-get update
sudo apt-get install cmake libunwind-dev libgflags-dev openjdk-11-jre-headless -y

# install glog
git clone https://github.com/google/glog.git ../glog
cd ../glog
git checkout v0.6.0
cmake -S . -B build
cmake --build build
sudo cmake --build build --target install

# install zookeeper
sudo apt install maven libcppunit-dev -y
cd ../zookeeper/zookeeper-jute && mvn compile && cd ..
cd zookeeper-client/zookeeper-client-c
autoreconf -if
./configure --with-syncapi
make && sudo make install
