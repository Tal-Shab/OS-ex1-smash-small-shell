#include "Commands.h"
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include <utime.h>
#include <climits>
#include <cstdlib>
#include <cassert>

using namespace std;

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

const std::string WHITESPACE = " \n\r\t\f\v";

string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = nullptr;
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

////////////////////Command Class start//////////////////////////////////////

Command::Command(const char* cmd_line){
  is_BG = _isBackgroundComamnd(cmd_line);
  strcpy(this->cmd_line,cmd_line);
  strcpy(this->raw_cmd_line, cmd_line);

  if(is_BG)
    _removeBackgroundSign(this->cmd_line);
  n_args = _parseCommandLine(this->cmd_line, args);
}

Command::~Command(){
  for(int i = 0; i < n_args; i++){
    if(args[i] != nullptr)
      free(args[i]);
  }
}

ostream& operator<<(ostream& os, const Command& cm) {
    os << cm.raw_cmd_line;
    return os;
}

////////////////////Command Class end//////////////////////////////////////

///////////////////Built in commands start//////////////////////////
BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command(cmd_line) {
    this->is_BG = false;
}

ChPromptCommand::ChPromptCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {
  if (n_args == 1)
    new_prompt = DEFAULT_PROMPT;
  else
    new_prompt = args[1];
}

pid_t ChPromptCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  smash.setPromptLine(this->new_prompt);
  return DEFAULT_PROCESS_ID;
}

ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

pid_t ShowPidCommand::execute(){
    SmallShell& smash = SmallShell::getInstance();
    smash.printSmashId();
    return DEFAULT_PROCESS_ID;
}

GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

pid_t GetCurrDirCommand::execute(){
    SmallShell& smash = SmallShell::getInstance();
    cout << smash.getCurrDir() << endl;
    return DEFAULT_PROCESS_ID;
}

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, string const prev_dir) : BuiltInCommand(cmd_line), dest_dir() {
    if (n_args > 2) {
        throw SmashCmdError("cd: too many arguments");
    }

    if (n_args == 2) {
        this->dest_dir = args[1];
    }

    if (dest_dir == "-") {
        if (prev_dir.empty()) {
            throw SmashCmdError("cd: OLDPWD not set");
        } else {
            dest_dir = prev_dir;
        }
    }
}

pid_t ChangeDirCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();

    // currently ignoring no empty dir argument, should check on Piazza once an instructor answers.
    // bash for example changes into the /home dir when no args are passed, so we might want to do that?
    if (dest_dir.empty()) {
        return true;
    }

    if (chdir(dest_dir.c_str()) == 0) {
        smash.setCurrDir();
    } else {
        throw SmashSysFailure("chdir failed");
    }
    return DEFAULT_PROCESS_ID;
}

JobsCommand::JobsCommand(const char* cmd_line, JobsList* jobs_list) : BuiltInCommand(cmd_line), jobs_list(jobs_list) {}
pid_t JobsCommand::execute() {
    this->jobs_list->printJobsList();
    return DEFAULT_PROCESS_ID;
}

bool _isnumber(char* str) {
    for (; *str != '\0'; str++) {
        if (isdigit(*str) == false) {
            return false;
        }
    }

    return true;
}

ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs_list(jobs) {
    if (this->n_args > 2) {
        throw SmashCmdError("fg: invalid arguments");
    }

    if (this->n_args == 2) {
        if (_isnumber(this->args[1])) {
            this->dest_jid = (job_id)stoul(this->args[1]);
        } else {
            throw SmashCmdError("fg: invalid arguments");
        }
    } else {
        this->dest_jid = DEFAULT_JOB_ID;
    }
}

