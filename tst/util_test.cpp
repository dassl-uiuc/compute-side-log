#include <gtest/gtest.h>

#include "../src/util.h"

TEST(ParseIpStrTest, TestSingleIp) {
    string ipstr = "127.0.0.1";
    set<string> ip;
    ASSERT_NO_THROW(ip = parseIpString(ipstr));
    ASSERT_EQ(ip.size(), 1);
    ASSERT_TRUE(ip.find("127.0.0.1") != ip.end());
}

TEST(ParseIpStrTest, TestMultiIp) {
    string ipstr = "127.0.0.1:127.0.0.2:127.0.0.3";
    set<string> ip;
    ASSERT_NO_THROW(ip = parseIpString(ipstr));
    ASSERT_EQ(ip.size(), 3);
    ASSERT_TRUE(ip.find("127.0.0.1") != ip.end());
    ASSERT_TRUE(ip.find("127.0.0.2") != ip.end());
    ASSERT_TRUE(ip.find("127.0.0.3") != ip.end());
}

TEST(ParseIpStrTest, TestNoIp) {
    string ipstr = "";
    set<string> ip;
    ASSERT_NO_THROW(ip = parseIpString(ipstr));
    ASSERT_TRUE(ip.empty());
}
