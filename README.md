# Near-Compute-Log (compute-side-log)

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

## Publication
This repo is the artifact of:

> [Xuhao Luo, Ramnatthan Alagappan, and Aishwarya Ganesan.
**SplitFT: Fault Tolerance for Disaggregated Datacenters via Remote
Memory Logging.** In *European Conference on Computer Systems
(EuroSys ’24), April 22–25, 2024, Athens, Greece.*](https://doi.org/10.1145/3627703.3629561)

## Organization
This repo is organized as follow:
```
Near-Compute-Log/
├─ RDMA/ (a C++ RDMA library)
├─ src/ (source code for NCL)
│  ├─ rdma/ (rdma related code)
│  ├─ csl.cc (top level code for NCL library)
│  ├─ csl_config.h (configurations)
│  ├─ server.cpp (NCL replication peer code)
│  ├─ client.cpp (an example client that uses NCL internal api)
│  ├─ posix_client.cpp (an example client that uses POSIX api)
│  ├─ ...
├─ tst/ (some test code)
```

## Download
```bash
git clone https://github.com/dassl-uiuc/compute-side-log.git --recurse-submodules
cd compute-side-log
```

## Install Dep

### Install submodules and dependencies
```bash
git submodule init
git submodule update
./install-deps.sh
sudo ldconfig
```

### Install RDMA Driver
For Mellanox NIC, use `RDMA/install.sh` to install RDMA driver. You may change the exact version of MLNX_OFED to match your own operating system. See https://network.nvidia.com/products/infiniband-drivers/linux/mlnx_ofed/ for detail.

For other RDMA NIC, please refer to vendor for installation instructions.

### Install zookeeper server
Zookeeper can be installed on any server that is accessible to all client and servers. You may install it on the client machine for convenience.
```bash
# install jvm if you haven't
sudo apt install -y openjdk-11-jre-headless
# Download zookeeper binary
cd ..
wget https://archive.apache.org/dist/zookeeper/zookeeper-3.6.3/apache-zookeeper-3.6.3-bin.tar.gz
tar -zxvf apache-zookeeper-3.6.3-bin.tar.gz
cd apache-zookeeper-3.6.3-bin
```
Edit zookeeper config
```bash
cd conf
cp zoo_sample.cfg zoo.cfg
# Edit line 12 of zoo.cfg to
# dataDir=${PATH_TO_ZOOKEEPER_FOLDER}/zookeeper
cd ..
```
Start zookeeper
```bash
./bin/zkServer.sh start
cd ../compute-side-log
```

## Configure
### Config RDMA library
The RDMA configuration is automatically generated at compile time. In case you need to edit it,
edit `RDMA/src/infinity/core/Configuration.h`
```c++
// line 47: edit this to be the device name of your RNIC (can be obtained from `ifconfig`)
static constexpr const char* DEFAULT_NET_DEVICE = "enp0s31f6";
// line 49: edit this to be the hca_id of your IB device (can be obtained from `ibv_devinfo`)
static constexpr const char* DEFAULT_IB_DEVICE = "rxe_0"
// line 51: edit this to be the physical port number of IB device (can be obtained from `ibv_devinfo`)
static constexpr const uint16_t DEFAULT_IB_PHY_PORT = 1;
```
### Config NCL library
Edit `src/csl-config.h`
```c++
// line 7: edit this to be the ip address of the server running zookeeper (the previous step)
const std::string ZK_DEFAULT_HOST = "127.0.0.1:2181";
// line 10: edit this to be the number of replicas
const int DEFAULT_REP_FACTOR = 1;
// line 11: edit this to be the size (in bytes) of memory region to be registered for each file on each replica
const size_t MR_SIZE = 1024 * 1024 * 100;

```

## Build
```bash
cd compute-side-log
sudo cp src/csl.h /usr/include  # install NCL header file
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
The binaries will be in `./build/src/`, which contains:
- `libcsl.so`: The NCL library
- `server`: The NCL replication peer
- `posix_client`: The example POSIX client


## Run test
First, download, install deps, configure, and build NCL on all client and servers. You may also only build it on one machine, and mount the src code folder to all other machines via NFS. But zookeeper client library and gflags need to be installed on all machines
```bash
# after mounting 
cd ${NCL_DIR}/zookeeper/zookeeper-client/zookeeper-client-c && sudo make install
sudo apt install -y libgflags-dev
sudo ldconfig
```
First start server process on every server machine, then run client process.
```bash
# start server process on every server machine. The number of server machines needed is specified in DEFAULT_REP_FACTOR in csl_config.h
./build/src/server
# start client
./build/src/posix_client 128 w ./test.txt ncl
```
What the posix client does is to keep writing a fixed value (42 here) until the memory region is filled up.

The server process prints the first 128 bytes of each memory region every second. You should see it prints 128 '*' (ASCII code 42) in this test.
Press Ctrl+C to exit the server process.

## General Usage
To make a file backed by NCL, just add the NCL flag `O_CSL` when creating the file.
```c
#include <csl.h>

int fd = open("test.txt", O_RDWR | O_CREAT | O_CSL, 0644);
```
The file should not have content in it. Currently NCL does not support backing a file that has existed content.

Developer should ensure that the file size will not exceed the configured `MR_SIZE`. Typically, write-ahead-log files are small and have a configurable size limit.

Then preload the NCL library when running the process (assume NCL servers are already running on replication peers).
```bash
LD_PRELOAD=${PATH_TO_LIB}/libcsl.so ./app
```

## Support Platform
Near-Compute-Log is tested on the following platform:
- OS: Ubuntu 20.04 LTS
- NIC: Mellanox MT27710 Family ConnectX-4 Lx (25Gb RoCE)
- RDMA Driver: MLNX_OFED-5.4-3.6.8.1
