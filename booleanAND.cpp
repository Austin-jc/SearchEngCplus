#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include "json.hpp"

using namespace std;
namespace fs = std::filesystem;
using json = nlohmann::json;

void printLexicon(unordered_map<string, int> lexicon) {
  for (auto& [k,v] : lexicon) {
    cout << k << " " << to_string(v) << endl;
  }
}
void printInvertedIndex(unordered_map<int, unordered_map<int, int>> invIndex) {
  for (auto& [k,v] : invIndex) {
    cout << k << endl;
    for (auto& [k2,v2] : v) {
      cout << k2 << " " << v2 << endl;
    }
  }
}
void printMetadata(unordered_map<string, unordered_map<string, string>> metadata) {
  for (auto& [docno, data] : metadata) {
    cout << docno << endl;
    for (auto& [info, value] : data) {
      cout << info << " " << value << endl;
    }
  }
}

vector<string> tokenize(string text) {
  transform(text.begin(), text.end(), text.begin(), ::tolower);
  vector<string> tokens;
  string word;
  int started = 0;
  for (char c : text) {
    if (isalnum(c)) {
      if (!started) {
        started = 1;
      }
      word += c;
    } else {
      if (started) {
        started = 0;
        tokens.push_back(word);
        word = "";
      }
    }
  }
  if (isalnum(word[0])) {
    tokens.push_back(word);
  }
  return tokens;
}

vector<int> naiveIntersection2(vector<string> tokens, unordered_map<string, int> &lexicon, unordered_map<int, unordered_map<int, int>> &invertedIndex) {
  unordered_map<int, int> docCount, postings;
  vector<int> res;
  int notInLexicon;
  for (int i = 1; i < tokens.size(); i++) { // start at 1 to ignore q number
    if (!lexicon.contains(tokens[i])) {
      notInLexicon++;
      continue;
    }
    postings = invertedIndex[lexicon[tokens[i]]];
    for (const auto& [doc, count] : postings) {
      docCount[doc]++;
    }
  }
  for (auto& [docId, count] : docCount) {
    if (docCount[docId] == tokens.size() - 1 - notInLexicon) {
      res.push_back(docId);
    }
  }
  return res;
}

vector<int> naiveIntersection(ifstream queryFilePath, unordered_map<string, int> lexicon, unordered_map<int, unordered_map<int, int>> invertedIndex) {
  ifstream &queries(queryFilePath);
  // parse Queries file - naive approach
  string q, qNumber;
  int termId;
  vector<string> tokens;
  unordered_map<int, int> docCount, postings;
  vector<int> res;
  while(getline(queries, q)) {
    tokens = tokenize(q);
    qNumber = tokens[0];
    for (int i = 1; i < tokens.size(); i++) {
      termId = lexicon[tokens[i]];
      for (const auto& doc : invertedIndex[termId]) {
        docCount[doc.first]++;
      }
    }
    for (auto& docId : docCount) {
      if (docCount[docId.first] == tokens.size() - 1) {
        res.push_back(docId.first);
      }
    }
  }
}

int main(int argc, char **argv) {
  if (argc < 4)
  {
    cout << "not enough arguments supplied.";
    return 0;
  }
  fs::path indexPath(argv[1]);
  fs::path queriesFile(argv[2]);
  fs::path outputFileName(argv[3]);

  fs::path lexiconFileName("lexicon.json");
  fs::path invertedIndexFileName("invertedIndex.json");
  fs::path metadataFileName("metadata.json");

  if (!fs::exists(indexPath)
      || !fs::exists(indexPath / lexiconFileName)
      || !fs::exists(indexPath / invertedIndexFileName)
      || !fs::exists(indexPath / metadataFileName)
      || !fs::exists(queriesFile)) {
    cout << "invalid file path" << endl;
    return 0;
  }

  unordered_map<string, int> lexicon;
  unordered_map<string, unordered_map<string, string>> metadata;
  unordered_map<int, unordered_map<int, int>> invertedIndex;
  json j1, j2, j3;

  cout << "loading lexicon" << endl;
  // load lexicon and inverted index
  ifstream lexiconInStream(indexPath / lexiconFileName);
  if (lexiconInStream.is_open()) {
    j1 << lexiconInStream;
  }
  lexicon = j1.get<unordered_map<string, int>>();

  cout << "loading inverted Index" << endl;
  ifstream invertedIndexInStream(indexPath / invertedIndexFileName);
  j2 << invertedIndexInStream;
  for (const auto&[termId, posting] : j2.items()) { // returns termid posting list pair
    unordered_map<int, int> postingsList;
    for (const auto&[docid, count] : posting.items()) { // returns docid and counts
      postingsList[atoi(docid.c_str())] = count;
    }
    invertedIndex[atoi(termId.c_str())] = postingsList;
  }

  cout << "loading metadata" << endl;
  ifstream metadataStream(indexPath / metadataFileName);
  j3 << metadataStream;
  for (auto& [docno, data] : j3.items()) {
    unordered_map<string, string> meta;
    for (auto[key, value] : data.items()) {
      if (value.is_number()) {
        meta[key] = to_string(value);
      } else {
        meta[key] = value;
      }
    }
    metadata[docno] = meta;
  }
  cout << "parsing queries" << endl;
  // parse Queries file - naive approach
  stringstream ss;
  string q, line, word;
  vector<string> tokens;
  vector<int> results;
  ifstream queryStream(queriesFile);
  while(getline(queryStream, q)) {
    tokens = tokenize(q);
    results = naiveIntersection2(tokens, lexicon, invertedIndex);
    for (int i = 0; i < results.size(); i++) {
      ss << tokens[0] << " "
             << "Q0 "
             << metadata[to_string(results[i])][""] << " "
             << i+1 << " "
             << results.size() - i << " "
             << "ajconAND" << endl;
    }
  }
  ofstream output(outputFileName);
  output << ss.rdbuf();
  return 0;
}
