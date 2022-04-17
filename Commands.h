#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <ctime>
#include <map>
#include <unistd.h>
#include <string.h>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define DEFAULT_PROMPT "smash"
#define ERROR_PREFIX "smash error: "

typedef unsigned int job_id;
enum JOB_STATUS {UNFINISHED, STOPPED};

using std::string;

class SmashError : public std::exception {
    const string msg;
public:
    const char* what() const noexcept override;
    explicit SmashError(const string& msg);
};

class SmashCmdError : public SmashError {
public:
    explicit SmashCmdError(const string& msg) : SmashError(msg) {}
};

class SmashSysFailure : public SmashError {
public:
    explicit SmashSysFailure(const string& msg) : SmashError(msg) {}
};

class Command {
protected:
  char* args[COMMAND_MAX_ARGS];
  int n_args;
  bool is_BG;
  char cmd_line[COMMAND_ARGS_MAX_LENGTH];
  char raw_cmd_line[COMMAND_ARGS_MAX_LENGTH];
public:
  explicit Command(const char* cmd_line);
  virtual ~Command();
  virtual void execute() = 0;
  //virtual void prepare();
  //virtual void cleanup();
  // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
 public:
  explicit BuiltInCommand(const char* cmd_line);
  virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
 public:
  explicit ExternalCommand(const char* cmd_line);
  virtual ~ExternalCommand() {}
  void execute() override;
};

class PipeCommand : public Command {
  // TODO: Add your data members
 public:
  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand() {}
  void execute() override;
};

class RedirectionCommand : public Command {
 // TODO: Add your data members
 public:
  explicit RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand() {}
  void execute() override;
  //void prepare() override;
  //void cleanup() override;
};

class ChangeDirCommand : public BuiltInCommand {
// TODO: Add your data members
    string dest_dir;
public:
  ChangeDirCommand(const char* cmd_line, string prev_dir);
  virtual ~ChangeDirCommand() {}
  void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
 public:
  GetCurrDirCommand(const char* cmd_line);
  virtual ~GetCurrDirCommand() {}
  void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
 public:
  ShowPidCommand(const char* cmd_line);
  virtual ~ShowPidCommand() {}
  void execute() override;
};

class JobsList;
class QuitCommand : public BuiltInCommand {
// TODO: Add your data members
public:
  QuitCommand(const char* cmd_line, JobsList* jobs);
  virtual ~QuitCommand() {}
  void execute() override;
};




class JobsList {
public:
  class JobEntry {
   // TODO: Add your data members
   pid_t pid;
   job_id id;
   time_t timestamp;
   JOB_STATUS status;
  };

 // TODO: Add your data members
 public:
  JobsList();
  ~JobsList();
  void addJob(Command* cmd, bool isStopped = false);
  void printJobsList();
  void killAllJobs();
  void removeFinishedJobs();
  JobEntry * getJobById(int jobId);
  void removeJobById(int jobId);
  JobEntry * getLastJob(int* lastJobId);
  JobEntry *getLastStoppedJob(int *jobId);
  // TODO: Add extra methods or modify exisitng ones as needed
private:
    std::map<job_id, JobEntry> jobs_list;
};

class JobsCommand : public BuiltInCommand {
 // TODO: Add your data members
 public:
  JobsCommand(const char* cmd_line, JobsList* jobs);
  virtual ~JobsCommand() {}
  void execute() override;
};

class KillCommand : public BuiltInCommand {
 // TODO: Add your data members
 public:
  KillCommand(const char* cmd_line, JobsList* jobs);
  virtual ~KillCommand() {}
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
 // TODO: Add your data members
 public:
  ForegroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~ForegroundCommand() {}
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
 // TODO: Add your data members
 public:
  BackgroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~BackgroundCommand() {}
  void execute() override;
};

class TailCommand : public BuiltInCommand {
 public:
  TailCommand(const char* cmd_line);
  virtual ~TailCommand() {}
  void execute() override;
};

class TouchCommand : public BuiltInCommand {
 public:
  TouchCommand(const char* cmd_line);
  virtual ~TouchCommand() {}
  void execute() override;
};

class ChPromptCommand : public BuiltInCommand {
public:
    string new_prompt;
    ChPromptCommand(const char* cmd_line);
    virtual ~ChPromptCommand() {}
    void execute() override;
};

class SmallShell {
 private:
  SmallShell();
  string prompt = DEFAULT_PROMPT;
  pid_t pid;
  string curr_dir;
  string prev_dir;
  pid_t curr_fg;

  std::map<pid_t, job_id> procToJobId;
 public:
  Command *CreateCommand(const char* cmd_line);
  SmallShell(SmallShell const&)      = delete; // disable copy ctor
  void operator=(SmallShell const&)  = delete; // disable = operator
  static SmallShell& getInstance() // make SmallShell singleton
  {
    static SmallShell instance; // Guaranteed to be destroyed.
    // Instantiated on first use.
    return instance;
  }
  ~SmallShell();
  void executeCommand(const char* cmd_line);
  void setPromptLine(const string new_prmp_line);
  string getCurrDir();
  void setCurrDir();
  void printPromptLine() const ;
  void printSmashId() const ;
  void setCurrFg(pid_t fg_pid=0);
  // TODO: add extra methods as needed
};

#endif //SMASH_COMMAND_H_
