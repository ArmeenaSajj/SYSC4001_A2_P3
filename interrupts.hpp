#ifndef INTERRUPTS_HPP_
#define INTERRUPTS_HPP_

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <random>
#include <utility>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdio.h>
#include <tuple>

#define ADDR_BASE 0
#define VECTOR_SIZE 2

struct memory_partition_t {
    const unsigned int partition_number;
    const unsigned int size;
    std::string code;
    memory_partition_t(unsigned int _pn, unsigned int _s, std::string _c):
        partition_number(_pn), size(_s), code(_c) {}
};

inline memory_partition_t memory[] = {
    memory_partition_t(1, 40, "empty"),
    memory_partition_t(2, 25, "empty"),
    memory_partition_t(3, 15, "empty"),
    memory_partition_t(4, 10, "empty"),
    memory_partition_t(5, 8,  "empty"),
    memory_partition_t(6, 2,  "empty")
};

struct PCB{
    unsigned int PID;
    int PPID;
    std::string program_name;
    unsigned int size;
    int partition_number;
    PCB(unsigned int _pid, int _ppid, std::string _pn, unsigned int _size, int _part_num):
        PID(_pid), PPID(_ppid), program_name(_pn), size(_size), partition_number(_part_num) {}
};

struct external_file{
    std::string program_name;
    unsigned int size;
};

inline bool allocate_memory(PCB* current) {
    for(int i = 5; i >= 0; i--) {
        if(memory[i].size >= current->size && memory[i].code == "empty") {
            current->partition_number = memory[i].partition_number;
            memory[i].code = current->program_name;
            return true;
        }
    }
    return false;
}

inline void free_memory(PCB* process) {
    if (process->partition_number >= 1 && process->partition_number <= 6)
        memory[process->partition_number - 1].code = "empty";
    process->partition_number = -1;
}

inline std::vector<std::string> split_delim(std::string input, std::string delim) {
    std::vector<std::string> tokens;
    std::size_t pos = 0;
    std::string token;
    while ((pos = input.find(delim)) != std::string::npos) {
        token = input.substr(0, pos);
        tokens.push_back(token);
        input.erase(0, pos + delim.length());
    }
    tokens.push_back(input);
    return tokens;
}

inline std::tuple<std::vector<std::string>, std::vector<int>, std::vector<external_file>>
parse_args(int argc, char** argv) {
    if(argc != 5) exit(1);

    std::ifstream f;
    f.open(argv[1]); if(!f.is_open()) exit(1); f.close();
    f.open(argv[2]); if(!f.is_open()) exit(1);

    std::vector<std::string> vectors;
    std::string line;
    while(std::getline(f, line)) if(!line.empty()) vectors.push_back(line);
    f.close();

    f.open(argv[3]); if(!f.is_open()) exit(1);
    std::vector<int> delays;
    while(std::getline(f, line)) if(!line.empty()) delays.push_back(std::stoi(line));
    f.close();

    f.open(argv[4]); if(!f.is_open()) exit(1);
    std::vector<external_file> external_files;
    while(std::getline(f, line)) {
        if(line.empty()) continue;
        auto parts = split_delim(line, ",");
        if(parts.size() < 2) continue;
        external_file e;
        e.program_name = parts[0];
        while(!e.program_name.empty() && e.program_name.back()==' ') e.program_name.pop_back();
        std::string sz = parts[1];
        while(!sz.empty() && sz.front()==' ') sz.erase(sz.begin());
        e.size = (unsigned int)std::stoi(sz);
        external_files.push_back(e);
    }
    f.close();

    return {vectors, delays, external_files};
}

inline std::tuple<std::string, int, std::string> parse_trace(std::string trace) {
    auto comma = trace.find(',');
    if (comma == std::string::npos) {
        std::string activity = trace;
        while (!activity.empty() && activity.back()==' ') activity.pop_back();
        while (!activity.empty() && activity.front()==' ') activity.erase(activity.begin());
        return {activity, 0, "null"};
    }
    auto parts = split_delim(trace, ",");
    std::string activity = parts[0];
    while (!activity.empty() && activity.back()==' ') activity.pop_back();
    while (!activity.empty() && activity.front()==' ') activity.erase(activity.begin());
    int duration_intr = std::stoi(parts[1]);
    std::string extern_file = "null";
    auto exec = split_delim(activity, " ");
    if(exec[0] == "EXEC") {
        extern_file = exec.size() > 1 ? exec[1] : "null";
        activity = "EXEC";
    }
    return {activity, duration_intr, extern_file};
}

inline std::pair<std::string, int> intr_boilerplate(int current_time, int intr_num, int context_save_time, std::vector<std::string> vectors) {
    std::string execution = "";
    execution += std::to_string(current_time) + ", 1, switch to kernel mode\n";
    current_time++;
    execution += std::to_string(current_time) + ", " + std::to_string(context_save_time) + ", context saved\n";
    current_time += context_save_time;
    execution += std::to_string(current_time) + ", 1, find vector " + std::to_string(intr_num) + "\n";
    current_time++;
    execution += std::to_string(current_time) + ", 1, load address " + vectors.at(intr_num) + " into the PC\n";
    current_time++;
    return std::make_pair(execution, current_time);
}

inline void write_output(std::string execution, const char* filename) {
    std::ofstream out(filename);
    out << execution;
    out.close();
}

inline std::string print_PCB(PCB current, std::vector<PCB> _PCB) {
    std::stringstream b;
    b << "+" << std::setfill('-') << std::setw(55) << "+" << "\n";
    b << "|" << std::setfill(' ') << std::setw(4) << "PID" << std::setw(2) << "|"
      << std::setw(12) << "program name" << std::setw(2) << "|"
      << std::setw(16) << "partition number" << std::setw(2) << "|"
      << std::setw(5) << "size" << std::setw(2) << "|"
      << std::setw(8) << "state" << std::setw(2) << "|\n";
    b << "+" << std::setfill('-') << std::setw(55) << "+" << "\n";
    b << "|" << std::setfill(' ') << std::setw(4) << current.PID << std::setw(2) << "|"
      << std::setw(12) << current.program_name << std::setw(2) << "|"
      << std::setw(16) << current.partition_number << std::setw(2) << "|"
      << std::setw(5) << current.size << std::setw(2) << "|"
      << std::setw(8) << "running" << std::setw(2) << "|\n";
    for (auto& p : _PCB) {
        b << "|" << std::setw(4) << p.PID << std::setw(2) << "|"
          << std::setw(12) << p.program_name << std::setw(2) << "|"
          << std::setw(16) << p.partition_number << std::setw(2) << "|"
          << std::setw(5) << p.size << std::setw(2) << "|"
          << std::setw(8) << "waiting" << std::setw(2) << "|\n";
    }
    b << "+" << std::setfill('-') << std::setw(55) << "+" << "\n";
    return b.str();
}

inline unsigned int get_size(std::string name, const std::vector<external_file>& external_files) {
    for (const auto& f : external_files) if (f.program_name == name) return f.size;
    return 0;
}

#endif
