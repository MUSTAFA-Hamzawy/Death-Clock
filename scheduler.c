#include "Helpers.c"
/******************************* Global Variables *******************************/
int MsgQueueId,
    SharedMemoryId,
    CountProcesses,
    LastArrivalTime,
    ReceivedCounter;
int *ShmAddr;
bool LastProcess;


void clearResources(int);

int main(int argc, char *argv[])
{
    initClk();
    signal(SIGINT, clearResources); // upon termination release the clock resources.

    printf("Scheduler is starting\n");

    MsgQueueId = InitMsgQueue('q');
    ShmAddr = InitShm('m', &SharedMemoryId);

    int chosenAlgorithm = atoi(argv[1]),
        quantum = atoi(argv[2]),
        NumOfProcesses = atoi(argv[3]);
    LastArrivalTime = atoi(argv[4]);

    CountProcesses = NumOfProcesses;

    // start the algorithm
    switch (chosenAlgorithm)
    {
    case 1:
        HPF();
        break;
    case 2:
        SRTN();
        break;
    case 3:
        RoundRobin(quantum);
        break;
    }

    destroyClk(true);
}

//------------------------------------------------------------------------------------------------------------------------//
/*
Function explaintion:-
    This function is the implementation of the Highest Priority First Algorithm,
    it selects the highest priority process of the ready queue and start it until it is finished,
    takes the next highest priority and so on ....
*/
void HPF()
{
    printf("Start HPF Algo\n");

    Process CurrentProcess;
    Node *q = NULL;

    int flag = 0,
        MaxArrivalTime = -1,
        last = -1,
        timeConsume = 0,
        ProcessesHashTime[LastArrivalTime];

    ReceivedCounter = CountProcesses;
    CurrentProcess.LastProcess = false;
    CurrentProcess.Priority = 11;
    *ShmAddr = -1;

    while (!(*ShmAddr == -1 && CountProcesses == 0 && CurrentProcess.LastProcess))
    {
        // function to check if there is new process or not
        if (ReceivedCounter > 0)
        {
            ReadyProcessExist(MsgQueueId, &q, &flag, 1, &MaxArrivalTime, ProcessesHashTime);
            ReceivedCounter -= flag;
        }

        last = getClk();

        if (*ShmAddr == 0 && (getClk() - MaxArrivalTime - timeConsume == CurrentProcess.RemainingTime))
        {
            FinishProcess(&CurrentProcess, ShmAddr, MaxArrivalTime);
            timeConsume += CurrentProcess.RunTime;
            *ShmAddr = -1;
            CountProcesses--;
            if (CountProcesses == 0)
            {
                break;
            }
        }

        if (CountProcesses > 0 && *ShmAddr == -1 && flag >= 1)
        {
            // pop a new process to run it
            CurrentProcess = pop(&q);
            *ShmAddr = CurrentProcess.RunTime;
            StartProcess(&CurrentProcess, MaxArrivalTime);
            flag--;
        }

        // wait until the clock changes
        while (last == getClk())
            ;
        if (last != getClk())
        {
            last = getClk();
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------//
/*
Function explaintion:-
    This function is the implementation of the Round Robin Algorithm,
    Each process is assigned a fixed time slot in a cyclic way.
    Algorithm Parameters:-
        1-quantum:- The time quantum
*/
void RoundRobin(int quantum)
{
    printf("Round Robin Algorithm Starts\n");
    Process CurrentProcess;
    Node *q = NULL;

    int CurrentQuantum = 0,
        index = 0,
        flag = 0,
        x = 0,
        MaxArrivalTime = -1,
        var = 0,
        last = -1,
        ProcessesHashTime[LastArrivalTime];

    *ShmAddr = -1;
    ReceivedCounter = CountProcesses;
    CurrentProcess.RemainingTime = -2;       // to prevent it enter the block of code that finishes the process in the start

    while (CountProcesses > 0)
    {
        if (*ShmAddr != -1)
        {
            CurrentQuantum = getClk() - MaxArrivalTime - var;
        }

        // Receive the arrived processes
        if (ReceivedCounter > 0)
        {
            ReadyProcessExist(MsgQueueId, &q, &flag, 0, &MaxArrivalTime, ProcessesHashTime);
            ReceivedCounter -= flag;
        }

        // check whether the current process is finished or not
        if (*ShmAddr == 0 || CurrentProcess.RemainingTime == CurrentQuantum)
        {
            FinishProcess(&CurrentProcess, ShmAddr, MaxArrivalTime);
            var += CurrentQuantum;
            CountProcesses--;
            CurrentQuantum = 0; // reset
            if (CountProcesses == 0)
            {
                break;
            }
            else
            {

                // switch to another process, or get the first arrived process to work
                CurrentProcess = pop(&q);

                // update the remaining time in the memory
                *ShmAddr = CurrentProcess.RemainingTime;
                x = CurrentProcess.RemainingTime;

                // check if this is the first time to start or conitue
                if (CurrentProcess.RemainingTime == CurrentProcess.RunTime)
                    StartProcess(&CurrentProcess, MaxArrivalTime);
                else
                    ContinueProcess(CurrentProcess, MaxArrivalTime);
            }
        }

        // if the quantum is over ==> switch to another process if any
        if ((CurrentQuantum == quantum || (*ShmAddr <= 0 && CurrentQuantum == quantum) || *ShmAddr == -1) && CurrentProcess.RemainingTime != *ShmAddr)
        {
            if (*ShmAddr != -1 && CurrentQuantum == quantum && CurrentProcess.RemainingTime != 0) // if true => there was a process already running
            {
                StopProcess(CurrentProcess, MaxArrivalTime);
                var += CurrentQuantum;
                CurrentProcess.RemainingTime = *ShmAddr;
                push(&q, CurrentProcess, 0);
            }

            // switch to another process, or get the first arrived process to work
            if (!isEmpty(q))
            {
                CurrentProcess = pop(&q);
                // update the remaining time in the memory
                *ShmAddr = CurrentProcess.RemainingTime;
                x = CurrentProcess.RemainingTime;

                // check if this is the first time to start or conitue
                if (CurrentProcess.RemainingTime == CurrentProcess.RunTime)
                    StartProcess(&CurrentProcess, MaxArrivalTime);
                else
                    ContinueProcess(CurrentProcess, MaxArrivalTime);
            }
            CurrentQuantum = 0; // reset
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------//
/*
Function explaintion:-
    This function is the implementation of the Shortest Remaining Time Next Algorithm,
    This Algorithm is the preemptive version of SJF scheduling.
    At the arrival of every process, the schedules select the process with the least remaining burst time among the list of available processes and the running process.
*/
void SRTN()
{
    initClk();
    printf("SRTN starts\n");

    Process CurrentProcess;
    Process *temp;
    Node *q = NULL;
    Node *Priority_Q = NULL;

    int LowestRemainingTime = 0,
        condition = 0,
        flag = 0,
        MaxArrivalTime = -1,
        counter = 0,
        last = -1;
    int ProcessesHashTime[LastArrivalTime + 1];

    *ShmAddr = -1;
    ReceivedCounter = CountProcesses;

    for (int i = 0; i <= LastArrivalTime; i++)
    {
        ProcessesHashTime[i] = 0;
    }

    while (CountProcesses > 0)
    {
        // check if there is a process arrived
        if (ReceivedCounter > 0)
        {
            ReadyProcessExist(MsgQueueId, &q, &flag, 0, &MaxArrivalTime, ProcessesHashTime);
            ReceivedCounter -= flag;
        }
        last = getClk();

        if (!isEmpty(q))
        {
            Process PoppedProcess;

            int time = getClk() - MaxArrivalTime; // clock
            for (int j = ProcessesHashTime[time]; j > 0; j--)
            {
                PoppedProcess = pop(&q);
                push(&Priority_Q, PoppedProcess, 2);
            }

            ProcessesHashTime[time] = -1;
        }

        if (isEmpty(Priority_Q) && *ShmAddr != -1)
        {
            condition = 0;
        }
        else
        {
            LowestRemainingTime = peek(Priority_Q).RemainingTime;
            condition = comparePriorities(*ShmAddr, LowestRemainingTime); // flag : 1 --> switch
        }
        if (condition == 1 && *ShmAddr != -1)
        {
            CurrentProcess.RemainingTime = *ShmAddr;
            push(&Priority_Q, CurrentProcess, 2);
            StopProcess(CurrentProcess, MaxArrivalTime);
            CurrentProcess = pop(&Priority_Q);
            *ShmAddr = CurrentProcess.RemainingTime + 1;

            if (CurrentProcess.RemainingTime == CurrentProcess.RunTime)
            {
                StartProcess(&CurrentProcess, MaxArrivalTime);
                continue;
            }
            else
            {
                ContinueProcess(CurrentProcess, MaxArrivalTime);
                continue;
            }
        }

        if (*ShmAddr == 0)
        {
            FinishProcess(&CurrentProcess, ShmAddr, MaxArrivalTime);
            *ShmAddr = -1;
            CountProcesses--;
            if (CountProcesses == 0)
            {
                break;
            }
        }

        if (!isEmpty(Priority_Q) && *ShmAddr == -1)
        {
            // pop a new process to run it
            CurrentProcess = pop(&Priority_Q);
            *ShmAddr = CurrentProcess.RemainingTime + 1;
            if (CurrentProcess.RemainingTime == CurrentProcess.RunTime)
            {
                StartProcess(&CurrentProcess, MaxArrivalTime);
                continue;
            }
            else
                ContinueProcess(CurrentProcess, MaxArrivalTime);
        }

        // wait until the clock changes
        while (last == getClk())
            ;
        if (last != getClk())
        {
            last = getClk();
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------//
/*
Function explaintion:-
    This function clears the resources
*/
void clearResources(int signum)
{
    msgctl(MsgQueueId, IPC_RMID, (struct msqid_ds *)0);
    shmctl(SharedMemoryId, IPC_RMID, NULL);
    signal(SIGINT, SIG_DFL);
}
