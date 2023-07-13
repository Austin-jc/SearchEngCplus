#include <iostream>
#include <string>
#include <filesystem>
#include <cstring>
#include <fstream>
#include "json.hpp"
#include <queue>
#include "OleanderStemmingLibrary/include/olestem/stemming/english_stem.h"
using namespace std;
using json = nlohmann::json;
namespace fs = std::filesystem;

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
class Bm25 {
public:
  float avgDoclen;
  unordered_map<string, unordered_map<string, string>> metadata;
  unordered_map<int, unordered_map<int, int>> invertedIndex;
  unordered_map<string, int> lexicon;
  int totalNumDocs = 131896;

  float calculateAverageDoclength() {
    float sum = 0;
    int count =0;
    for (auto& [docno, data] : metadata) {
      if (!data.contains("docLength")) { continue; }
      sum += stoi(data["docLength"]);
      count++;
    }
    return sum/count;
  }
  void loadMetadata(fs::path metadataPath) {
    cout << "loading metadata" << endl;
    ifstream metadataStream(metadataPath);
    json j;
    j << metadataStream;
    for (auto& [docno, data] : j.items()) {
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
    avgDoclen = calculateAverageDoclength();
    cout << "avg doc length: " << avgDoclen << endl;
  }

  void loadLexicon(fs::path lexiconPath) {
    cout << "loading lexicon" << endl;
    json j;
    // load lexicon and inverted index
    ifstream lexiconInStream(lexiconPath);
    if (lexiconInStream.is_open()) {
      j << lexiconInStream;
    }
    lexicon = j.get<unordered_map<string, int>>();
  }

  void loadInvertedIndex(fs::path invertedIndexPath) {
    cout << "loading inverted Index" << endl;
    json j;
    ifstream invertedIndexInStream(invertedIndexPath);
    j << invertedIndexInStream;
    for (const auto&[termId, posting] : j.items()) { // returns termid posting list pair
      unordered_map<int, int> postingsList;
      for (const auto&[docid, count] : posting.items()) { // returns docid and counts
        postingsList[atoi(docid.c_str())] = count;
      }
      invertedIndex[atoi(termId.c_str())] = postingsList;
    }
  }

  int countFreqInQuery(string term, vector<string> &tokens) {
    int count = 0;
    for (string s : tokens) {
      if (s == term) { count++; }
    }
    return count;
  }
// calculate BM25
  unordered_map<int, float> calculateBM25(vector<string> &tokens, float k1 = 1.2, float k2 = 7, float b = 0.75) {
    unordered_map<int, float> accumulator;
    unordered_map<int, int> postingsList;
    int ni, q, doclen;

    for (int i = 1; i < tokens.size(); i++) {
      postingsList = invertedIndex[lexicon[tokens[i]]];
      ni = postingsList.size();
      q = countFreqInQuery(tokens[i], tokens);
      for (auto& [docno, fi] : postingsList) {
        doclen = stoi(metadata[metadata[to_string(docno)][""]]["docLength"]);
        float K = k1*((1-b)+b*doclen/avgDoclen);
        float val = ((k1+1)*fi / (K+fi)) * ((k2+1)*q / (k2 + q)) * log((totalNumDocs-ni+0.5) / (ni+0.5));
        accumulator[docno] += val;
      }
    }
    return accumulator;
  }
  // given tokens go through every doc
  void getRankings(vector<string> &tokens, stringstream &ss) {
    auto comparator = [](pair<string, float>& a, pair<string, float>& b) {
        return a.second < b.second;
    };
    priority_queue<pair<string, float>, vector<pair<string, float>>, decltype(comparator)> q;
    unordered_map<int, float> scores = calculateBM25(tokens);
    for (auto& [docno, score] : scores) {
      q.push({metadata[to_string(docno)][""], score});
    }
    int n = 0;
    while(!q.empty() && n < 1001) {
      n++;
      ss << tokens[0] << " "
         << "Q0 "
         << q.top().first << " "
         << n << " "
         << q.top().second << " "
         << "ajconAND" << endl;
      q.pop();
    }
  }
};

using convert_t = codecvt_utf8<wchar_t>;
wstring_convert<convert_t, wchar_t> strconverter;

string fromWS_to_string(std::wstring wstr)
{
  return strconverter.to_bytes(wstr);
}

wstring to_wstring(std::string str)
{
  return strconverter.from_bytes(str);
}
vector<string> stemTokens(vector<string> &tokens) {
  stemming::english_stem<> stem;
  vector<string> stemmedTokens;
  for (string s : tokens) {
    wstring ws = to_wstring(s);
    stem(ws);
    string stemmedString = fromWS_to_string(ws);
    stemmedTokens.push_back(stemmedString);
  }
  return stemmedTokens;
}
int main(int argc, char **argv) {
  // if no arguments just run the driver code for HW.
  if (argc == 1) {
    Bm25 bm25;
    bm25.loadMetadata("..\\metadata.json");
    bm25.loadLexicon("..\\stemmed_lexicon.json");
    bm25.loadInvertedIndex("..\\stemmed_invertedIndex.json");
    fs::path queriesFile("..\\queries.txt");
    fs::path outputFileName("..\\hw4-bm25-stem-ajcon.txt");
    cout << "parsing queries" << endl;
    // parse Queries file
    stringstream ss;
    string q, line, word;
    vector<string> tokens;
    vector<int> results;
    ifstream queryStream(queriesFile);
    while(getline(queryStream, q)) {
      tokens = tokenize(q);
      vector<string> stemmed = stemTokens(tokens);
      bm25.getRankings(stemmed, ss);
    }
    ofstream output(outputFileName);
    output << ss.rdbuf();
    return 0;
  }

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

  Bm25 bm25;
  bm25.loadMetadata(indexPath / metadataFileName);
  bm25.loadLexicon(indexPath / lexiconFileName);
  bm25.loadInvertedIndex(indexPath / invertedIndexFileName);

  cout << "parsing queries" << endl;
  stringstream ss;
  string q, line, word;
  vector<string> tokens;
  vector<int> results;
  ifstream queryStream(queriesFile);
  while(getline(queryStream, q)) {
    tokens = tokenize(q);
    vector<string> stemmed = stemTokens(tokens);
    bm25.getRankings(stemmed, ss);
  }
  ofstream output(outputFileName);
  output << ss.rdbuf();
  return 0;
}