pid_t ForegroundCommand::execute() {
    this->jobs_list->removeFinishedJobs();

    JobsList::JobEntry job_entry;

    if (this->dest_jid == DEFAULT_JOB_ID) {
        job_entry = this->jobs_list->getLastJob(nullptr);
        if (job_entry == nullptr) {
            throw SmashCmdError("fg: jobs list is empty");
        }
    } else {
        job_entry = this->jobs_list->getJobByJobId(this->dest_jid);
        if (job_entry == nullptr) {
            throw SmashCmdError("fg: job-id " + to_string(this->dest_jid) + " does not exist");
        }
    }

    cout << *(job_entry->cmd) << " : " << job_entry->pid << endl;
    if ( kill(job_entry->pid, SIGCONT) == -1 ) {
        throw SmashSysFailure("kill failed");
    }

    this->jobs_list->updateCurrFGJob(job_entry->pid, job_entry->cmd, job_entry->id);
    this->jobs_list->removeJobByJobId(job_entry->id);

    waitpid(job_entry->pid, nullptr, WUNTRACED);

    this->jobs_list->resetCurrFGJob();
    return DEFAULT_PROCESS_ID;
}

BackgroundCommand::BackgroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs_list(jobs) {
    if (this->n_args > 2) {
        throw SmashCmdError("bg: invalid arguments");
    }

    if (this->n_args == 2) {
        if (_isnumber(this->args[1])) {
            this->dest_jid = (job_id)stoul(this->args[1]);
        } else {
            throw SmashCmdError("bg: invalid arguments");
        }
    } else {
        this->dest_jid = DEFAULT_JOB_ID;
    }
}

pid_t BackgroundCommand::execute() {
    this->jobs_list->removeFinishedJobs();

    JobsList::JobEntry job_entry;

    if (this->dest_jid == DEFAULT_JOB_ID) {
        job_entry = this->jobs_list->getLastStoppedJob(nullptr);
        if (job_entry == nullptr) {
            throw SmashCmdError("bg: there is no stopped jobs to resume");
        }
    } else {
        job_entry = this->jobs_list->getJobByJobId(this->dest_jid);
        if (job_entry == nullptr) {
            throw SmashCmdError("bg: job-id " + to_string(this->dest_jid) + " does not exist");
        } else if (job_entry->status != STOPPED) {
            throw SmashCmdError("bg: job-id " + to_string(this->dest_jid) + " is already running in the background");
        }
    }

    cout << *(job_entry->cmd) << " : " << job_entry->pid << endl;
    if ( kill(job_entry->pid, SIGCONT) == -1 ) {
        throw SmashSysFailure("kill failed");
    }
    job_entry->status = UNFINISHED;

    return DEFAULT_PROCESS_ID;
}

QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {
    kill = ( n_args >= 2 && args[1] == string("kill") );
}

pid_t QuitCommand::execute() {
    if (this->kill) {
        this->jobs->killAllJobs();
    }

    exit(0);
}

KillCommand::KillCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs){
    if (this->n_args != 3 || args[1][0] != '-' || !_isnumber(this->args[1]+1) || !_isnumber(this->args[2])) {
        throw SmashCmdError("kill: invalid arguments");
    }

    this->sig_num = (int)stoul(this->args[1]+1);
    this->dest_jid = (job_id)stoul(this->args[2]);
}

pid_t KillCommand::execute() {
//    this->jobs->removeFinishedJobs();
    JobsList::JobEntry job = this->jobs->getJobByJobId(this->dest_jid);
    if (job == nullptr) {
        throw SmashCmdError("kill: job-id " + to_string(this->dest_jid) + " does not exist");
    }

    if ( kill(job->pid, this->sig_num) == -1 ) {
        throw SmashSysFailure("kill failed");
    }

    cout << "signal number " << this->sig_num << " was sent to pid " << job->pid << endl;
    this->jobs->removeFinishedJobs();

    return DEFAULT_PROCESS_ID;
}


///////////////////Built in commands end//////////////////////////



///////////////////SmallShell start//////////////////////////

SmallShell::SmallShell(){
    this->pid = getpid();
    this->curr_dir = string();
    this->setCurrDir();
}

SmallShell::~SmallShell(){
// TODO: add your implementation
}

