/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 * @author Armeena Sajjad
 * @author Ajay Uppal 
 */

#include <interrupts.hpp>

std::tuple<std::string, std::string, int> simulate_trace(std::vector<std::string> trace_file, int time, const std::vector<std::string>& vectors, const std::vector<int>& delays, const std::vector<external_file>& external_files, PCB current, std::vector<PCB> wait_queue);

static inline void append_system_status(std::string& system_status, int current_time, const std::string& current_trace_line, const PCB& current, const std::vector<PCB>& wait_queue) {
    system_status += "time: " + std::to_string(current_time) + "; current trace: " + current_trace_line + "\n";
    system_status += print_PCB(current, wait_queue) + "\n";
}

std::tuple<std::string, std::string, int> simulate_trace(std::vector<std::string> trace_file, int time, const std::vector<std::string>& vectors, const std::vector<int>& delays, const std::vector<external_file>& external_files, PCB current, std::vector<PCB> wait_queue) {
    std::string execution = "";
    std::string system_status = "";
    int current_time = time;
    static unsigned int NEXT_PID = 1;
    static std::mt19937 rng(12345);
    std::uniform_int_distribution<int> step_ms(1,10);

    for(size_t i = 0; i < trace_file.size(); i++) {
        const std::string trace_line = trace_file[i];
        auto [activity, duration_intr, program_name] = parse_trace(trace_line);

        if(activity == "CPU") {
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", CPU Burst\n";
            current_time += duration_intr;

        } else if(activity == "SYSCALL") {
            auto [intr, t] = intr_boilerplate(current_time, 10, 10, vectors);
            execution += intr;
            current_time = t;
            execution += std::to_string(current_time) + ", " + std::to_string(delays.at(duration_intr)) + ", SYSCALL ISR\n";
            current_time += delays.at(duration_intr);
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time++;

        } else if(activity == "END_IO") {
            auto [intr, t] = intr_boilerplate(current_time, 10, 10, vectors);
            execution += intr;
            current_time = t;
            execution += std::to_string(current_time) + ", " + std::to_string(delays.at(duration_intr)) + ", ENDIO ISR\n";
            current_time += delays.at(duration_intr);
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time++;

        } else if(activity == "FORK") {
            auto [intr, t] = intr_boilerplate(current_time, 2, 10, vectors);
            execution += intr;
            current_time = t;

            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", cloning the PCB\n";
            current_time += duration_intr;

            PCB child(NEXT_PID++, current.PID, current.program_name, current.size, -1);
            allocate_memory(&child);

            wait_queue.push_back(current);
            PCB running = child;

            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time++;

            append_system_status(system_status, current_time, trace_line, running, wait_queue);

            std::vector<std::string> child_trace;
            bool collecting = false;
            size_t parent_index = i;

            for(size_t j = i + 1; j < trace_file.size(); j++) {
                auto [a2, d2, p2] = parse_trace(trace_file[j]);
                if(!collecting && a2 == "IF_CHILD") { collecting = true; continue; }
                if(collecting && a2 == "IF_PARENT") { parent_index = j; break; }
                if(!collecting && a2 == "ENDIF") { collecting = true; continue; }
                if(collecting) child_trace.push_back(trace_file[j]);
            }

            i = parent_index;

            auto [child_exec, child_sys, tchild] = simulate_trace(child_trace, current_time, vectors, delays, external_files, running, wait_queue);
            execution += child_exec;
            system_status += child_sys;
            current_time = tchild;

            if (!wait_queue.empty()) wait_queue.pop_back();
            free_memory(&running);

        } else if(activity == "IF_PARENT" || activity == "IF_CHILD" || activity == "ENDIF") {
            continue;

        } else if(activity == "EXEC") {
            auto [intr, t] = intr_boilerplate(current_time, 3, 10, vectors);
            execution += intr;
            current_time = t;

            int prog_size = (int)get_size(program_name, external_files);
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", Program is " + std::to_string(prog_size) + " Mb large\n";
            current_time += duration_intr;

            int load_time = prog_size * 15;
            execution += std::to_string(current_time) + ", " + std::to_string(load_time) + ", loading program into memory\n";
            current_time += load_time;

            int ms_mark = step_ms(rng);
            execution += std::to_string(current_time) + ", " + std::to_string(ms_mark) + ", marking partition as occupied\n";
            current_time += ms_mark;

            int ms_pcb = step_ms(rng);
            free_memory(&current);
            current.program_name = program_name;
            current.size = (unsigned int)prog_size;
            allocate_memory(&current);
            execution += std::to_string(current_time) + ", " + std::to_string(ms_pcb) + ", updating PCB\n";
            current_time += ms_pcb;

            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time++;

            append_system_status(system_status, current_time, trace_line, current, wait_queue);

            std::ifstream exec_trace_file(program_name + ".txt");
            std::vector<std::string> exec_traces;
            std::string x;
            while(std::getline(exec_trace_file, x)) if(!x.empty()) exec_traces.push_back(x);

            auto [child_exec, child_sys, t2] = simulate_trace(exec_traces, current_time, vectors, delays, external_files, current, wait_queue);
            execution += child_exec;
            system_status += child_sys;
            current_time = t2;
            break;
        }
    }

    return {execution, system_status, current_time};
}

int main(int argc, char** argv) {
    auto [vectors, delays, external_files] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    PCB current(0, -1, "init", 1, -1);
    allocate_memory(&current);

    std::vector<PCB> wait_queue;
    std::vector<std::string> trace_file;
    std::string trace;
    while(std::getline(input_file, trace)) if (!trace.empty()) trace_file.push_back(trace);

    auto [execution, system_status, _] = simulate_trace(trace_file, 0, vectors, delays, external_files, current, wait_queue);
    write_output(execution, "execution.txt");
    write_output(system_status, "system_status.txt");
    return 0;
}
