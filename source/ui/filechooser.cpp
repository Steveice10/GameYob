#include <sys/dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "platform/gfx.h"
#include "platform/input.h"
#include "platform/system.h"
#include "ui/config.h"
#include "ui/filechooser.h"

#define FLAG_DIRECTORY  1
#define FLAG_SUSPENDED  2
#define FLAG_ROM        4

using namespace std;

FileChooser::FileChooser(std::string directory) {
    this->directory = directory;
}

void FileChooser::updateScrollDown() {
    if(selection >= numFiles) {
        selection = numFiles - 1;
    }

    if(numFiles > filesPerPage) {
        if(selection == numFiles - 1) {
            scrollY = selection - filesPerPage + 1;
        } else if(selection - scrollY >= filesPerPage - 1) {
            scrollY = selection - filesPerPage + 2;
        }
    }
}

void FileChooser::updateScrollUp() {
    if(selection < 0) {
        selection = 0;
    }

    if(selection == 0) {
        scrollY = 0;
    } else if(selection == scrollY) {
        scrollY--;
    } else if(selection < scrollY) {
        scrollY = selection - 1;
    }

}

int nameSortFunction(string &a, string &b) {
    // ".." sorts before everything except itself.
    bool aIsParent = strcmp(a.c_str(), "..") == 0;
    bool bIsParent = strcmp(b.c_str(), "..") == 0;

    if(aIsParent && bIsParent) {
        return 0;
    } else if(aIsParent) {// Sorts before
        return -1;
    } else if(bIsParent) {// Sorts after
        return 1;
    } else {
        return strcasecmp(a.c_str(), b.c_str());
    }
}

/*
 * Determines whether a portion of a vector is sorted.
 * Input assertions: 'from' and 'to' are valid indices into data. 'to' can be
 *   the maximum value for the type 'unsigned int'.
 * Input: 'data', data vector, possibly sorted.
 *        'sortFunction', function determining the sort order of two elements.
 *        'from', index of the first element in the range to test.
 *        'to', index of the last element in the range to test.
 * Output: true if, for any valid index 'i' such as from <= i < to,
 *   data[i] < data[i + 1].
 *   true if the range is one or no elements, or if from > to.
 *   false otherwise.
 */
template<class Data> bool isSorted(std::vector<Data> &data, int (* sortFunction)(Data &, Data &), const unsigned int from, const unsigned int to) {
    if(from >= to) {
        return true;
    }

    Data* prev = &data[from];
    for(unsigned int i = from + 1; i < to; i++) {
        if((*sortFunction)(*prev, data[i]) > 0) {
            return false;
        }

        prev = &data[i];
    }

    if((*sortFunction)(*prev, data[to]) > 0) {
        return false;
    }

    return true;
}

/*
 * Chooses a pivot for Quicksort. Uses the median-of-three search algorithm
 * first proposed by Robert Sedgewick.
 * Input assertions: 'from' and 'to' are valid indices into data. 'to' can be
 *   the maximum value for the type 'unsigned int'.
 * Input: 'data', data vector.
 *        'sortFunction', function determining the sort order of two elements.
 *        'from', index of the first element in the range to be sorted.
 *        'to', index of the last element in the range to be sorted.
 * Output: a valid index into data, between 'from' and 'to' inclusive.
 */
template<class Data> unsigned int choosePivot(std::vector<Data> &data, int (* sortFunction)(Data &, Data &), const unsigned int from, const unsigned int to) {
    // The highest of the two extremities is calculated first.
    unsigned int highest = ((*sortFunction)(data[from], data[to]) > 0) ? from : to;
    // Then the lowest of that highest extremity and the middle
    // becomes the pivot.
    return ((*sortFunction)(data[from + (to - from) / 2], data[highest]) < 0) ? (from + (to - from) / 2) : highest;
}

