#pragma once

#include <set>
#include <string>

const uint16_t PORT = 8011;
const uint16_t CSL_MGMT_PORT = 2181;
const std::string ZK_ROOT_PATH = "/servers";
const size_t MR_SIZE = 1024 * 1024 * 100;
const std::set<std::string> HOST_ADDRS = {
    "localhost"
};
