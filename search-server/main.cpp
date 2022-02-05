#include "search_server.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

/* Подставьте вашу реализацию класса SearchServer сюда */

/* 
   Подставьте сюда вашу реализацию макросов 
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
*/

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);                                                                                                 
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

/*
Разместите код остальных тестов здесь
*/
void TestTfIdf() {
 
    SearchServer server;
    const vector<int> doc_id = { 10, 11,12,13 };
    vector < string> docus =
    {
        "I like cats",
        "cats is nice animals",
        "Where are you from",
        "Who it can be now",
    };
    vector <vector<string>> documents =
    {
        {"I", "like", "cats"},
        {"cats", "is", "nice", "animals"},
        {"Where", "are", "you", "from"},
        {"Who", "it", "can", "be", "now"}
    };
    const vector<int> ratings = { 1, 2, 3 };
 
 
    for (int i = 0; i < docus.size(); i++) 
    {
      server.AddDocument(doc_id[i], docus[i], DocumentStatus::ACTUAL, ratings);
    }
    ASSERT_EQUAL(server.GetDocumentCount(), 4);
 
   
    vector<double> tf;
    int document_freq = 0;
    vector<double> ved;
    string term = "cats";
   
    for (const auto& document : documents)
    {
        int word_count = count(begin(document), end(document), term);
        if (word_count > 0)
        {
            tf.push_back(static_cast<double>(word_count) / static_cast<double>(document.size()));
            document_freq += count(begin(document), end(document), term);
        }
    }
    
    for (int i = 0; i < tf.size(); i++) {
        ved.push_back(tf[i] * log(static_cast<double>(documents.size()) / static_cast<double>(document_freq)));
    }
    vector <Document> nv = server.FindTopDocuments(term);
    sort(begin(ved), end(ved), [](double& d1, double& d2) {return d1 > d2; });
    for (int i = 0; i < ved.size(); i++)
    {
        ASSERT_EQUAL(ved[i], nv[i].relevance);
    }
 
    
   
}
 
void TestForUsingPredicate()
{
    SearchServer server;
    const DocumentStatus status = DocumentStatus::ACTUAL;
    const vector<int> rating = { 1,2,3,4,5 };
    double sum_ratings = 0.0;
    for (auto& n : rating) {
        sum_ratings += n;
    }
    
    const int avg_rating = sum_ratings / rating.size();
 
    vector < string> docus =
    {
        "I like cats",
        "cats is nice animals",
        "Where are you from",
        "Who it can be now",
    };
 
    vector <vector<string>> documents =
    {
        {"I", "like", "cats"},
        {"cats", "is", "nice", "animals"},
        {"Where", "are", "you", "from"},
        {"Who", "it", "can", "be", "now"}
    };
 
    
    for (size_t i = 0; i < docus.size(); ++i) 
    {
       server.AddDocument(i, docus[i], status, rating);
    }
 
    for (int i = 0; i < documents.size(); ++i) {
        
        for (const auto& s : documents[i]) {
            
            auto predicate = [i, status, avg_rating](int id, DocumentStatus st, int rating) {
                return id == i && st == status && avg_rating == rating;
            };
            auto search_results = server.FindTopDocuments(s, predicate);
            ASSERT_EQUAL(search_results.size(), 1);           
        }
    }
}
 
void TestForAddDocument() {
    SearchServer server;
    const int doc_id = { 10 };
     string docus = "I like cats";
    vector <int> rating = { 1,2,3,4 };
    server.AddDocument(doc_id, docus, DocumentStatus::ACTUAL, rating);
    ASSERT_EQUAL(server.GetDocumentCount(), 1);
    vector <Document> v = server.FindTopDocuments("cats");
    ASSERT(v.size() == 1);
 
}
void TestForMatchDoc() {
    SearchServer server;
    const int doc_id = { 10 };
    vector <int> rating = { 1,2,3,4 };
    DocumentStatus status = DocumentStatus::ACTUAL;
    
    
 
    {
        SearchServer server;
        server.AddDocument(doc_id, "cat in black box", status, rating);
        ASSERT(server.GetDocumentCount() == 1);
        vector<string> v;
        tie(v, status) = server.MatchDocument("cat", doc_id);
        ASSERT(!v.empty());
    }
 
   {
        SearchServer server;
        
        server.AddDocument(doc_id, "-cat in black box", status, rating);
        ASSERT(server.GetDocumentCount() == 1);
        vector<string> v;
        tie(v, status) = server.MatchDocument("сat", doc_id);
        ASSERT(v.empty());
    }
    
 
}
 
void TestForRating() {
    SearchServer server;
    const DocumentStatus status = DocumentStatus::ACTUAL;
    const vector<int> rating = { 1,2,3,4 };
        double sum_ratings = 0.0;
    for (auto& n : rating) {
        sum_ratings += n;
    }
    const int avg_rating = sum_ratings / rating.size();
    server.AddDocument(0, "cat in the black box", status, rating);
    ASSERT(server.GetDocumentCount() == 1);
 
    vector<Document> v = server.FindTopDocuments("cat");
 
    ASSERT_EQUAL(avg_rating, v[0].rating);
 
}
 
 
 
void TestForExludeMinusWordsFromSearch() {
    SearchServer server;
    const string s = "cat in the black box";
    server.AddDocument(0, s, DocumentStatus::ACTUAL, { 1,2,3 });
 
    string query = "cat";
    vector<Document> v = server.FindTopDocuments(query);
    ASSERT(v.size() == 1);
    query = "-cat";
    v = server.FindTopDocuments(query);
    ASSERT(v.empty());
}
 
void TestForDocumentStatus() {
    SearchServer server;
    const string s = "cat in the black box";
    DocumentStatus status = DocumentStatus::ACTUAL;
    server.AddDocument(0, s, status, { 1,2,3 });
 
    
    auto v = server.FindTopDocuments("cat", status);
    ASSERT(v.size() == 1);
    
 
}
 

void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestForAddDocument);
    RUN_TEST(TestForMatchDoc);
    RUN_TEST(TestForExludeMinusWordsFromSearch);
    RUN_TEST(TestTfIdf);
    RUN_TEST(TestForUsingPredicate);
    RUN_TEST(TestForDocumentStatus);
    RUN_TEST(TestForRating);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}