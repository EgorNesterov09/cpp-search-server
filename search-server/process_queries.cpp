#include "process_queries.h"
#include "search_server.h"
#include <algorithm>
#include <execution>
using namespace std;

vector<vector<Document>> ProcessQueries(const SearchServer& search_server,
    const vector<string>& queries) {
    vector<vector<Document>> result (queries.size());
    transform(execution::par, queries.begin(), queries.end(), result.begin(), [&](const string& query) { return search_server.FindTopDocuments(query); });
    return result;
}

vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const vector<string>& queries){
    vector<vector<Document>> documents = ProcessQueries (search_server, queries);
    vector<Document> result;
    for (auto i: documents) {
        for (auto a: i) {
            result.insert(result.end(), a);
        }
    }
    return result;     
}