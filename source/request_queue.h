#pragma once
#include "document.h"
#include "search_server.h"
#include <deque>

class RequestQueue {
public:
	explicit RequestQueue(const SearchServer& search_server);
	
	// "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
	template <typename DocumentPredicate>
	std::vector<Document> AddFindRequest(const std::string& raw_query, const DocumentPredicate& document_predicate) {
		std::vector<Document> found_documents = search_server_.FindTopDocuments(raw_query, document_predicate);
		requests_.push_back({raw_query, found_documents});
		if(requests_.size() > sec_in_day_) {
			requests_.pop_front();
		}
		return found_documents;
	}
	
	std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
	
	std::vector<Document> AddFindRequest(const std::string& raw_query);
	
	int GetNoResultRequests() const;

private:
	struct QueryResult {
		std::string raw_query;
		std::vector<Document> found_documents;
	};
	
	std::deque<QueryResult> requests_;
	const static int sec_in_day_ = 1440;
	const SearchServer& search_server_;
};
