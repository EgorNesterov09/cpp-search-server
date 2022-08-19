#include "search_server.h"
#include "log_duration.h"
#include <cassert>

using namespace std;

SearchServer::SearchServer(string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {
}

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(std::string_view(stop_words_text)) {}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const auto& word : words) {
        word_to_document_freqs_[std::string(word)][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][std::string(word)] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy&, int document_id) {
    vector<string> vec_words;
    for (auto& [key, value] : SearchServer::word_to_document_freqs_) {
        value.erase(document_id);
        if (value.empty()) {
            vec_words.push_back(key);
        }
    }

    for (auto& word : vec_words) {
        word_to_document_freqs_.erase(word);
    }

    documents_.erase(document_id);
    document_ids_.erase(document_id);
    
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy&, int document_id) {
    // remove from word_to_document_freqs_
    auto& m = document_to_word_freqs_.at(document_id);
    std::vector<std::string*> v(m.size());
    transform(execution::par, m.begin(), m.end(), v.begin(),
             [](auto& p) { return const_cast<std::string*>(&(p.first)); } );
    for_each(execution::par, v.begin(), v.end(),
             [this, document_id](auto& s)
             { word_to_document_freqs_[*s].erase(document_id); }
            );
    
    document_ids_.erase(document_id);
    documents_.erase(document_id);
    document_to_word_freqs_.erase(document_id);
}    

void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(std::execution::seq, document_id);
}

vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::par,
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, string_view raw_query, DocumentStatus status) const {
    return SearchServer::FindTopDocuments(raw_query, status);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, string_view raw_query) const {
    return FindTopDocuments(execution::par, raw_query, DocumentStatus::ACTUAL);
}

vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, string_view raw_query) const {
    return SearchServer::FindTopDocuments(raw_query);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

SearchServer::Query SearchServer::ParseQuerySeq(const string_view text) const {
    SearchServer::Query result;
    for (const string_view word : SplitIntoWords(text)) {
        const auto query_word = SearchServer::ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            } else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
        
    sort(result.minus_words.begin(), result.minus_words.end());
    auto minus_words = unique(result.minus_words.begin(), result.minus_words.end());
    result.minus_words.erase(minus_words, result.minus_words.end());
        
    sort(result.plus_words.begin(), result.plus_words.end());
    auto plus_word = unique(result.plus_words.begin(), result.plus_words.end());
    result.plus_words.erase(plus_word, result.plus_words.end());
    return result;
}
    
SearchServer::Query SearchServer::ParseQueryPar(const string_view text) const {
    SearchServer::Query result;
    for (const string_view word : SplitIntoWords(text)) {
        const auto query_word = SearchServer::ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            } else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    return result;
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view raw_query, int document_id) const {
    return SearchServer::MatchDocument(execution::seq, raw_query, document_id);
}
    
tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::sequenced_policy&, const string_view raw_query, int document_id) const {
    SearchServer::Query query = SearchServer::ParseQuerySeq(raw_query);
    vector<string_view> matched_words;
    for (string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(string(word)).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(string(word)).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return {matched_words, documents_.at(document_id).status};
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::parallel_policy&, const string_view raw_query, int document_id) const {
    assert(documents_.count(document_id) > 0);

    const auto& word_freqs = document_to_word_freqs_.at(document_id);
    auto query = ParseQueryPar(raw_query);

    if (any_of(execution::par, query.minus_words.begin(),
                    query.minus_words.end(),
                    [&word_freqs](const std::string_view word) {
                        return word_freqs.count(word) > 0;
                    })) {
        return { {}, documents_.at(document_id).status };
    }
    
    vector<string_view> matched_words;
    matched_words.reserve(query.plus_words.size());
    copy_if(execution::par, query.plus_words.begin(),
                 query.plus_words.end(), back_inserter(matched_words),
                 [&word_freqs](const string_view word) {
                     return word_freqs.count(word) > 0;
                 });
    
    sort(execution::par, matched_words.begin(),matched_words.end());
    auto it_last = unique(execution::par, matched_words.begin(), matched_words.end());
    matched_words.erase(it_last, matched_words.end());
    return { matched_words, documents_.at(document_id).status };  
    }

double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(std::string(word)).size());
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static map<string_view, double> result;
    if (!document_to_word_freqs_.count(document_id)) {
        return result;
    }
    for (const auto& [k, v] : document_to_word_freqs_.at(document_id)) {
        result.insert(pair{string_view(k), v});
    }
    return result;
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.cbegin();
}

set<int>::const_iterator SearchServer::end() const {
    return document_ids_.cend();
}

bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(string(word)) > 0;
}

bool SearchServer::IsValidWord(string_view word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words;
    for (const auto& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + string(word) + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word "s + string(text) + " is invalid");
    }
    return {word, is_minus, IsStopWord(word)};
}

void AddDocument(SearchServer& search_server, int document_id, string_view document,
                 DocumentStatus status, const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const exception& e) {
        cout << "Error in adding document "s << document_id << ": "s << e.what() << endl;
    }
}
