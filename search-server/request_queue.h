#pragma once
#include "search_server.h"
#include <vector>
#include <string>
#include <deque>

class RequestQueue {
public:
    RequestQueue () = default;
    explicit RequestQueue(const SearchServer& search_server);
    
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;
private:
    struct QueryResult {
        std::vector<Document> search_results_;
    };
    int time_ = 0;
    int count_empty_request = 0;
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    SearchServer search_server_;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        RequestQueue::time_++;
        if (time_ > min_in_day_) {
            RequestQueue::requests_.pop_front();
        }
        
        RequestQueue::QueryResult a;
        a.search_results_ = RequestQueue::search_server_.FindTopDocuments(raw_query, document_predicate);
        RequestQueue::requests_.push_back(a);
        
        if (a.search_results_.empty() && time_ < min_in_day_) {
            RequestQueue::count_empty_request++;
        }
        if (time_ > min_in_day_  ) {
                RequestQueue::count_empty_request--;
            }   
        return RequestQueue::requests_.back().search_results_;
    }