void SmallShell::setPromptLine(const string new_prmp_line){
  this->prompt = new_prmp_line;
}

void SmallShell::printPromptLine() const{
   cout << this->prompt << "> ";
}

void SmallShell::printSmashId() const{
   cout << "smash pid is " << this->pid << endl ;
}

string SmallShell::getCurrDir() {
    return this->curr_dir;
}

void SmallShell::setCurrDir() {
    char buff[PATH_MAX];
    getcwd(buff, PATH_MAX);

    this->prev_dir = curr_dir;
    this->curr_dir = string(buff);
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
CommandPtr SmallShell::CreateCommand(const char* cmd_line) {
    string cmd_s = _trim((cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" &\n"));

    if (firstWord.empty()) {
        return nullptr;
    }

    if (cmd_s.find_first_of('>') != string::npos) {
        return make_shared<RedirectionCommand>(cmd_line);
    } else if (cmd_s.find_first_of('|') != string::npos) {
        return make_shared<PipeCommand>(cmd_line);
    } else if (firstWord == "chprompt") {
        return make_shared<ChPromptCommand>(cmd_line);
    } else if (firstWord == "showpid") {
        return make_shared<ShowPidCommand>(cmd_line);
    } else if (firstWord == "pwd") {
        return make_shared<GetCurrDirCommand>(cmd_line);
    } else if (firstWord == "cd") {
        return make_shared<ChangeDirCommand>(cmd_line, this->prev_dir);
    } else if (firstWord == "jobs") {
        return make_shared<JobsCommand>(cmd_line, &(this->jobs_list));
    } else if (firstWord == "fg") {
        return make_shared<ForegroundCommand>(cmd_line, &(this->jobs_list));
    } else if (firstWord == "bg") {
        return make_shared<BackgroundCommand>(cmd_line, &(this->jobs_list));
    } else if (firstWord == "quit") {
        return make_shared<QuitCommand>(cmd_line, &(this->jobs_list));
    } else if (firstWord == "kill") {
        return make_shared<KillCommand>(cmd_line, &(this->jobs_list));
    } else if (firstWord == "tail") {
        return make_shared<TailCommand>(cmd_line);
    } else if (firstWord == "touch") {
        return make_shared<TouchCommand>(cmd_line);
    }

    return make_shared<ExternalCommand>(cmd_line);
}

void SmallShell::executeCommand(const char *cmd_line) {
    this->jobs_list.removeFinishedJobs();

    try {
        CommandPtr cmd = CreateCommand(cmd_line);
        if (cmd != nullptr) {
            pid_t fork_pid = cmd->execute();
            if (fork_pid == DEFAULT_PROCESS_ID) {
//                delete (cmd);
                return; //it was FG, no need to add to list
            }
            /*not Built in command*/
            if(cmd->is_BG) {
                this->jobs_list.addJob(fork_pid, cmd);
            } else {
                this->jobs_list.updateCurrFGJob(fork_pid, cmd);
                waitpid(fork_pid, nullptr, WUNTRACED);
                this->jobs_list.resetCurrFGJob();
//                delete (cmd);
            }
        }
    }
    catch (SmashCmdError& err) {
        cerr << err.what() << endl;
    } catch (SmashSysFailure& err) {
        perror(err.what());
    }  
  // Please note that you must fork smash process for some commands (e.g., external commands....)
}


SmashError::SmashError(const string& msg) : msg(string(ERROR_PREFIX) + msg) {}

const char* SmashError::what() const noexcept {
    return msg.c_str();
}

///////////////////SmallShell end///////////////////////////



//////////////////////Job Entry start///////////////////////
double JobsList::JobEntry_t::calcDiffTime() {
    time_t curr_timestamp = time(nullptr);
    if (curr_timestamp == ((time_t) -1))
        throw SmashSysFailure("time failed");

    return ( difftime(curr_timestamp, this->timestamp) );
}
//////////////////////Job Entry end///////////////////////



//////////////////Job List start////////////////////////////

JobsList::JobEntry JobsList::getJobByJobId(job_id jobId) {
    auto map_elem = this->jobs_list.find(jobId);
    if (map_elem == this->jobs_list.end()) return nullptr;
    return (map_elem->second);
}

JobsList::JobEntry JobsList::getJobByProcessId(pid_t pid) {
    auto map_elem = this->proc_to_job_id.find(pid);
    if (map_elem == this->proc_to_job_id.end()) return nullptr;
    job_id jid = map_elem->second;
    return (getJobByJobId(jid));
}

JobsList::JobEntry JobsList::getLastJob(job_id* lastJobId) {
    if( this->jobs_list.empty()) {
        if (lastJobId != nullptr)
            *lastJobId = 0;
        return nullptr;
    }

    if (lastJobId != nullptr)
        *lastJobId = (this->jobs_list.rbegin())->first;
    return ( (this->jobs_list.rbegin())->second );
}

JobsList::JobEntry JobsList::getLastStoppedJob(job_id* jobId) {
    for(auto iter = this->jobs_list.rbegin(); iter != this->jobs_list.rend(); ++iter) {
        if(iter->second->status == STOPPED){
            if (jobId != nullptr)
                *jobId = iter->first;
            return (iter->second);
        }
    }

    if (jobId != nullptr)
        *jobId = 0;
    return nullptr; //not an error, just no stopped jobs at jobs list
}

void JobsList::removeJobByProcessId(pid_t pid_to_remove) {
    auto map_element = this->proc_to_job_id.find(pid_to_remove);
    if(map_element == this->proc_to_job_id.end()) return;
    
    job_id jid_to_remove = map_element->second;
    this->jobs_list.erase(jid_to_remove);
    this->proc_to_job_id.erase(pid_to_remove);
}

void JobsList::removeJobByJobId(job_id job_id_to_remove) {
    JobEntry job_entry = getJobByJobId(job_id_to_remove);
    if (job_entry == nullptr) return;
    pid_t process_to_remove = job_entry->pid;
    this->jobs_list.erase(job_id_to_remove);
    this->proc_to_job_id.erase(process_to_remove);
}


void JobsList::removeFinishedJobs() {
    pid_t done_pid;
    while ((done_pid = waitpid(-1, nullptr, WNOHANG)) > 0) {
        this->removeJobByProcessId(done_pid);
    }
}

void JobsList::addJob(pid_t pid, CommandPtr cmd, bool isStopped, job_id jobId) {
    this->removeFinishedJobs(); //cleanup all done jobs before inserting a new one
    if( waitpid(pid , nullptr, WNOHANG) == pid ) return;
    if( jobId == DEFAULT_JOB_ID) {
        this->getLastJob(&jobId);
        jobId += 1;
    }
    JOB_STATUS status = isStopped ? STOPPED : UNFINISHED;
    time_t timestamp = time(nullptr);
    if (timestamp == ((time_t) -1)) {
        throw SmashSysFailure("time failed");
    }
    JobEntry entry = make_shared<JobEntry_t>(jobId, timestamp, pid, cmd, status);
    this->proc_to_job_id.insert({entry->pid, entry->id});
    this->jobs_list.insert({entry->id, entry});
}


void JobsList::printJobsList() {
    this->removeFinishedJobs();
    for (auto iter = this->jobs_list.begin() ; iter != this->jobs_list.end() ; iter++) {
        JobEntry job_entry = iter->second;
        cout << "[" << job_entry->id << "] ";
        cout << *(job_entry->cmd) << " : ";
        cout << job_entry->pid << " ";
        cout << job_entry->calcDiffTime() << " secs";
        if(job_entry->status == STOPPED)
            cout << " (stopped)";
        cout << endl;
    }  
}

void JobsList::updateCurrFGJob(pid_t pid, CommandPtr cmd, job_id jobId) {
//    assert(this->curr_FG_job == nullptr);
    JobEntry entry = make_shared<JobEntry_t>(jobId, 0, pid, cmd, UNFINISHED);
    this->curr_FG_job = entry;
}

void JobsList::resetCurrFGJob() {
    this->curr_FG_job = nullptr;
}

void JobsList::killAllJobs() {
    this->removeFinishedJobs();

    cout << MSG_PREFIX << "sending SIGKILL signal to " << this->jobs_list.size() << " jobs:" << endl;
    for (auto iter = this->jobs_list.begin(), next_it = iter; iter != this->jobs_list.end(); iter = next_it) {
        ++next_it;
        JobEntry job = iter->second;
        cout << job->pid << ": " << *(job->cmd) << endl;
        if ( kill(job->pid, SIGKILL) == -1 ) {
            throw SmashSysFailure("kill failed");
        }
        this->jobs_list.erase(iter);
        this->proc_to_job_id.erase(job->pid);
    }
}

void JobsList::killCurrFGJob() {
    if(this->curr_FG_job == nullptr)
        return;
    if(-1 == kill(this->curr_FG_job->pid, SIGKILL) ) {
        throw SmashSysFailure("kill failed");
    }
    waitpid(this->curr_FG_job->pid, nullptr, 0);
    cout << MSG_PREFIX << "process " << this->curr_FG_job->pid << " was killed" << endl;
    resetCurrFGJob();
}

void JobsList::stopCurrFGJob() {
    JobEntry job = this->curr_FG_job;
    if(job == nullptr)
        return;
    if(-1 == kill(job->pid, SIGSTOP) ) {
        throw SmashSysFailure("kill failed");
    }
    addJob(job->pid, job->cmd, true, job->id);
    cout << MSG_PREFIX << "process " << job->pid << " was stopped" << endl;
    resetCurrFGJob();
}
//////////////////Jobs List end////////////////////////////


///////////////////External Commands start//////////////////////////

ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line){}

