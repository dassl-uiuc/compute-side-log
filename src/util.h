#pragma once

#include <string>
#include <set>
#include <zookeeper/zookeeper.h>

using std::string;
using std::set;

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
string generateIpString(set<string> peers) {
  if (peers.empty())
    return "";
  auto it = peers.begin();
  string peers_str = *(it++);
  for (; it != peers.end(); ++it)
    peers_str.append(":").append(*it);
  return peers_str;
}

/**
 * Separate ip string concatenated with ":" 
 */
set<string> parseIpString(string peers_str) {
  if (peers_str.empty())
    return {};
  set<string> peers;
  int pos, next_pos = -1;
  do {
    pos = next_pos + 1;
    next_pos = peers_str.find(':', pos);
    peers.insert(peers_str.substr(pos, next_pos - pos));
  } while(next_pos != -1);
  return peers;
}
