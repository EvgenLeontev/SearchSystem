#pragma once

#include "document.h"
#include "string_processing.h"
#include <string>
#include <vector>
#include <set>
#include <map>
#include <cmath>
#include <algorithm>
#include <tuple>
#include <execution>
#include <mutex>

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

namespace std::execution {
	class parallel_policy;
	class sequenced_policy;
}

class SearchServer {
public:
	template <typename StringContainer>
	explicit SearchServer(const StringContainer& stop_words)
			: stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
	{
		if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
			throw std::invalid_argument("Some of stop words are invalid");
		}
	}
	
	explicit SearchServer(const std::string& stop_words_text);
	explicit SearchServer(std::string_view stop_words_text);

	
	void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);
	
	
	template <typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(const std::execution::parallel_policy& ,std::string_view raw_query, const DocumentPredicate& document_predicate) const {
		const Query query = ParseQuery(raw_query);

		std::vector<Document> matched_documents = FindAllDocuments(std::execution::par, query, document_predicate);

		sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
			if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
				return lhs.rating > rhs.rating;
			}
			else {
				return lhs.relevance > rhs.relevance;
			}
			});
		if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
			matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
		}
		return matched_documents;
	}
	
	template <typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, const DocumentPredicate& document_predicate) const {
		const Query query = ParseQuery(raw_query);

		std::vector<Document> matched_documents = FindAllDocuments(std::execution::seq, query, document_predicate);

		sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
			if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
				return lhs.rating > rhs.rating;
			}
			else {
				return lhs.relevance > rhs.relevance;
			}
			});
		if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
			matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
		}

		return matched_documents;
	}
	
	template <typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(std::string_view raw_query, const DocumentPredicate& document_predicate) const {
		return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
	}

	
	std::vector<Document> FindTopDocuments(const std::execution::parallel_policy& ,std::string_view raw_query, DocumentStatus status) const;
	std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentStatus status) const;
	std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

	
	std::vector<Document> FindTopDocuments(std::execution::parallel_policy& ,std::string_view raw_query) const;
	std::vector<Document> FindTopDocuments(std::execution::sequenced_policy&, std::string_view raw_query) const;
	std::vector<Document> FindTopDocuments(std::string_view raw_query) const;
	
	
	int GetDocumentCount() const;

public:
	std::vector<int>::const_iterator begin() const;
	std::vector<int>::const_iterator end() const;

	
	const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

	
	void RemoveDocument(int document_id);
	void RemoveDocument(std::execution::parallel_policy&, int document_id);
	void RemoveDocument(std::execution::sequenced_policy&, int document_id);

	
	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;
	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy, std::string_view raw_query, int document_id) const;
	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, std::string_view raw_query, int document_id) const;

private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
		std::map<std::string_view, double> word_to_freq;
		std::set<std::string> words;
		std::string text;
	};
	const std::set<std::string_view> stop_words_;
	std::map<std::string, std::map<int, double>> word_to_document_freqs_; //слово - id - частота
	std::map<int, DocumentData> documents_;
	std::vector<int> document_ids_;

	
	bool IsStopWord(std::string_view word) const;

	
	static bool IsValidWord(std::string_view word);

	
	std::vector<std::string> SplitIntoWordsNoStop(std::string_view text) const;

	
	static int ComputeAverageRating(const std::vector<int>& ratings);

	
	struct QueryWord {
		std::string_view data;
		bool is_minus;
		bool is_stop;
	};

	
	QueryWord ParseQueryWord(std::string_view text) const;

	
	struct Query {
		std::set<std::string_view> plus_words;
		std::set<std::string_view> minus_words;
	};

	
	Query ParseQuery(std::string_view text) const;

	
	// Existence required
	double ComputeWordInverseDocumentFreq(const std::string& word) const;

	
	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(std::execution::parallel_policy, const Query& query, const DocumentPredicate& document_predicate) const {
		std::map<int, double> document_to_relevance;
		for (const std::string_view word : query.plus_words) {
			if (word_to_document_freqs_.count(std::string(word)) == 0) {
				continue;
			}
			const double inverse_document_freq = ComputeWordInverseDocumentFreq(std::string(word));

			for_each(std::execution::par, word_to_document_freqs_.at(std::string(word)).begin(),
					 word_to_document_freqs_.at(std::string(word)).end(), [&](const auto to_document_freqs) {
				int document_id = to_document_freqs.first;
				double term_freq = to_document_freqs.second;
				const auto& document_data = documents_.at(document_id);
				if (document_predicate(document_id, document_data.status, document_data.rating)) {
					if(document_to_relevance.count(document_id) == 0) {
						document_to_relevance[document_id] = 0.0;
					}
					std::mutex mtx;
					std::lock_guard<std::mutex> guard(mtx);
					document_to_relevance[document_id] += term_freq * inverse_document_freq;
				}
			} );
		}
		
		for (const std::string_view word : query.minus_words) {
			if (word_to_document_freqs_.count(std::string(word)) != 0) {

				for (const auto [document_id, _] : word_to_document_freqs_.at(std::string(word))) {
					document_to_relevance.erase(document_id);
				}
			}
		}
		std::vector<Document> matched_documents;
		for (const auto [document_id, relevance] : document_to_relevance) {
			matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
		}
		return matched_documents;
	}

	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(std::execution::sequenced_policy, const Query& query, const DocumentPredicate& document_predicate) const {
		std::map<int, double> document_to_relevance;
		for (const std::string_view word : query.plus_words) {
			if (word_to_document_freqs_.count(std::string(word)) == 0) {
				continue;
			}
			const double inverse_document_freq = ComputeWordInverseDocumentFreq(std::string(word));
			for (const auto [document_id, term_freq] : word_to_document_freqs_.at(std::string(word))) {
				const auto& document_data = documents_.at(document_id);
				if (document_predicate(document_id, document_data.status, document_data.rating)) {
					if (document_to_relevance.count(document_id) == 0) {
						document_to_relevance[document_id] = 0.0;
					}
					document_to_relevance[document_id] += term_freq * inverse_document_freq;
				}
			}
		}
		for (const std::string_view word : query.minus_words) {
			if (word_to_document_freqs_.count(std::string(word)) == 0) {
				continue;
			}
			for (const auto [document_id, _] : word_to_document_freqs_.at(std::string(word))) {
				document_to_relevance.erase(document_id);
			}
		}
		std::vector<Document> matched_documents;
		for (const auto [document_id, relevance] : document_to_relevance) {
			matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
		}
		return matched_documents;
	}
};

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
                 const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string& query);