pid_t ExternalCommand::execute() {
    pid_t fork_pid = fork();
    if (fork_pid < 0) {
        throw SmashSysFailure("fork failed");
    }

    if (fork_pid == 0) {
        // child process
        setpgrp();
        char* argv[] = {(char*)"/bin/bash", (char*)"-c", this->cmd_line, nullptr};
        execv(argv[0], argv);
        /*
         * Panic mode
         * https://stackoverflow.com/questions/3703013/what-can-cause-exec-to-fail-what-happens-next
         */
        perror("execv failed");
        exit(EXIT_FAILURE);
    } else {
        // parent process (smash)
        return (fork_pid);
    }
}



///////////////////External Commands end//////////////////////////



///////////////////Special Command start//////////////////////////

RedirectionCommand::RedirectionCommand(const char* cmd_line):Command(cmd_line) {
    string str_cmd_line = string(cmd_line);
    size_t index_of_sub = str_cmd_line.find('>');
    if( str_cmd_line[index_of_sub+1] == '>') { // ">>" meaning append
        this->flag |= O_APPEND;
    }
    else { // only ">"
        this->flag |=  O_TRUNC;
    }
    size_t last_index = str_cmd_line.find_last_of('>');
    this->output_file = _trim(str_cmd_line.substr(last_index+1));

    SmallShell& smash = SmallShell::getInstance();
    this->cmd = smash.CreateCommand(_trim(str_cmd_line.substr(0,index_of_sub)).c_str());

    this->is_BG = false;
    this->cmd->is_BG = false;
}

