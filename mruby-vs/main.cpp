#include <cstdlib>
#include <cstring>
#include <string>
#include <windows.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <list>
#include <direct.h>
#include <io.h>
#include "mruby/dump.h"
#include "mruby/../../src/opcode.h"
#include "mruby/string.h"

char *replace(const char *s, char ch, const char *repl) {
	int count = 0;
	const char *t;
	for (t = s; *t; t++)
		count += (*t == ch);

	size_t rlen = strlen(repl);
	char *res = (char*)malloc(strlen(s) + (rlen - 1)*count + 1);
	char *ptr = res;
	for (t = s; *t; t++) {
		if (*t == ch) {
			memcpy(ptr, repl, rlen);
			ptr += rlen;
		}
		else {
			*ptr++ = *t;
		}
	}
	*ptr = 0;
	return res;
}

struct State
{
	bool read = true;
	std::vector<std::pair<std::string, std::vector<std::string>>> texts;
	int lang_index;
};

int new_string(mrb_state *mrb, mrb_irep *irep, mrb_value val)
{
	size_t i;
	mrb_value *pv;

	switch (mrb_type(val)) {
	case MRB_TT_STRING:
		for (i = 0; i<irep->plen; i++) {
			mrb_int len;
			pv = &irep->pool[i];

			if (mrb_type(*pv) != MRB_TT_STRING) continue;
			if ((len = RSTRING_LEN(*pv)) != RSTRING_LEN(val)) continue;
			if (memcmp(RSTRING_PTR(*pv), RSTRING_PTR(val), len) == 0)
				return i;
		}
		break;
	default:
		throw 0;
		break;
	}

	irep->pool = (mrb_value*)mrb_realloc_simple(mrb, irep->pool, (irep->plen + 1) * sizeof(mrb_value));

	pv = &irep->pool[irep->plen];
	i = irep->plen++;

	switch (mrb_type(val)) {
	case MRB_TT_STRING:
		*pv = mrb_str_pool(mrb, val);
		break;
	default:
		throw 0;
		break;
	}
	return i;
}

void codedump(mrb_state *mrb, mrb_irep *irep, State* state)
{
	int i;
	int ai;
	mrb_code c_cur;

	if (!irep) return;

	for (i = 9; i < (int)irep->ilen; i++)
	{
		ai = mrb_gc_arena_save(mrb);
		c_cur = irep->iseq[i];
		if (GET_OPCODE(c_cur) == OP_SETCONST)
		{
			std::string id(mrb_sym2name(mrb, irep->syms[GETARG_Bx(c_cur)]));
			int c_arr = irep->iseq[i - 1];
			if (GET_OPCODE(c_arr) == OP_ARRAY && GETARG_C(c_arr) == 8)
			{
				std::vector<int> c_strs(8);
				c_strs[0] = irep->iseq[i - 9];
				c_strs[1] = irep->iseq[i - 8];
				c_strs[2] = irep->iseq[i - 7];
				c_strs[3] = irep->iseq[i - 6];
				c_strs[4] = irep->iseq[i - 5];
				c_strs[5] = irep->iseq[i - 4];
				c_strs[6] = irep->iseq[i - 3];
				c_strs[7] = irep->iseq[i - 2];

				bool all_right = true;
				for (int j = 0; j < (int)c_strs.size(); j++)
					if (GET_OPCODE(c_strs[j]) != OP_STRING)
					{
						all_right = false;
						break;
					}

				if (all_right)
				{
					if (state->read)
						state->texts.push_back(std::make_pair(id, std::vector<std::string>()));

					for (int j = 0; j < (int)c_strs.size(); j++)
					{
						if (state->read)
						{
							mrb_value v = irep->pool[GETARG_Bx(c_strs[j])];
							mrb_int len_test = RSTRING_LEN(v);
							state->texts.back().second.push_back(std::string(RSTRING_PTR(v), RSTRING_LEN(v)));
						}
						else
						{
							if (state->lang_index == -1 || state->lang_index == j)
							{
								int str_index = -1;
								for (int i = 0; i < (int)state->texts.size(); i++)
									if (state->texts[i].first == id)
									{
										str_index = i;
										break;
									}

								if (str_index != -1)
								{
									int test_c = MKOP_ABx(GET_OPCODE(c_strs[j]), GETARG_A(c_strs[j]), GETARG_Bx(c_strs[j]));
									if (test_c != c_strs[j])
										throw 0;

									mrb_value v = mrb_str_new(mrb, state->texts[str_index].second[j].c_str(), state->texts[str_index].second[j].size());

									int new_bx = new_string(mrb, irep, v);
									//if (GETARG_Bx(c) != new_bx)
									//	throw 0;

									irep->iseq[i - 9 + j] = MKOP_ABx(GET_OPCODE(c_strs[j]), GETARG_A(c_strs[j]), new_bx);
								}
							}
						}
					}
				}
			}
		}
		mrb_gc_arena_restore(mrb, ai);
	}
}

