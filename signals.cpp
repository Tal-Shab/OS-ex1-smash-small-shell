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
  // TODO: Add your implementation
}

