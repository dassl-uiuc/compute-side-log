# Near-Compute-Log (compute-side-log)

## Get
```bash
git clone https://github.com/dassl-uiuc/compute-side-log.git --recurse-submodules
cd compute-side-log
```

## Install Dep

Install submodules and dependencies
```bash
git submodule init
git submodule update
./install-deps.sh
```

For Mellanox NIC, use `RDMA/install.sh` to install RDMA driver. You may change the exact version of MLNX_OFED to match your own operating system. See https://network.nvidia.com/products/infiniband-drivers/linux/mlnx_ofed/ for detail.

For other RDMA NIC, please refer to vendor for installation instructions.

Install zookeeper server
```bash
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
# dataDir=./zookeeper
cd ..
```
Start zookeeper
```bash
./bin/zkServer.sh start
cd ../compute-side-log
```

## Configure
### Config RDMA library
Edit `RDMA/src/infinity/core/Configuration.h`
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
// line 11: edit this to be the size (in bytes) of memory region to be registered on each replica
const size_t MR_SIZE = 1024 * 1024 * 100;

```

## Build
```bash
cd compute-side-log
sudo cp src/csl.h /usr/include  # NCL header file
cmake -S . -B build
cmake --build build
```

## Run test
```bash
# start server
./build/src/server
# start client
./build/src/posix_client
```
The server process prints the first 128 bytes of each memory region every second. You should see it print 128 *s in this test.
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