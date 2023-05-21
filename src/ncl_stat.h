#pragma once
#include <string>

using namespace std;

string getFileExt(const string &s) {
    size_t i = s.rfind('.', s.length());
    if (i != string::npos) {
        return (s.substr(i + 1, s.length() - i));
    }

    return ("");
}

string getFileName(const string &s) {
    size_t l = s.rfind('/', s.length());
    size_t r = s.rfind('.', s.length());
    if (l != string::npos && r != string::npos) {
        return (s.substr(l + 1, r - l - 1));
    } else if (l != string::npos) {
        return (s.substr(l + 1, s.length() - l));
    } else if (r != string::npos) {
        return (s.substr(0, r));
    } else {
        return s;
    }
}

string getParentDir(const string &s) {
    size_t r = s.rfind('/', s.length());
    if (r == 0)
        return ("");
    if (r != string::npos) {
        size_t l = s.rfind('/', r - 1);
        if (l != string::npos) {
            return s.substr(l + 1, r - l - 1);
        } else {
            return s.substr(0, r);
        }
    } else {
        return ("");
    }
}

bool checkFileNamePrefix(const string &s, const string &prefix) {
    string filename = getFileName(s);
    return filename.compare(0, prefix.size(), prefix) == 0;
}