void codedump_recur(mrb_state *mrb, mrb_irep *irep, State * state)
{
	size_t i;

	codedump(mrb, irep, state);
	for (i = 0; i<irep->rlen; i++) {
		codedump_recur(mrb, irep->reps[i], state);
	}
}

extern "C" void codedump_recur(mrb_state *mrb, mrb_irep *irep);

std::string ReplaceAll(std::string str, const std::string& from, const std::string& to)
{
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
	}
	return str;
}

void write_line(std::ofstream & stream, std::string line)
{
	std::string escaped_line = ReplaceAll(line, "\n", "\\n");
	stream.write(escaped_line.c_str(), escaped_line.size());
	stream.write("\r\n", 2);
}

void getline(std::ifstream & f, std::string & s)
{
	std::getline(f, s);
	s = ReplaceAll(s.substr(0, s.size() - 1), "\\n", "\n");
}

bool folderExists(const char* folderName) {
	if (_access(folderName, 0) == -1) {
		//File not found
		return false;
	}

	DWORD attr = GetFileAttributes((LPCSTR)folderName);
	if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
		// File is not a directory
		return false;
	}

	return true;
}

bool createFolder(std::string folderName) {
	std::list<std::string> folderLevels;
	char* c_str = (char*)folderName.c_str();

	// Point to end of the string
	char* strPtr = &c_str[strlen(c_str) - 1];

	// Create a list of the folders which do not currently exist
	do {
		if (folderExists(c_str)) {
			break;
		}
		// Break off the last folder name, store in folderLevels list
		do {
			strPtr--;
		} while ((*strPtr != '\\') && (*strPtr != '/') && (strPtr >= c_str));
		folderLevels.push_front(std::string(strPtr + 1));
		strPtr[1] = 0;
	} while (strPtr >= c_str);

	if (_chdir(c_str)) {
		return true;
	}

	// Create the folders iteratively
	for (auto it = folderLevels.begin(); it != folderLevels.end(); it++) {
		if (CreateDirectory(it->c_str(), NULL) == 0) {
			return true;
		}
		_chdir(it->c_str());
	}

	return false;
}

void find_files(std::string mode, int lang_index, std::vector<std::pair<std::string, State>> & files, std::string dir_in, std::string bin_dir_out, std::string current_dir)
{
	WIN32_FIND_DATA fd;
	HANDLE hFind = ::FindFirstFile((dir_in + (current_dir.empty() ? "" : "\\") + current_dir + R"(\*)").c_str(), &fd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			std::string file_name(fd.cFileName);
			if (file_name == "." || file_name == "..")
				continue;

			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				find_files(mode, lang_index, files, dir_in, bin_dir_out, current_dir + (current_dir.empty() ? "" : "\\") + file_name);
				continue;
			}

			if (file_name.substr(file_name.size() - 4) != ".bin")
				continue;

			std::cout << (current_dir + "\\" + file_name) << std::endl;

			mrb_state * state = mrb_open();

			FILE * fin = nullptr;
			errno_t err1 = fopen_s(&fin, (dir_in + (current_dir.empty() ? "" : "\\") + current_dir + "\\" + file_name).c_str(), "rb");
			if (!fin)
				throw 0;
			mrb_irep * irep = mrb_read_irep_file(state, fin);
			fclose(fin);

			//codedump_recur(state, irep);

			if (mode == "u")
			{
				State st;
				codedump_recur(state, irep, &st);
				files.push_back(std::make_pair(current_dir + "\\" + file_name, st));
			}
			if (mode == "p")
			{
				State st;
				for (int i = 0; i < (int)files.size(); i++)
				{
					if (files[i].first == current_dir + "\\" + file_name)
					{
						st = files[i].second;
						break;
					}
				}
				st.read = false;
				st.lang_index = lang_index;
				codedump_recur(state, irep, &st);

				char cpath[MAX_PATH + 1];
				_getcwd(cpath, MAX_PATH);
				if (createFolder(bin_dir_out + (current_dir.empty() ? "" : "\\") + current_dir))
					throw 0;
				_chdir(cpath);
				FILE * fout = nullptr;
				auto test_test = bin_dir_out + (current_dir.empty() ? "" : "\\") + current_dir + "\\" + file_name;
				errno_t err2 = fopen_s(&fout, (bin_dir_out + (current_dir.empty() ? "" : "\\") + current_dir + "\\" + file_name).c_str(), "wb");
				if (!fout)
					throw 0;
				mrb_dump_irep_binary(state, irep, 0, fout);
				fclose(fout);
			}

			mrb_irep_free(state, irep);
			mrb_close(state);

		} while (::FindNextFile(hFind, &fd));
		::FindClose(hFind);
	}
}

