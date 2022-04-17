#include "Commands.h"
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include <time.h>
#include <utime.h>
#include <climits>

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

void ChPromptCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  smash.setPromptLine(this->new_prompt);
}

ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute(){
    SmallShell& smash = SmallShell::getInstance();
    smash.printSmashId();
}

GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute(){
    SmallShell& smash = SmallShell::getInstance();
    cout << smash.getCurrDir() << endl;
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

void ChangeDirCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();

    // currently ignoring no empty dir argument, should check on Piazza once an instructor answers.
    // bash for example changes into the /home dir when no args are passed, so we might want to do that?
    if (dest_dir.empty()) {
        return;
    }

    if (chdir(dest_dir.c_str()) == 0) {
        smash.setCurrDir();
    } else {
        throw SmashSysFailure("chdir failed");
    }
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
Command * SmallShell::CreateCommand(const char* cmd_line) {
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    if (firstWord == "chprompt") {
        return new ChPromptCommand(cmd_line);
    }

    if (firstWord == "showpid") {
        return new ShowPidCommand(cmd_line);
    }

    if (firstWord == "pwd") {
        return new GetCurrDirCommand(cmd_line);
    }

    if (firstWord == "cd") {
        return new ChangeDirCommand(cmd_line, this->prev_dir);
    }
    /*
    else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
    }
    else if ...
    .....
    else {
    return new ExternalCommand(cmd_line);
    }
    */


    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    try {
        Command *cmd = CreateCommand(cmd_line);
        if (cmd != nullptr)
            cmd->execute();
    } catch (SmashCmdError& err) {
        cerr << err.what() << endl;
    } catch (SmashSysFailure& err) {
        perror(err.what());
    }
  // Please note that you must fork smash process for some commands (e.g., external commands....)
}

///////////////////SmallShell end//////////////////////////

SmashError::SmashError(const string& msg) : msg(string(ERROR_PREFIX) + msg) {}

const char* SmashError::what() const noexcept {
    return msg.c_str();
}