pid_t RedirectionCommand::execute() {
    pid_t pid = fork();
    if(pid == 0) { 
        setpgrp();
        try {
            if (-1 == close(STDOUT_FD)) {
                throw SmashSysFailure("close failed");
            }
            if (-1 == open(this->output_file.c_str(), this->flag, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) {
                throw SmashSysFailure("open failed");
            }

            this->cmd->execute();
            if (-1 == close(STDOUT_FD)) {
                throw SmashSysFailure("close failed");
            }
        }
        catch (SmashCmdError& err) {
            cerr << err.what() << endl;
            exit(1);
        } catch (SmashSysFailure& err) {
            perror(err.what());
            exit(1);
        }

        exit(0);
    } else if (pid < 0) {
        throw SmashSysFailure("fork failed");
    } else {
        waitpid(pid, nullptr , 0);
    }
    return DEFAULT_PROCESS_ID;
}

PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line), write_fd(STDOUT_FD) {
    string str_cmd_line = string(cmd_line);
    size_t index_of_pipe = str_cmd_line.find('|');
    size_t index_after_pipe = index_of_pipe + 1;
    if( str_cmd_line[index_of_pipe+1] == '&') {
        this->write_fd = STDERR_FD;
        index_after_pipe += 1;
    }

    string src_cmd_line = _trim(str_cmd_line.substr(0, index_of_pipe));
    string dest_cmd_line = _trim(str_cmd_line.substr(index_after_pipe));

    SmallShell& smash = SmallShell::getInstance();
    this->cmd_src = smash.CreateCommand(src_cmd_line.c_str());
    this->cmd_dest = smash.CreateCommand(dest_cmd_line.c_str());

    this->is_BG = false;
    this->cmd_src->is_BG = false;
    this->cmd_dest->is_BG = false;
}

