#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
  cout << MSG_PREFIX << "got ctrl-Z" << endl;
	SmallShell& smash = SmallShell::getInstance();
    smash.jobs_list.stopCurrFGJob();
}

void ctrlCHandler(int sig_num) {
    cout << MSG_PREFIX << "got ctrl-C" << endl;
    SmallShell& smash = SmallShell::getInstance();
    smash.jobs_list.killCurrFGJob();
}

void alarmHandler(int sig_num) {
    cout << MSG_PREFIX << "got an alarm" << endl;

    SmallShell& smash = SmallShell::getInstance();

    pid_t pid_to_kill = smash.time_out_manager.RemoveTimedOut();
    while ( pid_to_kill != DEFAULT_PROCESS_ID) {
        smash.jobs_list.removeFinishedJobs();
        if ( 0 == kill(pid_to_kill, SIGKILL ) ) {
            cout << MSG_PREFIX << *(smash.jobs_list.getCmdForPID(pid_to_kill)) << " timed out!" << endl;
        }
        pid_to_kill = smash.time_out_manager.RemoveTimedOut();
    }
    smash.time_out_manager.SetNextAlarm();
}

