#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server)
		: search_server_(search_server)
		{}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
	vector<Document> found_documents = search_server_.FindTopDocuments(raw_query, status);
	requests_.push_back({raw_query, found_documents});
	if(requests_.size() > sec_in_day_) {
		requests_.pop_front();
	}
	return found_documents;
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
	// напишите реализацию
	vector<Document> found_documents = search_server_.FindTopDocuments(raw_query);
	requests_.push_back({raw_query, found_documents});
	if(requests_.size() > sec_in_day_) {
		requests_.pop_front();
	}
	return found_documents;
}

int RequestQueue::GetNoResultRequests() const {
	int count = 0;
	for(const auto & request : requests_) {
		if(request.found_documents.empty()) {
			++count;
		}
	}
	return count;
}