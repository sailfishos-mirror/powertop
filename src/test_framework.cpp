/*
 * Copyright 2024, Intel Corporation
 *
 * This is part of PowerTOP
 *
 * This program file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; version 2 of the License.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "test_framework.h"

using namespace std;

test_framework_manager& test_framework_manager::get() {
	static test_framework_manager instance;
	return instance;
}

#ifdef ENABLE_TEST_FRAMEWORK
test_framework_manager::test_framework_manager() : recording(false), replaying(false) {}

test_framework_manager::~test_framework_manager() {
	if (recording) {
		save();
	}
}

void test_framework_manager::set_record(const string& filename) {
	recording = true;
	record_filename = filename;
}

void test_framework_manager::set_replay(const string& filename) {
	replaying = true;
	replay_filename = filename;
	load();
}

void test_framework_manager::record_read(const string& path, const string& content) {
	if (!recording) return;
	recorded_reads.push_back({path, content});
}

void test_framework_manager::record_read_fail(const string& path) {
	if (!recording) return;
	recorded_reads.push_back({path, "__POWERTOP_FILE_NOT_FOUND__"});
}

string test_framework_manager::replay_read(const string& path) {
	if (!replaying) return "";
	if (read_sequences[path].empty()) {
		throw test_exception("TEST FAIL: No more recorded content for read: " + path);
	}
	string content = read_sequences[path].front();
	read_sequences[path].pop_front();
	if (content == "__POWERTOP_FILE_NOT_FOUND__")
		return "";
	return content;
}

void test_framework_manager::record_write(const string& path, const string& content) {
	if (!recording) return;
	recorded_writes.push_back({path, content});
}

void test_framework_manager::replay_write(const string& path, const string& content) {
	if (!replaying) return;
	if (write_sequences[path].empty()) {
		cerr << "TEST FAIL: Unexpected write to: " << path << " (nothing recorded)" << endl;
		return;
	}
	string expected = write_sequences[path].front();
	write_sequences[path].pop_front();
	if (expected != content) {
		cerr << "TEST FAIL: Write mismatch for " << path << endl;
		cerr << "  Expected: " << expected << endl;
		cerr << "  Actual:   " << content << endl;
	}
}

void test_framework_manager::save() {
	ofstream file(record_filename);
	if (!file) {
		cerr << "Failed to open record file: " << record_filename << endl;
		return;
	}

	for (const auto& p : recorded_reads) {
		if (p.second == "__POWERTOP_FILE_NOT_FOUND__")
			file << "N " << p.first << endl;
		else
			file << "R " << p.first << " " << base64_encode(p.second) << endl;
	}
	for (const auto& p : recorded_writes) {
		file << "W " << p.first << " " << base64_encode(p.second) << endl;
	}
}

void test_framework_manager::load() {
	ifstream file(replay_filename);
	if (!file) {
		cerr << "Failed to open replay file: " << replay_filename << endl;
		return;
	}

	string line;
	while (getline(file, line)) {
		if (line.empty()) continue;
		stringstream ss(line);
		char type;
		string path, b64_content;
		ss >> type;
		if (type == 'N') {
			getline(ss, path);
			path = path.substr(1); // remove leading space
			read_sequences[path].push_back("__POWERTOP_FILE_NOT_FOUND__");
			continue;
		}
		ss >> path >> b64_content;
		if (type == 'R') {
			read_sequences[path].push_back(base64_decode(b64_content));
		} else if (type == 'W') {
			write_sequences[path].push_back(base64_decode(b64_content));
		}
	}
}

static const string base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

string test_framework_manager::base64_encode(const string& in) {
    string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

string test_framework_manager::base64_decode(const string& in) {
    string out;
    vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[base64_chars[i]] = i;

    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}
#else
test_framework_manager::test_framework_manager() {}
test_framework_manager::~test_framework_manager() {}
#endif
