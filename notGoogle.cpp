#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <queue>
#include "json.hpp"
#include <unordered_set>
#include <algorithm>
namespace fs = std::filesystem;
using namespace std;
using json = nlohmann::json;

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
    string indexPath = "..\\latimesIndex2\\";
    unordered_map<string, unordered_map<string, string>> metadata;
    unordered_map<int, unordered_map<int, int>> invertedIndex;
    unordered_map<string, int> lexicon;
    unordered_set<char> delineators = {'.', '!', '?'};
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
//      cout << "avg doc length: " << avgDoclen << endl;
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

      for (int i = 0; i < tokens.size(); i++) {
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

    string getDoc(string docno) {
      cout << indexPath + metadata[docno]["relativePath"] + ".txt" << endl;
      ifstream doc = ifstream(indexPath + metadata[docno]["relativePath"] + ".txt");
      // https://stackoverflow.com/questions/2912520/read-file-contents-into-a-string-in-c
      string output( (istreambuf_iterator<char>(doc)), (istreambuf_iterator<char>()) );
      return output;
    }

    float rankLine(vector<string>& query, unordered_set<string> queryWords, vector<string> line) {
      int c = 0, d, k = 0, n = 0; // i do not see the relevance of h and l and so i choose to weight them as zero.
      unordered_set<string> uniqueW;
      for (string s : line) {
        if (queryWords.contains(s)) {
          c++;
          if (query[n] == s) {
            n++;
          }
          uniqueW.insert(s);
        } else {
          n = 0;
          k = max(k, n);
        }
      }
      d = size(uniqueW);
      return d + 1.25*c + 0.75*k;
    }

    vector<string> tokenizeP(string text, unordered_set<char> &delineators) {
      transform(text.begin(), text.end(), text.begin(), ::tolower);
      vector<string> tokens;
      string sentence;
      for (char c : text) {
        if (delineators.find(c) == delineators.end()) {
          sentence += c;
        } else {
          sentence +=c;
          tokens.push_back(sentence);
          sentence = "";
        }
      }
      tokens.push_back(sentence);
      return tokens;
    }

    string getQuerySnippet(vector<string> query, unordered_set<string> queryWords, string relativePath) {
      ifstream doc = ifstream(indexPath + relativePath  + ".txt");
      auto comparator = [](pair<string, float>& a, pair<string, float>& b) {
          return a.second < b.second;
      };
      priority_queue<pair<string, float>, vector<pair<string, float>>, decltype(comparator)> q;

      string output, temp, para;
      float score = 0;
      bool isP, isText;
      while (getline(doc, temp)) {
        if (temp == "<TEXT>") { isText = true; }
        else if (temp == "<P>" && isText) { isP = true; }
        else if (temp == "</P>") {
          vector<string> tokens = tokenizeP(para, delineators);
          for (string s : tokens) {
            score += rankLine(query, queryWords, tokenize(s));
          }
          score = score/size(tokens);
          q.push({para, score});
          score = 0;
          para = "";
          isP = false;
        }
        else if (isP) {
          if (temp.at(0) != '<') {
            para += temp;
          }
        }
      }
      return q.top().first;
    }

    string getSnippet(string relativePath) {
      ifstream doc = ifstream(indexPath + relativePath  + ".txt");
      string output, temp;
      float score;
      bool start = false;
      while (getline(doc, temp)) {
        if (temp == "<TEXT>") { start = true; }
        if (temp == "</P>" && start == true) { return output; }
        if (start) {
          if (temp.at(0) != '<') {
            output += temp;
          }
        }
      }
      return output;
    }

    vector<string> getRankings(vector<string> &tokens, int numRanks) {
      auto comparator = [](pair<string, float>& a, pair<string, float>& b) {
          return a.second < b.second;
      };
      priority_queue<pair<string, float>, vector<pair<string, float>>, decltype(comparator)> q;
      unordered_map<int, float> scores = calculateBM25(tokens);
      for (auto& [docno, score] : scores) {
        q.push({metadata[to_string(docno)][""], score});
      }
      int n = 0;
      string headline, date, snippet, doc, docno;
      unordered_set<string> querySet;
      for (string s : tokens) {
        querySet.insert(s);
      }
      vector<string> rankings;
      while(!q.empty() && n < numRanks) {
        docno = q.top().first;
//        snippet = getSnippet(metadata[docno]["relativePath"]);
        snippet = getQuerySnippet(tokens, querySet, metadata[docno]["relativePath"]);
        headline = metadata[docno]["headline"];

        if (headline.length() == 0) {
          headline = snippet.substr(0,max(50, (int) headline.length()));
        }
        date = metadata[docno]["date"];
        n++;
        cout << n << ". " << headline << " " << date << endl << endl;
        cout << snippet << endl << "(" + docno + ")" << endl << endl;
        rankings.push_back(docno);
        q.pop();
      }
      return rankings;
    }
};

bool isNumber(string s) {
  for (char c : s) {
    if (!isdigit(c)) {
      return false;
    }
  }
  return true;
}

int main (int argc, char** argv) {
  int numRanks = 10;
  Bm25 bm25;
  // load inverted index, metadata and lexicon
  bm25.loadMetadata("..\\metadata.json");
  bm25.loadLexicon("..\\lexicon.json");
  bm25.loadInvertedIndex("..\\invertedIndex.json");

  string input, doc;
  vector<string> tokens, rankings;
  long rankingTime, retrievalTime;
  while (true) {
    cout << "please enter your query: ";
    getline(cin, input);
    auto start = chrono::steady_clock::now();
    tokens = tokenize(input);
    rankings = bm25.getRankings(tokens, numRanks);
    auto end = chrono::steady_clock::now();
    retrievalTime = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    // loop through this too.
    cout << "retrieval took " << retrievalTime << " ms" << endl;
    while (true) {
      cout << endl << "would you like to view a doc (enter number), start a new query (n) or quit (q)? ";
      getline(cin, input);
      input.erase(remove(input.begin(), input.end(), ' '), input.end());
      if (input == "q" || input == "Q") {
        cout << endl << " \\( ^o^)/\\(^_^ )bye bye!";
        return 0;
      }
      else if (input == "n" || input == "N") { break; }
      else if (isNumber(input)) {
        int rank = stoi(input);
        if (rank <= rankings.size()) {
          start = chrono::steady_clock::now();
          doc = bm25.getDoc(rankings[rank-1]);
          end = chrono::steady_clock::now();
          retrievalTime = chrono::duration_cast<chrono::milliseconds>(end - start).count();
          cout << doc << endl;
        }
      } else {
        cout << "invalid input" <<endl;
      }
    }
  }
  return 0;
}
