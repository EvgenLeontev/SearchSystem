#pragma once
#include <iostream>
#include <string>
#include <set>
#include <vector>
#include <string_view>

std::vector<std::string_view> SplitIntoWords(std::string_view text);

template <typename StringContainer>
std::set<std::string_view> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
	using namespace std;
	std::set<std::string_view> non_empty_strings;
	for (std::string_view str : strings) {
		if (!str.empty()) {
			non_empty_strings.insert(str);
		}
	}
	return non_empty_strings;
}