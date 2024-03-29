#pragma once

#include <set>
#include <string>

const uint16_t PORT = 8011;  // data plane service port
const std::string ZK_DEFAULT_HOST = "127.0.0.1:2181";
const std::string ZK_SVR_ROOT_PATH = "/servers";
const std::string ZK_CLI_ROOT_PATH = "/clients";
const int DEFAULT_REP_FACTOR = 1;
const size_t MR_SIZE = 1024 * 1024 * 100;
const char TAIL_MARKER = 255;  // a magic number
const std::set<std::string> HOST_ADDRS = {
    "localhost"
};