int main(int argc, char ** argv)
{
	// u all "D:\Downloads\NieRAutomata™ .bin\NieRAutomata™ .bin" "D:\Downloads\NieRAutomata™ .bin\NieRAutomata™ .bin.txt" 1
	// u us "D:\Downloads\NieRAutomata™ .bin\NieRAutomata™ .bin" "D:\Downloads\NieRAutomata™ .bin\NieRAutomata™ .bin.txt" 1

	// u all "F:\___SOURCES___\nier\NieRAutomata™ .bin" "F:\___SOURCES___\nier\NieRAutomata™ .bin.txt" 1
	// u us "F:\___SOURCES___\nier\NieRAutomata™ .bin" "F:\___SOURCES___\nier\NieRAutomata™ .bin.txt2" 1

	// u all "F:\___SOURCES___\nier\NieRAutomata™ .bin2" "F:\___SOURCES___\nier\NieRAutomata™ .bin_test.txt" 1

	// p all "F:\___SOURCES___\nier\NieRAutomata™ .bin" "F:\___SOURCES___\nier\NieRAutomata™ .bin.txt" "F:\___SOURCES___\nier\NieRAutomata™ .bin2"
	// p us "F:\___SOURCES___\nier\NieRAutomata™ .bin" "F:\___SOURCES___\nier\NieRAutomata™ .bin.txt2" "F:\___SOURCES___\nier\NieRAutomata™ .bin3"

	if (argc < 6)
	{
		std::cout << "Unpack: nier_auto_bin.exe u <lang> <bins_dir> <text_dir> <with_names>" << std::endl;
		std::cout << "Pack: nier_auto_bin.exe p <lang> <bins_dir> <text_dir> <out_bins_dir>" << std::endl;
		std::cout << "    lang: all / jp / us / fr / it / de / us / ko / cn" << std::endl;
		std::cout << "    with_names: 0 - without names / 1 - with names" << std::endl;
		return 0;
	}

	std::string mode(argv[1]);
	std::string lang(argv[2]);
	std::string dir_in(argv[3]);
	std::string dir_out(argv[4]);
	std::string with_id_s(mode == "u" ? argv[5] : "");
	std::string bin_dir_out(mode == "p" ? argv[5] : "");

	bool with_id = (with_id_s == "1");

	int lang_index = -1;
	if (lang == "jp") lang_index = 0;
	else if (lang == "us") lang_index = 1;
	else if (lang == "fr") lang_index = 2;
	else if (lang == "it") lang_index = 3;
	else if (lang == "de") lang_index = 4;
	else if (lang == "es") lang_index = 5;
	else if (lang == "ko") lang_index = 6;
	else if (lang == "cn") lang_index = 7;
	else if (lang != "all") throw 0;

	std::vector<std::pair<std::string, State>> files;

	if (mode == "p")
	{
		std::cout << "Loading texts..." << std::endl;
		std::vector<std::string> langs = { "jp", "us", "fr", "it", "de", "es", "ko", "cn" };
		if (lang_index != -1)
		{
			for (int i = 0; i < (int)langs.size(); i++)
				if (lang_index != i)
					langs[i] == "";
		}
		std::ifstream f_ids(dir_out + "\\bin_ids.txt", std::ios::in | std::ios::binary);
		std::ifstream f_jp(dir_out + "\\bin_jp.txt", std::ios::in | std::ios::binary);
		std::ifstream f_us(dir_out + "\\bin_us.txt", std::ios::in | std::ios::binary);
		std::ifstream f_fr(dir_out + "\\bin_fr.txt", std::ios::in | std::ios::binary);
		std::ifstream f_it(dir_out + "\\bin_it.txt", std::ios::in | std::ios::binary);
		std::ifstream f_de(dir_out + "\\bin_de.txt", std::ios::in | std::ios::binary);
		std::ifstream f_es(dir_out + "\\bin_es.txt", std::ios::in | std::ios::binary);
		std::ifstream f_ko(dir_out + "\\bin_ko.txt", std::ios::in | std::ios::binary);
		std::ifstream f_cn(dir_out + "\\bin_cn.txt", std::ios::in | std::ios::binary);
		while (!f_ids.eof())
		{
			std::string line_ids; getline(f_ids, line_ids);
			std::string line_jp; if (lang == "all" || lang == "jp") getline(f_jp, line_jp);
			std::string line_us; if (lang == "all" || lang == "us") getline(f_us, line_us);
			std::string line_fr; if (lang == "all" || lang == "fr") getline(f_fr, line_fr);
			std::string line_it; if (lang == "all" || lang == "it") getline(f_it, line_it);
			std::string line_de; if (lang == "all" || lang == "de") getline(f_de, line_de);
			std::string line_es; if (lang == "all" || lang == "es") getline(f_es, line_es);
			std::string line_ko; if (lang == "all" || lang == "ko") getline(f_ko, line_ko);
			std::string line_cn; if (lang == "all" || lang == "cn") getline(f_cn, line_cn);
			
			if (line_ids[0] == '[' && line_ids[1] == '[')
			{
				files.push_back(std::make_pair(line_ids.substr(2, line_ids.size() - 4), State()));
				if ((lang == "all" || lang == "jp") && line_ids != line_jp) throw 0;
				if ((lang == "all" || lang == "us") && line_ids != line_us) throw 0;
				if ((lang == "all" || lang == "fr") && line_ids != line_fr) throw 0;
				if ((lang == "all" || lang == "it") && line_ids != line_it) throw 0;
				if ((lang == "all" || lang == "de") && line_ids != line_de) throw 0;
				if ((lang == "all" || lang == "es") && line_ids != line_es) throw 0;
				if ((lang == "all" || lang == "ko") && line_ids != line_ko) throw 0;
				if ((lang == "all" || lang == "cn") && line_ids != line_cn) throw 0;
				continue;
			}

			files.back().second.texts.push_back(std::pair<std::string, std::vector<std::string>> (line_ids, {
				(lang == "all" || lang == "jp" ? line_jp : ""),
				(lang == "all" || lang == "us" ? line_us : ""),
				(lang == "all" || lang == "fr" ? line_fr : ""),
				(lang == "all" || lang == "it" ? line_it : ""),
				(lang == "all" || lang == "de" ? line_de : ""),
				(lang == "all" || lang == "es" ? line_es : ""),
				(lang == "all" || lang == "ko" ? line_ko : ""),
				(lang == "all" || lang == "cn" ? line_cn : "")
			}));
		}
	}

	if (mode == "u")
		std::cout << "Loading files..." << std::endl;

	if (mode == "p")
		std::cout << "Repacking files..." << std::endl;

	find_files(mode, lang_index, files, dir_in, bin_dir_out, "");

	if (mode == "u")
	{
		std::cout << "Saving texts..." << std::endl;

		if (lang == "all")
		{
			std::ofstream f_ids(dir_out + "\\bin_ids.txt", std::ios::out | std::ios::binary);
			std::ofstream f_jp(dir_out + "\\bin_jp.txt", std::ios::out | std::ios::binary);
			std::ofstream f_us(dir_out + "\\bin_us.txt", std::ios::out | std::ios::binary);
			std::ofstream f_fr(dir_out + "\\bin_fr.txt", std::ios::out | std::ios::binary);
			std::ofstream f_it(dir_out + "\\bin_it.txt", std::ios::out | std::ios::binary);
			std::ofstream f_de(dir_out + "\\bin_de.txt", std::ios::out | std::ios::binary);
			std::ofstream f_es(dir_out + "\\bin_es.txt", std::ios::out | std::ios::binary);
			std::ofstream f_ko(dir_out + "\\bin_ko.txt", std::ios::out | std::ios::binary);
			std::ofstream f_cn(dir_out + "\\bin_cn.txt", std::ios::out | std::ios::binary);

			for (int i = 0; i < (int)files.size(); i++)
			{
				write_line(f_ids, "[[" + files[i].first + "]]");
				write_line(f_jp, "[[" + files[i].first + "]]");
				write_line(f_us, "[[" + files[i].first + "]]");
				write_line(f_fr, "[[" + files[i].first + "]]");
				write_line(f_it, "[[" + files[i].first + "]]");
				write_line(f_de, "[[" + files[i].first + "]]");
				write_line(f_es, "[[" + files[i].first + "]]");
				write_line(f_ko, "[[" + files[i].first + "]]");
				write_line(f_cn, "[[" + files[i].first + "]]");
				for (int j = 0; j < (int)files[i].second.texts.size(); j++)
				{
					if (
						files[i].second.texts[j].second[0].empty()
						&& files[i].second.texts[j].second[1].empty()
						&& files[i].second.texts[j].second[2].empty()
						&& files[i].second.texts[j].second[3].empty()
						&& files[i].second.texts[j].second[4].empty()
						&& files[i].second.texts[j].second[5].empty()
						&& files[i].second.texts[j].second[6].empty()
						&& files[i].second.texts[j].second[7].empty()
					)
					{
						continue;
					}

					write_line(f_ids, files[i].second.texts[j].first);
					write_line(f_jp, (with_id ? files[i].second.texts[j].first.substr(files[i].second.texts[j].first.find_last_of('_') + 1) + ": " : "") + files[i].second.texts[j].second[0]);
					write_line(f_us, (with_id ? files[i].second.texts[j].first.substr(files[i].second.texts[j].first.find_last_of('_') + 1) + ": " : "") + files[i].second.texts[j].second[1]);
					write_line(f_fr, (with_id ? files[i].second.texts[j].first.substr(files[i].second.texts[j].first.find_last_of('_') + 1) + ": " : "") + files[i].second.texts[j].second[2]);
					write_line(f_it, (with_id ? files[i].second.texts[j].first.substr(files[i].second.texts[j].first.find_last_of('_') + 1) + ": " : "") + files[i].second.texts[j].second[3]);
					write_line(f_de, (with_id ? files[i].second.texts[j].first.substr(files[i].second.texts[j].first.find_last_of('_') + 1) + ": " : "") + files[i].second.texts[j].second[4]);
					write_line(f_es, (with_id ? files[i].second.texts[j].first.substr(files[i].second.texts[j].first.find_last_of('_') + 1) + ": " : "") + files[i].second.texts[j].second[5]);
					write_line(f_ko, (with_id ? files[i].second.texts[j].first.substr(files[i].second.texts[j].first.find_last_of('_') + 1) + ": " : "") + files[i].second.texts[j].second[6]);
					write_line(f_cn, (with_id ? files[i].second.texts[j].first.substr(files[i].second.texts[j].first.find_last_of('_') + 1) + ": " : "") + files[i].second.texts[j].second[7]);
				}
			}
		}
		else
		{
			std::ofstream f_ids(dir_out + "\\bin_ids.txt", std::ios::out | std::ios::binary);
			std::ofstream f_lang(dir_out + "\\bin_" + lang + ".txt", std::ios::out | std::ios::binary);

			for (int i = 0; i < (int)files.size(); i++)
			{
				write_line(f_ids, "[[" + files[i].first + "]]");
				write_line(f_lang, "[[" + files[i].first + "]]");
				for (int j = 0; j < (int)files[i].second.texts.size(); j++)
				{
					if (files[i].second.texts[j].second[lang_index].empty())
						continue;

					write_line(f_ids, files[i].second.texts[j].first);
					write_line(f_lang, (with_id ? files[i].second.texts[j].first.substr(files[i].second.texts[j].first.find_last_of('_') + 1) + ": " : "") + files[i].second.texts[j].second[lang_index]);
				}
			}
		}
	}

	return 0;
}