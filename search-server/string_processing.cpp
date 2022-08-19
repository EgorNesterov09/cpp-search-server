#include "string_processing.h"
using namespace std;
    
vector<string_view> SplitIntoWords(string_view str) {
    vector<string_view> result;
    // 1
    int64_t pos = str.find_first_not_of(" ");
    // 2
    const int64_t pos_end = str.npos;
    // 3
    while (pos != pos_end) {
        // 4
        int64_t space = str.find(' ', pos);
        // 5
        result.push_back(space == pos_end ? str.substr(pos) : str.substr(pos, space - pos));
        // 6
        pos = str.find_first_not_of(" ", space);
    }

    return result;
} 
