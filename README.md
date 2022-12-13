# compute-side-log

## Get
```bash
git clone git@github.com:dassl-uiuc/compute-side-log.git --recurse-submodules
```

## Install Dep
```bash
./install-deps.sh
```

## Install
```bash
git submodule update
make -C RDMA
cmake -S . -B build
cmake --build build
```