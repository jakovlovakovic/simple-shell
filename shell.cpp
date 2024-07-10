#include <iostream>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <string>
#include <sstream>
#include <vector>
#include <csignal>
#include <algorithm>
#include <sys/types.h>
#include <termios.h>
#include <sys/wait.h>
#include <filesystem>

using namespace std;

// neke podatkovne strukture koje koristim
struct termios postavke_shella;

int proces_counter = 0;

struct sigaction before;

struct Proces {
    pid_t pid;
    string ime_procesa;

    bool operator==(const Proces& other) const {
        return pid == other.pid;
    }
};

vector<Proces> vector_aktivnih;


// implementacija cd funkcije
void cd_func(const string& path_string) {
    const char* path_arr = path_string.c_str();
    if(chdir(path_arr) != 0) {
        perror("cd failed");
    }
}

// ispisi trenutni path
void print_path() {
    char buffer[2048]; // alociraj buffer
    char* ptr;
    ptr = getcwd(buffer, sizeof(buffer)); // passaj buffer u getcwd
    cout << "\033[0;32m" << ptr << " > " << "\033[0m";
}

// naredba exit
void exit_func() {
    bool flag = false;
    vector<Proces> temp;
    for(const auto &value : vector_aktivnih) {
        if (kill(value.pid, SIGKILL) == -1) {
            flag = true;
        }
        else {
            temp.push_back(value);
        }
    }
    for(const auto &value : temp) {
        vector_aktivnih.erase(remove(vector_aktivnih.begin(), vector_aktivnih.end(), value), vector_aktivnih.end());
    }
    if(flag) cout << "ERROR: Some processes failed to shut down." << endl;
    else { exit(0); }
}

// kreiranje procesa u pozadini
void create_process(vector<string>& tokens) {
    pid_t pid = fork();
    if (pid != 0) {
        Proces novi_proces = {
            pid, "process" + to_string(proces_counter++)
            };
        cout << "Process with PID: " << novi_proces.pid << " created successfuly!" << endl;
        vector_aktivnih.push_back(novi_proces);
    }
    if (pid == 0) {
        cout << endl;
        //sigaction(SIGINT, &before, nullptr);
        setpgid(getpid(), getpid());
        vector<const char *> cstr_vec;
        for (const auto &str : tokens) {
            cstr_vec.push_back(str.c_str());
        }
        cstr_vec.push_back(NULL);
        const char **naredba = cstr_vec.data();
        char* const* naredba_nonconst = const_cast<char* const*>(naredba);
        execvp(naredba_nonconst[0], naredba_nonconst);
		perror("Program failed to load!");
		exit(1);
    }
}

// kreiranje procesa u prednjem planu
void create_process_prednji_plan(vector<string>& tokens) {
    tcgetattr(STDIN_FILENO, &postavke_shella);
    pid_t pid = fork();
    // dijete
    if (pid == 0) {
        cout << "PID = " + to_string(getpid()) + " started in foreground!" << endl;
        //sigaction(SIGINT, &before, NULL);
		setpgid(getpid(), getpid()); //stvori novu grupu za ovaj proces
		tcsetpgrp(STDIN_FILENO, getpgid(pid)); //dodijeli terminal
        vector<const char *> cstr_vec;
        for (const auto &str : tokens) {
            cstr_vec.push_back(str.c_str());
        }
        cstr_vec.push_back(NULL);
        const char **naredba = cstr_vec.data();
        char* const* naredba_nonconst = const_cast<char* const*>(naredba);
        execvp(naredba_nonconst[0], naredba_nonconst);
		perror("Program failed to load!");
		exit(1);
    }
    // roditelj
    else { 
        int status;
        waitpid(pid, &status, 0); // ceka da se djecji proces zavrsi
        tcsetpgrp(STDIN_FILENO, getpgid(0)); // vraca roditeljski proces u prednji plan
        tcsetattr(STDIN_FILENO, 0, &postavke_shella); // obnova shella
    }
}

// ispis aktivnih procesa
void ps_func() {
    if(vector_aktivnih.empty()) {
        cout << "No active processes." << endl;
    }
    for(const auto &value : vector_aktivnih) {
        cout << value.pid << ": " << value.ime_procesa << endl;
    }
}