/*
 * Partition function for Quicksort. Moves elements such that those that are
 * less than the pivot end up before it in the data vector.
 * Input assertions: 'from', 'to' and 'pivotIndex' are valid indices into data.
 *   'to' can be the maximum value for the type 'unsigned int'.
 * Input: 'data', data vector.
 *        'metadata', data describing the values in 'data'.
 *        'sortFunction', function determining the sort order of two elements.
 *        'from', index of the first element in the range to sort.
 *        'to', index of the last element in the range to sort.
 *        'pivotIndex', index of the value chosen as the pivot.
 * Output: the index of the value chosen as the pivot after it has been moved
 *   after all the values that are less than it.
 */
template<class Data, class Metadata> unsigned int partition(std::vector<Data> &data, std::vector<Metadata> &metadata, int (* sortFunction)(Data &, Data &), const unsigned int from, const unsigned int to, const unsigned int pivotIndex) {
    Data pivotValue = data[pivotIndex];
    data[pivotIndex] = data[to];
    data[to] = pivotValue;

    const Metadata tM = metadata[pivotIndex];
    metadata[pivotIndex] = metadata[to];
    metadata[to] = tM;

    unsigned int storeIndex = from;
    for(unsigned int i = from; i < to; i++) {
        if((*sortFunction)(data[i], pivotValue) < 0) {
            const Data tD = data[storeIndex];
            data[storeIndex] = data[i];
            data[i] = tD;
            const Metadata tM2 = metadata[storeIndex];
            metadata[storeIndex] = metadata[i];
            metadata[i] = tM2;
            ++storeIndex;
        }
    }

    const Data tD = data[to];
    data[to] = data[storeIndex];
    data[storeIndex] = tD;
    const Metadata tM2 = metadata[to];
    metadata[to] = metadata[storeIndex];
    metadata[storeIndex] = tM2;
    return storeIndex;
}

/*
 * Sorts an array while keeping metadata in sync.
 * This sort is unstable and its average performance is
 *   O(data.size() * log2(data.size()).
 * Input assertions: for any valid index 'i' in data, index 'i' is valid in
 *   metadata. 'from' and 'to' are valid indices into data. 'to' can be
 *   the maximum value for the type 'unsigned int'.
 * Invariant: index 'i' in metadata describes index 'i' in data.
 * Input: 'data', data to sort.
 *        'metadata', data describing the values in 'data'.
 *        'sortFunction', function determining the sort order of two elements.
 *        'from', index of the first element in the range to sort.
 *        'to', index of the last element in the range to sort.
 */
template<class Data, class Metadata> void quickSort(std::vector<Data> &data, std::vector<Metadata> &metadata, int (* sortFunction)(Data &, Data &), const unsigned int from, const unsigned int to) {
    if(isSorted(data, sortFunction, from, to)) {
        return;
    }

    unsigned int pivotIndex = choosePivot(data, sortFunction, from, to);
    unsigned int newPivotIndex = partition(data, metadata, sortFunction, from, to, pivotIndex);
    if(newPivotIndex > 0) {
        quickSort(data, metadata, sortFunction, from, newPivotIndex - 1);
    }

    if(newPivotIndex < to) {
        quickSort(data, metadata, sortFunction, newPivotIndex + 1, to);
    }
}

std::string FileChooser::getDirectory() {
    return this->directory;
}

void FileChooser::setDirectory(std::string directory) {
    this->directory = directory;
}

/*
 * Prompts the user for a file to load.
 * Returns a pointer to a newly-allocated string. The caller is responsible
 * for free()ing it.
 */
