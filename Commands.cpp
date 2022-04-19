#include "Commands.h"
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include <utime.h>
#include <climits>
#include <cstdlib>

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

// TODO: Add your implementation for classes in Commands.h


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
    pid_t dest_pid;

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

    waitpid(job_entry->pid, nullptr, 0);

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
    pid_t dest_pid;

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
   cout << this->prompt << "> ";  //////TODO - remember maybe no space
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

    if (firstWord == "chprompt") {
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
    } else {
        return make_shared<ExternalCommand>(cmd_line);
    }

    return nullptr;
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
                waitpid(fork_pid, nullptr, 0);
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
    JobEntry entry = make_shared<JobEntry_t>(jobId, 0, pid, cmd, UNFINISHED);
    ///TODO assert this->curr_FG_job==nullptr 
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

//////////////////Job List end////////////////////////////


///////////////////External Commands start//////////////////////////

ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line){}

pid_t ExternalCommand::execute() {
    pid_t fork_pid = fork();
    if (fork_pid == -1) {
        throw SmashSysFailure("fork failed");
    }

    if (fork_pid == 0) {
        // child process
        setpgrp();
        char *argv[] = {"/bin/bash", "-c", this->cmd_line, nullptr};
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