void close_pipe(int fd[2]) {
    if ( -1 == close(fd[0]) || -1 == close(fd[1]) ) {
        throw SmashSysFailure("close failed");
    }
}

pid_t PipeCommand::execute() {
    int fd[2];
    if ( -1 == pipe(fd) ) {
        throw SmashSysFailure("pipe failed");
    }

    pid_t src_pid;
    pid_t dest_pid;

    src_pid = fork();
    if (src_pid == 0) {
        setpgrp();
        try {
            if ( -1 == dup2(fd[STDOUT_FD], this->write_fd) ) {
                throw SmashSysFailure("dup2 failed");
            }
            close_pipe(fd);
            pid_t src_child = this->cmd_src->execute();
            if (src_child != DEFAULT_PROCESS_ID) {
                waitpid(src_child, nullptr, 0);
            }
        }
        catch (SmashCmdError& err) {
            cerr << err.what() << endl;
            exit(1);
        } catch (SmashSysFailure& err) {
            perror(err.what());
            exit(1);
        }
        exit(0);
    } else if (src_pid < 0) {
        throw SmashSysFailure("fork failed");
    }

    dest_pid = fork();
    if (dest_pid == 0) {
        setpgrp();
        try {
            if ( -1 == dup2(fd[STDIN_FD], STDIN_FD) ) {
                throw SmashSysFailure("dup2 failed");
            }

            close_pipe(fd);
            pid_t dest_child = this->cmd_dest->execute();
            if (dest_child != DEFAULT_PROCESS_ID) {
                waitpid(dest_child, nullptr, 0);
            }
        }
        catch (SmashCmdError& err) {
            cerr << err.what() << endl;
            exit(1);
        } catch (SmashSysFailure& err) {
            perror(err.what());
            exit(1);
        }
        exit(0);
    } else if (dest_pid < 0) {
        throw SmashSysFailure("fork failed");
        // TODO check if first fork succeeded and kill it if necessary (only if TA says we should do this)
    }

    close_pipe(fd);

    bool src_fin = false, dest_fin = false;
    while (!src_fin || !dest_fin) {
        src_fin = src_fin || (waitpid(src_pid, nullptr, WNOHANG) == src_pid);
        dest_fin = dest_fin || (waitpid(dest_pid, nullptr, WNOHANG) == dest_pid);
    }

    return DEFAULT_PROCESS_ID;
}

TailCommand::TailCommand(const char *cmd_line) : BuiltInCommand(cmd_line), line_count(DEFAULT_TAIL_COUNT) {
    if (this->n_args > 3 || this->n_args <= 1) {
        throw SmashCmdError("tail: invalid arguments");
    }

    if (n_args == 3) {
        if (!_isnumber(this->args[1]+1)) {
            throw SmashCmdError("tail: invalid arguments");
        }

        this->line_count = (int)stoul(this->args[1]+1);
        this->filename = this->args[2];
    }

    if (n_args == 2) {
        this->filename = this->args[1];
    }
}