char* FileChooser::startFileChooser(const char* extensions[], bool romExtensions, bool canQuit) {
    filesPerPage = systemGetConsoleHeight();
    filesPerPage--;
    if(canQuit) {
        filesPerPage--;
    }

    int numExtensions = sizeof(extensions) / sizeof(const char*);
    char* retval;
    char buffer[256];
    struct dirent* entry;

    while(true) {
        numFiles = 0;
        std::vector<string> filenames;
        std::vector<int> flags;
        std::vector<string> unmatchedStates;
        if(directory.compare("/") != 0) {
            filenames.push_back(string(".."));
            flags.push_back(FLAG_DIRECTORY);
            numFiles++;
        }

        DIR* dir = opendir(directory.c_str());
        if(dir != NULL) {
            // Read file list
            while((entry = readdir(dir)) != NULL) {
                char* ext = strrchr(entry->d_name, '.') + 1;
                if(strrchr(entry->d_name, '.') == 0) {
                    ext = 0;
                }

                bool isValidExtension = false;
                bool isRomFile = false;
                if(!(entry->d_type & DT_DIR)) {
                    if(ext) {
                        for(int i = 0; i < numExtensions; i++) {
                            if(strcasecmp(ext, extensions[i]) == 0) {
                                isValidExtension = true;
                                break;
                            }
                        }

                        if(romExtensions) {
                            isRomFile = strcasecmp(ext, "cgb") == 0 || strcasecmp(ext, "gbc") == 0 || strcasecmp(ext, "gb") == 0 || strcasecmp(ext, "sgb") == 0;
                            if(isRomFile) {
                                isValidExtension = true;
                            }
                        }
                    }
                }

                if(entry->d_type & DT_DIR || isValidExtension) {
                    if(!(strcmp(".", entry->d_name) == 0)) {
                        int flag = 0;
                        if(entry->d_type & DT_DIR) {
                            flag |= FLAG_DIRECTORY;
                        }

                        if(isRomFile) {
                            flag |= FLAG_ROM;
                        }

                        // Check for suspend state
                        if(isRomFile) {
                            if(!unmatchedStates.empty()) {
                                strcpy(buffer, entry->d_name);
                                *(strrchr(buffer, '.')) = '\0';
                                for(uint i = 0; i < unmatchedStates.size(); i++) {
                                    if(strcmp(buffer, unmatchedStates[i].c_str()) == 0) {
                                        flag |= FLAG_SUSPENDED;
                                        unmatchedStates.erase(unmatchedStates.begin() + i);
                                        break;
                                    }
                                }
                            }
                        }

                        flags.push_back(flag);
                        filenames.push_back(string(entry->d_name));
                        numFiles++;
                    }
                } else if(ext && strcasecmp(ext, "yss") == 0 && !(entry->d_type & DT_DIR)) {
                    bool matched = false;
                    char buffer2[256];
                    strcpy(buffer2, entry->d_name);
                    *(strrchr(buffer2, '.')) = '\0';
                    for(int i = 0; i < numFiles; i++) {
                        if(flags[i] & FLAG_ROM) {
                            strcpy(buffer, filenames[i].c_str());
                            *(strrchr(buffer, '.')) = '\0';
                            if(strcmp(buffer, buffer2) == 0) {
                                flags[i] |= FLAG_SUSPENDED;
                                matched = true;
                                break;
                            }
                        }
                    }

                    if(!matched) {
                        unmatchedStates.push_back(string(buffer2));
                    }
                }
            }

            closedir(dir);
        }

        quickSort(filenames, flags, nameSortFunction, 0, numFiles - 1);

        if(selection >= numFiles)
            selection = 0;

        if(!matchFile.empty()) {
            for(int i = 0; i < numFiles; i++) {
                if(matchFile == filenames[i]) {
                    selection = i;
                    break;
                }
            }

            matchFile = "";
        }

        // Done reading files

        scrollY = 0;
        updateScrollDown();
        bool readDirectory = false;
        while(!readDirectory) {
            int screenLen = systemGetConsoleWidth();
            // Draw the screen
            iprintf("\x1b[2J");
            strncpy(buffer, directory.c_str(), screenLen);
            buffer[screenLen] = '\0';
            printf("%s", buffer);
            for(uint j = 0; j < screenLen - strlen(buffer); j++) {
                printf(" ");
            }

            for(int i = scrollY; i < scrollY + filesPerPage && i < numFiles; i++) {
                if(i == selection) {
                    printf("\x1b[47m\x1b[30m* ");
                } else if(i == scrollY && i != 0) {
                    printf("^ ");
                } else if(i == scrollY + filesPerPage - 1 && scrollY + filesPerPage - 1 != numFiles - 1) {
                    printf("v ");
                } else {
                    printf("  ");
                }

                int stringLen = screenLen - 2;
                if(flags[i] & FLAG_DIRECTORY) {
                    stringLen--;
                }

                strncpy(buffer, filenames[i].c_str(), stringLen);
                buffer[stringLen] = '\0';
                if(flags[i] & FLAG_DIRECTORY) {
                    if(i == selection) {
                        printf("\x1b[2m");
                    } else {
                        printf("\x1b[1m");
                    }

                    printf("\x1b[33m%s/", buffer);
                } else if(flags[i] & FLAG_SUSPENDED) {
                    if(i == selection) {
                        printf("\x1b[2m");
                    } else {
                        printf("\x1b[1m");
                    }

                    printf("\x1b[35m%s", buffer);
                } else {
                    printf("%s", buffer);
                }

                for(uint j = 0; j < stringLen - strlen(buffer); j++) {
                    printf(" ");
                }

                printf("\x1b[0m");
            }

            if(canQuit) {
                if(numFiles < filesPerPage) {
                    for(int i = numFiles; i < filesPerPage; i++) {
                        printf("\n");
                    }
                }

                const char* text = "Press Y to exit";
                int spaces = systemGetConsoleWidth() - strlen(text);
                for(int i = 0; i < spaces; i++) {
                    printf(" ");
                }

                printf(text);
            }

            gfxFlush();
            gfxWaitForVBlank();

            // Wait for input
            while(true) {
                systemCheckRunning();
                gfxWaitForVBlank();
                inputUpdate();

                if(inputKeyPressed(inputMapMenuKey(MENU_KEY_A))) {
                    if(flags[selection] & FLAG_DIRECTORY) {
                        if(strcmp(filenames[selection].c_str(), "..") == 0) {
                            goto lowerDirectory;
                        }

                        directory += filenames[selection] + "/";
                        readDirectory = true;
                        selection = 1;
                        break;
                    } else {
                        // Copy the result to a new allocation, as the
                        // filename would become unavailable when freed.
                        retval = (char*) malloc(sizeof(char) * (directory.length() + strlen(filenames[selection].c_str()) + 1));
                        strcpy(retval, directory.c_str());
                        strcpy(retval + (directory.length() * sizeof(char)), filenames[selection].c_str());
                        goto end;
                    }
                } else if(inputKeyPressed(inputMapMenuKey(MENU_KEY_B))) {
                    lowerDirectory:
                    // Select this directory when going up
                    std::string currDir = directory;
                    std::string::size_type slash = currDir.find_last_of('/');
                    if(currDir.length() != 1 && slash == currDir.length() - 1) {
                        currDir = currDir.substr(0, slash);
                        slash = currDir.find_last_of('/');
                    }

                    matchFile = currDir.substr(0, slash);

                    directory = matchFile + "/";
                    readDirectory = true;
                    selection = 1;
                    break;
                } else if(inputKeyRepeat(inputMapMenuKey(MENU_KEY_UP))) {
                    if(selection > 0) {
                        selection--;
                        updateScrollUp();
                        break;
                    }
                } else if(inputKeyRepeat(inputMapMenuKey(MENU_KEY_DOWN))) {
                    if(selection < numFiles - 1) {
                        selection++;
                        updateScrollDown();
                        break;
                    }
                } else if(inputKeyRepeat(inputMapMenuKey(MENU_KEY_RIGHT))) {
                    selection += filesPerPage / 2;
                    updateScrollDown();
                    break;
                } else if(inputKeyRepeat(inputMapMenuKey(MENU_KEY_LEFT))) {
                    selection -= filesPerPage / 2;
                    updateScrollUp();
                    break;
                } else if(inputKeyPressed(inputMapMenuKey(MENU_KEY_Y))) {
                    if(canQuit) {
                        retval = NULL;
                        goto end;
                    }
                }
            }
        }
    }

    end:
    iprintf("\x1b[2J");
    return retval;
}
