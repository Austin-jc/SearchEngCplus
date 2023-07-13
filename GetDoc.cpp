//
// Created by austin on 9/25/2021.
//
//
#include <stdio.h>
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include "json.hpp"

using namespace std;
namespace fs = std::filesystem;
using json = nlohmann::json;

void printData(json j, string docno, string indexPath) {
  json metadata = j[docno];
  string relativePath =  metadata["relativePath"];
  ifstream doc = ifstream(indexPath + relativePath  + ".txt");

  cout << "docno: " << metadata["docno"] << endl
       << "internal id: " << to_string(metadata["id"]) << endl
       << "date: " << metadata["date"] << endl
       << "headline: " << metadata["headline"] << endl << endl
       << "raw document: " << endl;
  cout << doc.rdbuf();
}

// testing method
void test() {
  char* path = "insert path here";
  string strPath = path;
  string idType = "id";
  string id = "2";
  string month, day, year, datestampPath;
  json j;

  ifstream f = ifstream(strPath + "metadata.json");
  f >> j;
  if (idType == "id") {
    id = j[id];
  }
  printData(j, id, path);
}

int main(int argc, char ** argv) {
  if (argc < 4)
    {
      cout << "Insufficient amount of Arguments supplied.  Usage: <path to data folder> <id type> <id>";
      return 0;
    }

  json j;
  string path = argv[1];
  string idType = argv[2];
  string id = argv[3];

  fs::path indexPath(path);
  if (!fs::exists(indexPath)) {
    cout << "index location malformed or does not exist";
    return 0;
  }

  if (!path.ends_with('/') || !path.ends_with('\\')) {
    path = path + '/';
  }
  ifstream f = ifstream(path + "metadata.json");
  f >> j;

  if (j[id] == nullptr) {
    cout << "id does not exist or is incorrect";
    return 0;
  }
  if (idType == "id") {
    id = j[id];
  } else if (idType != "docno") {
    cout << "invalid id type (docno or id)";
    return 0;
  }

  printData(j, id, path);
  return 0;
}