off_t findLastLinesPos(int fd, int line_count) {
    char buff[BUFFER_SIZE];
    ssize_t rbytes;
    off_t pos = lseek(fd, 0, SEEK_END);
    if (pos == -1) {
        throw SmashSysFailure("lseek failed");
    }

    size_t bytes_to_read;
    // We want to see line_count newline chars ('\n') regardless of the last char in the file -
    // we don't care whether the last char is a newline, because we need to get line_count lines
    pos -= 1;
    while (pos > 0) {
        if (pos - BUFFER_SIZE >= 0) {
            bytes_to_read = BUFFER_SIZE;
            pos = lseek(fd, pos - BUFFER_SIZE, SEEK_SET);
        } else {
            bytes_to_read = pos;
            pos = lseek(fd, 0, SEEK_SET);
        }
        if (pos == -1) {
            throw SmashSysFailure("lseek failed");
        }
        // we assume that read reads the whole BUFFER_SIZE bytes, if not - how should we handle that? try again? loop? ignore? TODO ask TA
        rbytes = read(fd, buff, bytes_to_read);
        if (rbytes == -1) {
            throw SmashSysFailure("read failed");
        }
        for (int i = ((int)rbytes - 1); i >= 0; i--) {
            if (buff[i] == '\n') {
                line_count -= 1;
                if (line_count == 0) {
                    return pos + i + 1; // pos + i is the exact position of the newline in the file, so we want the next char
                }
            }
        }
    }

    return 0;
}

pid_t TailCommand::execute() {
    int fd = open(this->filename, O_RDONLY);
    if (-1 == fd) {
        throw SmashSysFailure("open failed");
    }

    off_t pos = findLastLinesPos(fd, this->line_count);
    if (-1 == lseek(fd, pos, SEEK_SET)) {
        throw SmashSysFailure("lseek failed");
    }
    char buff[BUFFER_SIZE];
    ssize_t rbytes = -1;
    while (rbytes != 0) {
        rbytes = read(fd, buff, BUFFER_SIZE); // TODO what if rbytes < BUFFER_SIZE ?
        if (rbytes == -1) {
            throw SmashSysFailure("read failed");
        }

        if (-1 == write(STDOUT_FD, buff, rbytes)) { // TODO what if we wrote less than rbytes?
            throw SmashSysFailure("write failed");
        }
    }

    if (-1 == close(fd)) {
        throw SmashSysFailure("close failed");
    }

    return DEFAULT_PROCESS_ID;
}

TouchCommand::TouchCommand(const char *cmd_line) : BuiltInCommand(cmd_line), timestamp() {
    if (n_args != 3) {
        throw SmashCmdError("touch: invalid arguments");
    }

    this->filename = this->args[1];
    string time_str(this->args[2]);

    int date_parts[6];
    size_t pos = 0;
    for (int i=0; i < 6; i++) {
        pos = time_str.find_first_of(':');
        date_parts[i] = (int)stoul(time_str.substr(0,pos));
        time_str = time_str.substr(pos+1);
    }

    tm time_obj{ .tm_sec = date_parts[0], .tm_min = date_parts[1], .tm_hour = date_parts[2],
                .tm_mday = date_parts[3], .tm_mon = date_parts[4] - 1, .tm_year = (date_parts[5] - 1900),
                .tm_wday = 0, .tm_yday = 0, .tm_isdst = -1, .tm_gmtoff = 0 };

    this->timestamp = mktime(&time_obj);
//    assert(this->timestamp != -1);
}

pid_t TouchCommand::execute() {
    utimbuf times{ .actime = this->timestamp, .modtime = this->timestamp };
    if ( -1 == utime(this->filename, &times) ) {
        throw SmashSysFailure("utime failed");
    }

    return DEFAULT_PROCESS_ID;
}
