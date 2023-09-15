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
sudo cp src/csl.h /usr/include
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
