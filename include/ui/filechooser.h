#pragma once

#include <string>

class FileChooser {
public:
    FileChooser(std::string directory);

    std::string getDirectory();
    void setDirectory(std::string directory);
    char* startFileChooser(const char* extensions[], bool romExtensions, bool canQuit = false);

private:
    void updateScrollDown();
    void updateScrollUp();

    std::string directory;
    int selection = 0;
    int filesPerPage = 24;
    int numFiles = 0;
    int scrollY = 0;
    std::string matchFile = "";
};
