#!/bin/bash

# insatll cmake
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | sudo apt-key add -
sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ bionic main'
sudo apt-get update
sudo apt-get install cmake libunwind-dev libgflags-dev -y

# install glog
git clone https://github.com/google/glog.git ../glog
cd ../glog
git checkout v0.6.0
cmake -S . -B build
cmake --build build
sudo cmake --build build --target install