// funkcija kill (kill <PID> <SIGNAL_NUM>)
void kill_func(string argumenti) {
    istringstream stringstream(argumenti);
    pid_t pid;
    int signal_value;
    if (!(stringstream >> pid >> signal_value)) {
        cout << "ERROR: Wrong arguments for the kill command." << endl;
    }
    bool flag = true;
    for(const auto& value : vector_aktivnih) {
        if(value.pid == pid) {
            if (kill(pid, signal_value) == -1) {
                cout << "Something went wrong with shutting down the process with PID = " << pid << endl; 
            }
            flag = false;
        }
    }
    if(flag) cout << "Kill exception: NOT ALLOWED PID." << endl;
}

// ovo se poziva u shellu kad dodje sigint
void obradi_dogadjaj(int sig) {
    cout << endl;
}

// obradi dijete
void obradi_dijete(int id) { 
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if(pid > 0) {
            if (kill(pid, 0) == -1) {
                Proces value_to_remove = {
                    pid, "garbage_line"};
                vector_aktivnih.erase(remove(vector_aktivnih.begin(), vector_aktivnih.end(), value_to_remove), vector_aktivnih.end());
            }
        }
    }
}

int main(void) {
    string input;
    string naredba;
    string argumenti;
    struct sigaction signal_action;
    signal_action.sa_handler = obradi_dogadjaj;
	sigemptyset(&signal_action.sa_mask);
	signal_action.sa_flags = 0;
	sigaction(SIGINT, &signal_action, &before);
	signal_action.sa_handler = obradi_dijete;
	sigaction(SIGCHLD, &signal_action, NULL);
    //zbog tcsetpgrp
	signal_action.sa_handler = SIG_IGN;
	sigaction(SIGTTOU, &signal_action, NULL);

    do {
        // ispis putanje na pocetku ljuske
        print_path();

        // input i parsanje inputa, dobivanje naredba i argumenata
        getline(cin, input);
        cin.clear();
        if(!input.empty() && input[input.length() - 1] == '\n') {
            input.erase(input.length() - 1);
        }
        istringstream inputstream(input);
        getline(inputstream, naredba, ' ');
        getline(inputstream, argumenti);

        // ako se zove cd funkcija
        if(naredba == "cd") {
            cd_func(argumenti);
        }
        
        // ako se zove exit
        if(naredba == "exit") {
            exit_func();
        }

        // poziv ps naredbe
        if(naredba == "ps") {
            ps_func();
        }

        // poziv kill funkcije
        if(naredba == "kill") {
            kill_func(argumenti);
        }

         // pokretanje procesa parsanje
        if(naredba != "cd" && naredba != "exit" && naredba != "ps" && naredba != "kill") {
            istringstream new_input_string(input);
            vector<string> tokens;
            string token;
            while(getline(new_input_string, token, ' ')) {
                tokens.push_back(token);
            }
            if(!tokens.empty()) {
                token = tokens.front();
                size_t pos = token.find('/');
                if(pos != string::npos) {
                    // "./"
                    string firstPart = token.substr(0, pos + 1);
                    // ostatak
                    string secondPart = token.substr(pos + 1);
                    // provjera
                    if(firstPart == "./") {
                        if(!secondPart.empty()) {
                            filesystem::path currentPath = filesystem::current_path();
                            filesystem::path fullPath = currentPath / secondPart;
                            if(filesystem::exists(fullPath)) {
                                // ako postoji program
                                if(tokens[tokens.size() - 1] == "&") {
                                    tokens[0] = fullPath.string();
                                    tokens.pop_back();
                                    create_process(tokens);
                                }
                                else {
                                    tokens[0] = fullPath.string();
                                    create_process_prednji_plan(tokens);
                                }
                            }
                            else {
                                string basePath = "/usr/bin";
                                string newPath = basePath + "/" + secondPart;
                                if(filesystem::exists(newPath)) {
                                    if(tokens[tokens.size() - 1] == "&") {
                                        tokens[0] = newPath;
                                        tokens.pop_back();
                                        create_process(tokens);
                                    }
                                    else {
                                        tokens[0] = newPath;
                                        create_process_prednji_plan(tokens);
                                    }
                                }
                                else {
                                    cout << "ERROR: The file doesn't exist." << endl;
                                }
                            }
                        }
                        else {
                            cout << "ERROR: command doesn't exist." << endl;
                        }
                    }
                    else {
                        cout << "ERROR: command doesn't exist." << endl;
                    }
                }
                else {
                    cout << "ERROR: command doesn't exist." << endl;
                }
            }
        }
    } while(true);

    return 0;
}