#include "search_server.h"
#include "string_processing.h"
#include <numeric>

#include "log_duration.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
		: SearchServer(string_view(stop_words_text))  // Invoke delegating constructor from string container
		{}

SearchServer::SearchServer(string_view stop_words_text)
	: SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string_view 
{}


void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings) {
	if ((document_id < 0) || (documents_.count(document_id) > 0)) {
		throw invalid_argument("Invalid document_id"s);
	}

	documents_[document_id].rating = ComputeAverageRating(ratings);
	documents_[document_id].status = status;
	documents_[document_id].text = string(document);

	const auto words = SplitIntoWordsNoStop(documents_[document_id].text);

	map<string, double> word_to_freq;
	const double inv_word_count = 1.0 / words.size();
	for (const string& word : words) {
		if (word_to_document_freqs_[word].count(document_id) == 0) {
			word_to_document_freqs_[word][document_id] = 0.0;
			word_to_freq[word] = 0.0;
		}
		word_to_document_freqs_[word][document_id] += inv_word_count;
		word_to_freq[word] += inv_word_count;
		
	}
	document_ids_.push_back(document_id);
}


vector<Document> SearchServer::FindTopDocuments(const execution::parallel_policy&, string_view raw_query, DocumentStatus status) const {
	return FindTopDocuments(execution::par, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
		return document_status == status;
		});
}

vector<Document> SearchServer::FindTopDocuments(const execution::sequenced_policy&, string_view raw_query, DocumentStatus status) const {
	return FindTopDocuments(execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
		return document_status == status;
		});
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
	return FindTopDocuments(execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
		return document_status == status;
		});
}


vector<Document> SearchServer::FindTopDocuments(execution::parallel_policy&, string_view raw_query) const {
	return FindTopDocuments(execution::par, raw_query, DocumentStatus::ACTUAL);
}
vector<Document> SearchServer::FindTopDocuments(execution::sequenced_policy&, string_view raw_query) const {
	return FindTopDocuments(execution::seq, raw_query, DocumentStatus::ACTUAL);
}
vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
	return FindTopDocuments(execution::seq, raw_query, DocumentStatus::ACTUAL);
}



int SearchServer::GetDocumentCount() const {
	return documents_.size();
}


vector<int>::const_iterator SearchServer::begin() const {
	return document_ids_.begin();
}

vector<int>::const_iterator SearchServer::end() const {
	return document_ids_.end();
}


const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
	if (documents_.lower_bound(document_id) == documents_.end()) {
		const static map<string_view, double> empty_map;
		return empty_map;
	}
	return documents_.at(document_id).word_to_freq;
}


void SearchServer::RemoveDocument(int document_id) {
	if (documents_.lower_bound(document_id) != documents_.end()) {
		auto it = documents_.lower_bound(document_id)->second.word_to_freq;
		for(auto word : it) {
			word_to_document_freqs_[string(word.first)].erase(document_id);
			if(word_to_document_freqs_[string(word.first)].empty()) {
				word_to_document_freqs_.erase(string(word.first));
			}
		}
		documents_.erase(document_id);
		document_ids_.erase(lower_bound(document_ids_.begin(), document_ids_.end(), document_id));
	}
}

void SearchServer::RemoveDocument(std::execution::parallel_policy&, int document_id) {
	if (documents_.lower_bound(document_id) != documents_.end()) {
		auto it = documents_.lower_bound(document_id)->second.word_to_freq;
		for_each(/*std::execution::par, */it.begin(), it.end(), [&](const auto& word) {
																word_to_document_freqs_[string(word.first)].erase(document_id);
																if(word_to_document_freqs_[string(word.first)].empty()) {
																	word_to_document_freqs_.erase(string(word.first));
																} });
		documents_.erase(document_id);
		document_ids_.erase(lower_bound(document_ids_.begin(), document_ids_.end(), document_id));
	}
}

void SearchServer::RemoveDocument(std::execution::sequenced_policy&, int document_id) {
	if (documents_.lower_bound(document_id) != documents_.end()) {
		auto it = documents_.lower_bound(document_id)->second.word_to_freq;
		for_each(/*std::execution::seq,*/ it.begin(), it.end(), [&](const auto& word) {
																word_to_document_freqs_[string(word.first)].erase(document_id);
																if(word_to_document_freqs_[string(word.first)].empty()) {
																	word_to_document_freqs_.erase(string(word.first));
																} });
		documents_.erase(document_id);
		document_ids_.erase(lower_bound(document_ids_.begin(), document_ids_.end(), document_id));
	}
}


tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query, int document_id) const {
	const Query query = ParseQuery(raw_query);
	
	vector<string_view> matched_words;
	for (const string_view word : query.plus_words) {
		if (word_to_document_freqs_.count(string(word)) == 0) {
			continue;
		}
		if (word_to_document_freqs_.at(string(word)).count(document_id)) {
			matched_words.push_back(word);
		}
	}
	for (const string_view word : query.minus_words) {
		if (word_to_document_freqs_.count(string(word)) == 0) {
			continue;
		}
		if (word_to_document_freqs_.at(string(word)).count(document_id)) {
			matched_words.clear();
			break;
		}
	}
	return {matched_words, documents_.at(document_id).status};
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy, string_view raw_query, int document_id) const {
	const Query query = ParseQuery(raw_query);
	
	vector<string_view> matched_words;
	for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(), [&](const string_view word){
		if (word_to_document_freqs_.count(string(word)) != 0) {
			if (word_to_document_freqs_.at(string(word)).count(document_id)) {
				matched_words.push_back(word);
			}
		}
	});
	for_each(/*std::execution::par,*/ query.minus_words.begin(), query.minus_words.end(), [&](const string_view word){
		if (word_to_document_freqs_.count(string(word)) != 0) {
			if (word_to_document_freqs_.at(string(word)).count(document_id)) {
				matched_words.clear();
				return ;
			}
		}
	});
	return { matched_words, documents_.at(document_id).status };
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy, string_view raw_query, int document_id) const {
	const Query query = ParseQuery(raw_query);
	
	vector<string_view> matched_words;
	for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(), [&](const string_view word){
		if (word_to_document_freqs_.count(string(word)) != 0) {
			if (word_to_document_freqs_.at(string(word)).count(document_id)) {
				matched_words.push_back(word);
			}
		}
	});
	for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [&](const string_view word){
		if (word_to_document_freqs_.count(string(word)) != 0) {
			if (word_to_document_freqs_.at(string(word)).count(document_id)) {
				matched_words.clear();
				return ;
			}
		}
	});
	return {matched_words, documents_.at(document_id).status};
}


bool SearchServer::IsStopWord(string_view word) const {
	return stop_words_.count(string(word)) > 0;
}


bool SearchServer::IsValidWord(string_view word) {
	// A valid word must not contain special characters
	return none_of(word.begin(), word.end(), [](char c) {
		return c >= '\0' && c < ' ';
	});
}


vector<string> SearchServer::SplitIntoWordsNoStop(string_view text) const {
	vector<string> words;
	for (const string_view word : SplitIntoWords(text)) {
		if (!IsValidWord(word)) {
			throw invalid_argument("Word "s + string(word) + " is invalid"s);
		}
		if (!IsStopWord(word)) {
			words.push_back(string(word));
		}
	}
	return words;
}


int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
	if (ratings.empty()) {
		return 0;
	}
	int rating_sum = 0;
	if (!ratings.empty()) {
		rating_sum = accumulate(ratings.begin(), ratings.end(), 0) /
		             static_cast<int>(ratings.size());
	}
	return rating_sum;
}


SearchServer::QueryWord SearchServer::ParseQueryWord(string_view word) const {
	if (word.empty()) {
		throw invalid_argument("Query word is empty"s);
	}
	bool is_minus = false;
	if (word[0] == '-') {
		is_minus = true;
		word.remove_prefix(1);
	}
	if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
		throw invalid_argument("Query word "s + string(word) + " is invalid");
	}
	return {word, is_minus, IsStopWord(word)};
}


SearchServer::Query SearchServer::ParseQuery(string_view text) const {
	Query result;
	for (const string_view word : SplitIntoWords(text)) {
		const auto query_word = ParseQueryWord(word);
		if (!query_word.is_stop) {
			if (query_word.is_minus) {
				result.minus_words.insert(query_word.data);
			}
			else {
				result.plus_words.insert(query_word.data);
			}
		}
	}
	return result;
}


// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const string &word) const {
	return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}


void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
                 const vector<int>& ratings) {
	try {
		search_server.AddDocument(document_id, document, status, ratings);
	} catch (const invalid_argument& e) {
		cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
	}
}


void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
	LOG_DURATION_STREAM("FindTopDocuments", cout);
	cout << "Результаты поиска по запросу: "s << raw_query << endl;
	try {
		for (const Document& document : search_server.FindTopDocuments(raw_query)) {
			PrintDocument(document);
		}
	} catch (const invalid_argument& e) {
		cout << "Ошибка поиска: "s << e.what() << endl;
	}
}

void MatchDocuments(const SearchServer& search_server, string_view query) {
	LOG_DURATION_STREAM("MatchDocuments", cout);
	try {
		cout << "Матчинг документов по запросу: "s << query << endl;
		for (auto it = search_server.begin(); it != search_server.end(); ++it) {
			const int document_id = *it;
			const auto [words, status] = search_server.MatchDocument(query, document_id);
			PrintMatchDocumentResult(document_id, words, status);
		}
	} catch (const invalid_argument& e) {
		cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
	}
}

