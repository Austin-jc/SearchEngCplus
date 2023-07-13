#include <iostream>
#include <string>
#include <filesystem>
#include <zlib.h>
#include <cstring>
#include <fstream>
#include "json.hpp"
#include "OleanderStemmingLibrary/include/olestem/stemming/english_stem.h"
#include <codecvt>
//#include "simdjson.h"
#include <chrono>
#include <unistd.h>
namespace fs = std::filesystem;
using namespace std;
using json = nlohmann::json;

string getNestedParagraphs(gzFile f, char* closingTag, string& raw) {
  string values;
  char buffer[256];
  while(gzgets(f, buffer, 256) != nullptr) {
    raw += buffer;
    if (strcmp(buffer, closingTag) == 0) {
      break;
    }
    values = values + buffer;
  }
  return values;
}

string getNestedParagraphsText(ifstream& in, char* closingTag) {
  string values, line;
  while(getline(in, line)) {
    if  (line == closingTag) {
      break;
    }
    values += line;
  }
  return values;
}

vector<string> parseLine(const char* S)
{
  // adapted from https://www.geeksforgeeks.org/html-parser-in-c-cpp/
  int n = strlen(S);
  int start;
  string tag, value;
  for (int i = 0; i < n; i++) {
    if (S[i] == '>') {
      start = i + 1;
      tag += '>';
      break;
    }
    tag += S[i];
  }
  for (int i = start; i < n; i++) {
    if (S[i] == ' ') {
      i++;
    }
    if (S[i] == '<') {
      break;
    } else {
      value += S[i];
    }
  }
  return vector<string> {tag, value};
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

string getStrippedText(string text, string tag = "0") {
  string output;
  if (tag == "0") { // tag == 0 -> strip all tags
    int content = 0;
    for (char c : text ) {
      if (c == '>') { // content starts
        content = 1;
      } else if (c == '<') { // content stops
        content = 0;
      } else if (content == 1 && c != '\n') { // if the character is part of text
        output += c;
      }
    }
  }
  return output;
}

void writeToDirectory(string directoryPath, string filename, auto data) {
  fs::path path(directoryPath);
  if (!fs::exists(path)) { // ensure that directory exists
    fs::create_directories(path);
  }
  ofstream f(directoryPath + "\\" + filename);
  f << data;
  f.close();
}

class DataBuilder {
  private:
      fs::path dataPath, lexiconFile, invertedIndexFile;
      unordered_map<string, int> lexicon;
      unordered_map<int, unordered_map<int, int>> invertedIndex; // term id to postings

  public:
    DataBuilder(string targetPath) {
      fs::path path(targetPath);
      fs::path lexiconFileName("stemmed_lexicon.json");
      fs::path invertedIndexFileName("stemmed_invertedIndex.json");
      dataPath = path;
      lexiconFile = dataPath / lexiconFileName;
      invertedIndexFile = dataPath / invertedIndexFileName;
    }
    unordered_map<string, int> getLexicon() {
      return lexicon;
    }
    unordered_map<int, unordered_map<int, int>> getInvertedIndex() {
      return invertedIndex;
    }
    void openLexicon() {
      json j;
      // if lexicon exists, load it
      if (fs::exists(lexiconFile)) {
        cout << "loading lexicon" << endl;
        ifstream f(dataPath / lexiconFile);
        f >> j;
        lexicon = j.get<unordered_map<string, int>>();
        f.close();
      } else {
      }
    }
    void writeToLexicon() {
      // write to file.
      ofstream o(lexiconFile);
      json jOut(lexicon);
      o << jOut;
      o.close();
    }

    void buildLexicon(vector<string> &tokens) {
      int tokenId;
      for (string t : tokens) {
        if (!lexicon.contains(t)) {
          tokenId = lexicon.size();
          lexicon[t] = tokenId;
          // lexicon[tokenId] = t;
        }
      }
    }

    int getLexiconLen(){ return lexicon.size(); }

    // map term id to map of doc no/term count pair
    void buildInvertedIndex(vector<string> &tokens, int docId) {
      unordered_map<int, int> wordCounts;
      for (string token : tokens) {
         wordCounts[lexicon[token]]++;
      }
      for (auto it : wordCounts) {
        invertedIndex[it.first][docId] = it.second;
      }
    }

    void writeToInvertedIndex() {
      json jOut;
      ofstream o(invertedIndexFile);
      for (auto& [k, v] : invertedIndex) {
        json j2;
        for (auto& [k2, v2] : v) {
          j2[to_string(k2)] = v2;
        }
        jOut[to_string(k)] = j2;
      }
      o << jOut;
      o.close();
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
void parseGz(char* path2gz, string path2index) {
  DataBuilder dataBuilder(path2index);
  unordered_map<string, string> num2months = {
          {"01", "January"},
          {"02", "February"},
          {"03", "March"},
          {"04", "April"},
          {"05", "May"},
          {"06", "June"},
          {"07", "July"},
          {"08", "August"},
          {"09", "September"},
          {"10", "October"},
          {"11", "November"},
          {"12", "December"}
  };
  
  // Data to store
  string docno, docid, date, day, month, year, datestampPath, headline, raw, text;
  vector<string> tokens, data, stemmedTokens;
  int chunk = 256, internalId = 0, progressCounter = 0;
  char buffer[chunk];
  json metadata;
  unordered_map<string, map<string, string>> mdMap;

  dataBuilder.openLexicon();
  gzFile f = gzopen(path2gz, "r");
  while (gzgets(f, buffer, chunk) != nullptr) {
    raw += buffer;
    if (buffer[0] == '<') { // if its a tag
      data = parseLine(buffer);
      if (data[0] == "<DOCNO>") { // extract date information
        docno = data[1];
        month = docno.substr(2,2);
        day = docno.substr(4,2);
        year = docno.substr(6,2);
        date = num2months[month] + " " + day + ", " + "19" + year;
        datestampPath = month + "/" + day + "/" + year + "/";
      }
      else if (data[0] == "<HEADLINE>") {
        headline = getStrippedText(getNestedParagraphs(f, "</HEADLINE>\n", raw));
        text += headline;
      }
      else if (data[0] == "<TEXT>") {
        text += getStrippedText(getNestedParagraphs(f, "</TEXT>\n", raw));
      }
      else if (data[0] == "<GRAPHIC>") {
        text += getStrippedText(getNestedParagraphs(f, "</GRAPHIC>\n", raw));
      }
      else if (data[0] == "</DOC>") { // end of doc -> write out everything
        progressCounter++;
        if (progressCounter%10000 == 0) {
          cout << ".";
        }
        tokens = tokenize(text); // tokenize the doc
        stemmedTokens = stemTokens(tokens);

        dataBuilder.buildLexicon(stemmedTokens); // add words to lexicon

        metadata[docno] = {
            {"docno", docno},
            {"id", internalId},
            {"date", date},
            {"headline", headline},
            {"relativePath", datestampPath + docno},
            {"docLength", tokens.size()}
          };
        metadata[to_string(internalId)] = docno;
        writeToDirectory(path2index + datestampPath, docno + ".txt", raw);
        dataBuilder.buildInvertedIndex(stemmedTokens, internalId);
        raw = "";
        text = "";
        internalId++;
        continue;
      }
    }
  }
  cout << endl << "lexicon Length" <<endl;
  cout << to_string(dataBuilder.getLexiconLen()) << endl;
  cout << "number of docs" << endl;
  cout << to_string(progressCounter) << endl;

  cout << endl << "writing lexicon" << endl;
  dataBuilder.writeToLexicon();
  writeToDirectory(path2index, "metadata.json", metadata);
  cout  << "writing InvertedIndex" << endl;
  dataBuilder.writeToInvertedIndex();
  cout<<"done";
}

int main(int argc, char **argv) {

 if (argc < 3)
 {
   cout << "not enough arguments supplied.  Usage: a1 <path to .gz> <path to data folder>";
   return 0;
 }

 char* path2gz = argv[1];
 string indexPath = argv[2];
 if (!indexPath.ends_with('/') || !indexPath.ends_with('\\')) {
   indexPath = indexPath + '/';
 }

 fs::path gz(path2gz);
 if (!fs::exists(gz)) {
   cout << "location of .gz is incorrect";
   return 0;
 }
 // if the index does not exist, create it.
 fs::path path2index(indexPath);
 if (fs::exists(path2index)) {
   cout << "Directory already exists";
   return 0;
 } else {
   // else create it
   fs::create_directory(path2index);
   cout << "created";
 }

 parseGz(path2gz, indexPath);
  return 0;
}
