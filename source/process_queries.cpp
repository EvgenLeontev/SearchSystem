#include "process_queries.h"

#include <algorithm>
#include <numeric>
#include <functional>
//#include <tbb/tbb.h>
#include <execution>


using namespace std;

vector<vector<Document>> ProcessQueries( const SearchServer& search_server,
											const vector<string>& queries) {
	vector<vector<Document>> result(queries.size());
	transform(execution::par, queries.begin(), queries.end(), result.begin(), [&search_server](const string& query){
		return search_server.FindTopDocuments(query);});
	return result;
}

vector<Document> ProcessQueriesJoined(const SearchServer& search_server,
													const vector<string>& queries) {
	int total_size = 0;
	vector<vector<Document>> process_queries_result = ProcessQueries(search_server, queries);

	vector<size_t> v(process_queries_result.size());
	
	transform_inclusive_scan(execution::par, process_queries_result.begin(),
				process_queries_result.end(), v.begin(), plus<>{},
				[](const vector<Document>& vd){ return vd.size(); }, 0);

	total_size = v.back();
	vector<Document> result(total_size);
	auto it = result.begin();
	for(auto & i : process_queries_result) {
		it = transform(execution::par, i.begin(), i.end(), it,
		                              [](const Document &doc) { return doc; });
	}
	return result;
}
