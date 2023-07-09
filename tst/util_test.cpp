#include <gtest/gtest.h>

#include "../src/util.h"
#include "../src/ncl_stat.h"

TEST(ParseIpStrTest, TestSingleIp) {
    string ipstr = "127.0.0.1";
    set<string> ip;
    uint64_t ep;
    int cnt;
    tie(ep, cnt) = parseIpString(ipstr, ip);
    ASSERT_EQ(cnt, 1);
    ASSERT_EQ(ep, 0);
    ASSERT_EQ(ip.size(), 1);
    ASSERT_TRUE(ip.find("127.0.0.1") != ip.end());
}

TEST(ParseIpStrTest, TestMultiIp) {
    string ipstr = "127.0.0.1:127.0.0.2:127.0.0.3";
    set<string> ip;
    uint64_t ep;
    int cnt;
    tie(ep, cnt) = parseIpString(ipstr, ip);
    ASSERT_EQ(cnt, 3);
    ASSERT_EQ(ep, 0);
    ASSERT_EQ(ip.size(), 3);
    ASSERT_TRUE(ip.find("127.0.0.1") != ip.end());
    ASSERT_TRUE(ip.find("127.0.0.2") != ip.end());
    ASSERT_TRUE(ip.find("127.0.0.3") != ip.end());
}

TEST(ParseIpStrTest, TestNoIp) {
    string ipstr = "";
    set<string> ip;
    uint64_t ep;
    int cnt;
    tie(ep, cnt) = parseIpString(ipstr, ip);
    ASSERT_EQ(cnt, 0);
    ASSERT_EQ(ep, 0);
    ASSERT_TRUE(ip.empty());
}

TEST(ParseIpStrTest, TestSingleIpWithEpoch) {
    string ipstr = "1234567/127.0.0.1";
    set<string> ip;
    uint64_t ep;
    int cnt;
    tie(ep, cnt) = parseIpString(ipstr, ip);
    ASSERT_EQ(cnt, 1);
    ASSERT_EQ(ep, 1234567);
    ASSERT_EQ(ip.size(), 1);
    ASSERT_TRUE(ip.find("127.0.0.1") != ip.end());
}

TEST(ParseIpStrTest, TestMultiIpWithEpoch) {
    string ipstr = "1234567/127.0.0.1:127.0.0.2:127.0.0.3";
    set<string> ip;
    uint64_t ep;
    int cnt;
    tie(ep, cnt) = parseIpString(ipstr, ip);
    ASSERT_EQ(cnt, 3);
    ASSERT_EQ(ep, 1234567);
    ASSERT_EQ(ip.size(), 3);
    ASSERT_TRUE(ip.find("127.0.0.1") != ip.end());
    ASSERT_TRUE(ip.find("127.0.0.2") != ip.end());
    ASSERT_TRUE(ip.find("127.0.0.3") != ip.end());
}

TEST(ParseIpStrTest, TestNoIpWithEpoch) {
    string ipstr = "1234567/";
    set<string> ip;
    uint64_t ep;
    int cnt;
    tie(ep, cnt) = parseIpString(ipstr, ip);
    ASSERT_EQ(cnt, 0);
    ASSERT_EQ(ep, 1234567);
    ASSERT_TRUE(ip.empty());
}

TEST(ParseIpStrTest, TestSingleIpWithNoEpoch) {
    string ipstr = "/127.0.0.1";
    set<string> ip;
    uint64_t ep;
    int cnt;
    tie(ep, cnt) = parseIpString(ipstr, ip);
    ASSERT_EQ(cnt, 1);
    ASSERT_EQ(ep, 0);
    ASSERT_EQ(ip.size(), 1);
    ASSERT_TRUE(ip.find("127.0.0.1") != ip.end());
}

TEST(FileStringTest, TestGetExt) {
    string file1 = "file.ext";
    string file2 = "file";

    ASSERT_EQ(getFileExt(file1).compare("ext"), 0);
    ASSERT_TRUE(getFileExt(file2).empty());
}

TEST(FileStringTest, TestGetName) {
    string file1 = "/dir/file.ext";
    string file2 = "/dir/file";
    string file3 = "file.ext";
    string file4 = "file";

    ASSERT_EQ(getFileName(file1), "file");
    ASSERT_EQ(getFileName(file2), "file");
    ASSERT_EQ(getFileName(file3), "file");
    ASSERT_EQ(getFileName(file4), "file");
}

TEST(FileStringTest, TestGetParent) {
    string file1 = "/dir1/dir2/file.ext";
    string file2 = "/dir1/file.ext";
    string file3 = "dir1/file.ext";
    string file4 = "/file.ext";
    string file5 = "file.ext";

    ASSERT_EQ(getParentDir(file1), "dir2");
    ASSERT_EQ(getParentDir(file2), "dir1");
    ASSERT_EQ(getParentDir(file3), "dir1");
    ASSERT_EQ(getParentDir(file4), "");
    ASSERT_EQ(getParentDir(file5), "");
}

TEST(FileStringTest, TestCheckPrefix) {
    string file1 = "dir/prefile.ext";
    string file2 = "dir/grefile.ext";
    string file3 = "dir/pr.ext";
    string file4 = "dir/pre.ext";

    string prefix = "pre";

    ASSERT_TRUE(checkFileNamePrefix(file1, prefix));
    ASSERT_FALSE(checkFileNamePrefix(file2, prefix));
    ASSERT_FALSE(checkFileNamePrefix(file3, prefix));
    ASSERT_TRUE(checkFileNamePrefix(file4, prefix));
}
