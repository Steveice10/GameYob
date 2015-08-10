#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>
#include <platform/ui.h>

#include "platform/common/filechooser.h"
#include "platform/gfx.h"
#include "platform/input.h"
#include "platform/system.h"

#define FLAG_DIRECTORY  1
#define FLAG_SUSPENDED  2
#define FLAG_ROM        4

static bool fileChooserActive = false;

bool isFileChooserActive() {
    return fileChooserActive;
}

FileChooser::FileChooser(std::string directory, std::vector<std::string> extensions, bool canQuit) {
    this->directory = directory;
    this->extensions = extensions;
    this->canQuit = canQuit;
}

int nameSortFunction(std::string &a, std::string &b) {
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

void FileChooser::navigateBack() {
    std::string currDir = directory;
    std::string::size_type slash = currDir.find_last_of('/');
    if(currDir.length() != 1 && slash == currDir.length() - 1) {
        currDir = currDir.substr(0, slash);
        slash = currDir.find_last_of('/');
    }

    matchFile = currDir.substr(0, slash);

    directory = matchFile + "/";
    selection = 1;
}

void FileChooser::refreshContents() {
    filenames.clear();
    flags.clear();

    std::vector<std::string> unmatchedStates;
    numFiles = 0;
    if(directory.compare("/") != 0) {
        filenames.push_back(std::string(".."));
        flags.push_back(FLAG_DIRECTORY);
        numFiles++;
    }

    DIR* dir = opendir(directory.c_str());
    if(dir != NULL) {
        // Read file list
        dirent* entry;
        while((entry = readdir(dir)) != NULL) {
            if(strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char* ext = strrchr(entry->d_name, '.') + 1;
            if(strrchr(entry->d_name, '.') == 0) {
                ext = 0;
            }

            bool isValidExtension = false;
            bool isRomFile = false;
            if(!(entry->d_type & DT_DIR)) {
                if(ext) {
                    for(u32 i = 0; i < extensions.size(); i++) {
                        if(strcasecmp(ext, extensions[i].c_str()) == 0) {
                            isValidExtension = true;
                            isRomFile = strcasecmp(ext, "cgb") == 0 || strcasecmp(ext, "gbc") == 0 || strcasecmp(ext, "gb") == 0 || strcasecmp(ext, "sgb") == 0;
                            break;
                        }
                    }
                }
            }

            if(entry->d_type & DT_DIR || isValidExtension) {
                if(strcmp(".", entry->d_name) != 0) {
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
                            std::string name = entry->d_name;
                            std::string::size_type dot = name.find('.');
                            if(dot != std::string::npos) {
                                name = name.substr(0, dot);
                            }

                            for(uint i = 0; i < unmatchedStates.size(); i++) {
                                if(name.compare(unmatchedStates[i]) == 0) {
                                    flag |= FLAG_SUSPENDED;
                                    unmatchedStates.erase(unmatchedStates.begin() + i);
                                    break;
                                }
                            }
                        }
                    }

                    flags.push_back(flag);
                    filenames.push_back(std::string(entry->d_name));
                    numFiles++;
                }
            } else if(ext && strcasecmp(ext, "yss") == 0 && !(entry->d_type & DT_DIR)) {
                bool matched = false;

                std::string dirName = entry->d_name;
                std::string::size_type dirDot = dirName.find('.');
                if(dirDot != std::string::npos) {
                    dirName = dirName.substr(0, dirDot);
                }

                for(int i = 0; i < numFiles; i++) {
                    if(flags[i] & FLAG_ROM) {
                        std::string fileName = filenames[i];
                        std::string::size_type fileDot = fileName.find('.');
                        if(fileDot != std::string::npos) {
                            fileName = fileName.substr(0, fileDot);
                        }

                        if(fileName.compare(dirName) == 0) {
                            flags[i] |= FLAG_SUSPENDED;
                            matched = true;
                            break;
                        }
                    }
                }

                if(!matched) {
                    unmatchedStates.push_back(dirName);
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

    scrollY = 0;
    updateScrollDown();
}

void FileChooser::redrawChooser() {
    int screenLen = uiGetWidth();

    uiWaitForVBlank();
    uiClear();

    std::string currDirName = directory;
    if(currDirName.length() > (u32) screenLen) {
        currDirName = currDirName.substr(0, (std::string::size_type) screenLen);
    }

    uiPrint("%s", currDirName.c_str());

    for(uint j = 0; j < screenLen - currDirName.length(); j++) {
        uiPrint(" ");
    }

    for(int i = scrollY; i < scrollY + filesPerPage && i < numFiles; i++) {
        if(i == selection) {
            uiSetLineHighlighted(true);
            uiPrint("* ");
        } else if(i == scrollY && i != 0) {
            uiPrint("^ ");
        } else if(i == scrollY + filesPerPage - 1 && scrollY + filesPerPage - 1 != numFiles - 1) {
            uiPrint("v ");
        } else {
            uiPrint("  ");
        }

        int displayLen = screenLen - 2;
        if(flags[i] & FLAG_DIRECTORY) {
            displayLen--;
        }

        std::string fileName = filenames[i];
        if(fileName.length() > (u32) displayLen) {
            fileName = fileName.substr(0, (std::string::size_type) displayLen);
        }

        if(flags[i] & FLAG_DIRECTORY) {
            uiSetTextColor(TEXT_COLOR_YELLOW);
        } else if(flags[i] & FLAG_SUSPENDED) {
            uiSetTextColor(TEXT_COLOR_PURPLE);
        }

        uiPrint("%s", fileName.c_str());
        if(flags[i] & FLAG_DIRECTORY) {
            uiPrint("/");
        }

        for(uint j = 0; j < displayLen - fileName.length(); j++) {
            uiPrint(" ");
        }

        uiSetTextColor(TEXT_COLOR_NONE);
        uiSetLineHighlighted(false);
    }

    uiFlush();
}

bool FileChooser::updateChooser(char** result) {
    bool readDirectory = false;
    bool redraw = false;

    UIKey key;
    while((key = uiReadKey()) != UI_KEY_NONE) {
        if(key == UI_KEY_A) {
            if(flags[selection] & FLAG_DIRECTORY) {
                if(strcmp(filenames[selection].c_str(), "..") == 0) {
                    navigateBack();
                } else {
                    directory += filenames[selection] + "/";
                    selection = 1;
                }

                readDirectory = true;
                redraw = true;
            } else {
                // Copy the result to a new allocation, as the
                // filename would become unavailable when freed.
                *result = (char*) malloc(sizeof(char) * (directory.length() + strlen(filenames[selection].c_str()) + 1));
                strcpy(*result, directory.c_str());
                strcpy(*result + (directory.length() * sizeof(char)), filenames[selection].c_str());
                return true;
            }
        } else if(key == UI_KEY_B) {
            if(canQuit && directory.compare("/") == 0) {
                *result = NULL;
                return true;
            }

            navigateBack();
            readDirectory = true;
            redraw = true;
        } else if(key == UI_KEY_UP) {
            if(selection > 0) {
                selection--;
                updateScrollUp();
                redraw = true;
            }
        } else if(key == UI_KEY_DOWN) {
            if(selection < numFiles - 1) {
                selection++;
                updateScrollDown();
                redraw = true;
            }
        } else if(key == UI_KEY_RIGHT) {
            selection += filesPerPage / 2;
            updateScrollDown();
            redraw = true;
        } else if(key == UI_KEY_LEFT) {
            selection -= filesPerPage / 2;
            updateScrollUp();
            redraw = true;
        }
    }

    if(readDirectory) {
        refreshContents();
    }

    if(redraw) {
        redrawChooser();
    }

    return false;
}

/*
 * Prompts the user for a file to load.
 * Returns a pointer to a newly-allocated string. The caller is responsible
 * for free()ing it.
 */
char* FileChooser::startFileChooser() {
    filesPerPage = uiGetHeight() - 1;

    refreshContents();
    redrawChooser();

    fileChooserActive = true;
    while(true) {
        systemCheckRunning();
        uiWaitForVBlank();
        inputUpdate();

        char* result;
        if(updateChooser(&result)) {
            uiClear();
            uiFlush();

            fileChooserActive = false;
            return result;
        }
    }

    fileChooserActive = false;
    return NULL;
}
