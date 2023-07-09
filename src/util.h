#pragma once

#include <string>
#include <sstream>
#include <set>
#include <tuple>
#include <zookeeper/zookeeper.h>

using std::string;
using std::stringstream;
using std::set;
using std::tuple;
using std::tie;
using std::make_tuple;

static const char* state2String(int state){
  if (state == 0)
    return "CLOSED_STATE";
  if (state == ZOO_CONNECTING_STATE)
    return "CONNECTING_STATE";
  if (state == ZOO_ASSOCIATING_STATE)
    return "ASSOCIATING_STATE";
  if (state == ZOO_CONNECTED_STATE)
    return "CONNECTED_STATE";
  if (state == ZOO_READONLY_STATE)
    return "READONLY_STATE";
  if (state == ZOO_EXPIRED_SESSION_STATE)
    return "EXPIRED_SESSION_STATE";
  if (state == ZOO_AUTH_FAILED_STATE)
    return "AUTH_FAILED_STATE";

  return "INVALID_STATE";
}

static const char* type2String(int type){
  if (type == ZOO_CREATED_EVENT)
    return "CREATED_EVENT";
  if (type == ZOO_DELETED_EVENT)
    return "DELETED_EVENT";
  if (type == ZOO_CHANGED_EVENT)
    return "CHANGED_EVENT";
  if (type == ZOO_CHILD_EVENT)
    return "CHILD_EVENT";
  if (type == ZOO_SESSION_EVENT)
    return "SESSION_EVENT";
  if (type == ZOO_NOTWATCHING_EVENT)
    return "NOTWATCHING_EVENT";

  return "UNKNOWN_EVENT_TYPE";
}

/**
 * Generate ip string concatenated with ":"
 */
string generateIpString(set<string> &peers, uint64_t epoch=0) {
  stringstream peers_str;
  if (epoch != 0)
    peers_str << epoch << "/";
  if (peers.empty())
    return peers_str.str();
  auto it = peers.begin();
  peers_str << *(it++);
  for (; it != peers.end(); ++it)
    peers_str << ":" << *it;
  return peers_str.str();
}

/**
 * Separate ip string concatenated with ":"
 * @return epoch and number of ips parsed
 */
tuple<uint64_t, int> parseIpString(string &peers_str, set<string> &peers) {
  if (peers_str.empty())
    return make_tuple(0, 0);
  int pos = 0, next_pos = -1, cnt = 0;
  uint64_t epoch = 0;
  // parse epoch number
  next_pos = peers_str.find('/', pos);
  if (next_pos != -1 && next_pos != 0) {
    epoch = stoul(peers_str.substr(pos, next_pos - pos));
  }
  // parse peer ips
  if (next_pos == peers_str.length() - 1)
    return make_tuple(epoch, 0);
  do {
    pos = next_pos + 1;
    next_pos = peers_str.find(':', pos);
    peers.insert(peers_str.substr(pos, next_pos - pos));
    cnt++;
  } while(next_pos != -1);
  return make_tuple(epoch, cnt);
}

string getPeerFromPath(const char *path) {
  string peer;
  stringstream pathss(path);
  // node path: /servers/<peer>
  getline(pathss, peer, '/');
  getline(pathss, peer, '/');
  getline(pathss, peer, '/');
  return peer;
}
