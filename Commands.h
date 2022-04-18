#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <ctime>
#include <map>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <memory>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define DEFAULT_PROMPT "smash"
#define ERROR_PREFIX "smash error: "
#define DEFAULT_JOB_ID (0)
#define DEFAULT_PROCESS_ID (0)

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
  char cmd_line[COMMAND_ARGS_MAX_LENGTH];
  char raw_cmd_line[COMMAND_ARGS_MAX_LENGTH];
public:
  bool is_BG;
  explicit Command(const char* cmd_line);
  virtual ~Command();
  virtual pid_t execute() = 0; //will return true is job finished. else false.
  friend std::ostream& operator<<(std::ostream& os, const Command& cm);
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
  pid_t execute() override;
};

class PipeCommand : public Command {
  // TODO: Add your data members
 public:
  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand() {}
  pid_t execute() override;
};

class RedirectionCommand : public Command {
 // TODO: Add your data members
 public:
  explicit RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand() {}
  pid_t execute() override;
  //void prepare() override;
  //void cleanup() override;
};

class ChangeDirCommand : public BuiltInCommand {
// TODO: Add your data members
    string dest_dir;
public:
  ChangeDirCommand(const char* cmd_line, string prev_dir);
  virtual ~ChangeDirCommand() {}
  pid_t execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
 public:
  GetCurrDirCommand(const char* cmd_line);
  virtual ~GetCurrDirCommand() {}
  pid_t execute() override;
};

class ShowPidCommand : public BuiltInCommand {
 public:
  ShowPidCommand(const char* cmd_line);
  virtual ~ShowPidCommand() {}
  pid_t execute() override;
};

class JobsList;
class QuitCommand : public BuiltInCommand {
// TODO: Add your data members
public:
  QuitCommand(const char* cmd_line, JobsList* jobs);
  virtual ~QuitCommand() {}
  pid_t execute() override;
};




class JobsList {
public:
    struct JobEntry_t {
        job_id id;
        time_t timestamp;
        pid_t pid;
        Command* cmd;
        JOB_STATUS status;

        JobEntry_t(job_id id, time_t timestamp, pid_t pid, Command* cmd, JOB_STATUS status) :
            id(id), timestamp(timestamp), pid(pid), cmd(cmd), status(status) {}
        double calcDiffTime();
    };
typedef std::shared_ptr<JobEntry_t> JobEntry;
public:
    JobsList() = default;
    ~JobsList() = default;
    void addJob(pid_t pid, Command* cmd, bool isStopped = false, job_id jobId = DEFAULT_JOB_ID);
    void printJobsList();
    JobEntry getJobByJobId(job_id jobId);
    JobEntry getJobByProcessId(pid_t pid);
    void removeJobByJobId(job_id job_id_to_remove);
    void removeJobByProcessId(pid_t pid_to_remove);
    JobEntry getLastJob(job_id* lastJobId);
    JobEntry getLastStoppedJob(job_id* jobId);
    void removeFinishedJobs(); 
    void killAllJobs(); //TODO
    void updateCurrFGJob(pid_t pid, Command* cmd, job_id jobId = DEFAULT_JOB_ID);
    void resetCurrFGJob();
    ///function that mobed the curr FG_JOB inside the job list TODO
private:
    std::map<pid_t, job_id> proc_to_job_id;
    std::map<job_id, JobEntry > jobs_list;
    JobEntry curr_FG_job;
};

class JobsCommand : public BuiltInCommand {
    JobsList* jobs_list;
public:
    JobsCommand(const char* cmd_line, JobsList* jobs_list);
    virtual ~JobsCommand() {}
    pid_t execute() override;
};

class KillCommand : public BuiltInCommand {
 // TODO: Add your data members
 public:
  KillCommand(const char* cmd_line, JobsList* jobs);
  virtual ~KillCommand() {}
  pid_t execute() override;
};

class ForegroundCommand : public BuiltInCommand {
 // TODO: Add your data members
 public:
  ForegroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~ForegroundCommand() {}
  pid_t execute() override;
};

class BackgroundCommand : public BuiltInCommand {
 // TODO: Add your data members
 public:
  BackgroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~BackgroundCommand() {}
  pid_t execute() override;
};

class TailCommand : public BuiltInCommand {
 public:
  TailCommand(const char* cmd_line);
  virtual ~TailCommand() {}
  pid_t execute() override;
};

class TouchCommand : public BuiltInCommand {
 public:
  TouchCommand(const char* cmd_line);
  virtual ~TouchCommand() {}
  pid_t execute() override;
};

class ChPromptCommand : public BuiltInCommand {
public:
    string new_prompt;
    ChPromptCommand(const char* cmd_line);
    virtual ~ChPromptCommand() {}
    pid_t execute() override;
};

class SmallShell {
 private:
  SmallShell();
  string prompt = DEFAULT_PROMPT;
  pid_t pid;
  string curr_dir;
  string prev_dir;
  //pid_t curr_fg_pid;
  //job_id curr_fg_job_id;

 public:
  JobsList jobs_list;
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
};

#endif //SMASH_COMMAND_H_
