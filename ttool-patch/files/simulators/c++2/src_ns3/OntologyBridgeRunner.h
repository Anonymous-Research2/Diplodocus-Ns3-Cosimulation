#ifndef ONTOLOGYBRIDGERUNNER_H
#define ONTOLOGYBRIDGERUNNER_H

#include <NS3Runner.h>
#include <OntologyBridgeTask.h>
#include <string>
#include <vector>
#include <cstdint>
#include <netinet/in.h>

/*
 * IMPORTANT RULE (from Simulator.cpp):
 *   Diplodocus calls now() ONCE and applies that single end time to EVERY
 *   task in `dones`. Therefore this runner pushes AT MOST ONE task into
 *   `dones` per run() call, and now() returns exactly that task's end.
 *
 * Units: _currentPs and _lastEndPs are PICOSECONDS.
 *        Side-channel endCycle and task startCycle are CYCLES.
 */
class OntologyBridgeRunner : public NS3Runner {
public:
    OntologyBridgeRunner(unsigned int timePerCycle,
                         const std::string& ttlPath,
                         unsigned int pollSleepMs = 5);
    ~OntologyBridgeRunner();

    void registerTask(TMLNetTask* diploTask, OntologyBridgeTask* bridgeTask);
    void run(long int timeslice) override;
    void run() override;
    uint64_t now() override;

private:
    struct TaskEntry {
        TMLNetTask*         diploTask;
        OntologyBridgeTask* bridgeTask;
        unsigned int        pushCount;   // how many results already consumed
    };

    std::string        _ttlPath;
    std::string        _sideChannelPath;
    unsigned int       _pollSleepMs;
    uint64_t           _currentPs;    // accumulated simulated time (ps)
    uint64_t           _lastEndPs;    // end of the task just pushed (ps)
    bool               _useLastEnd;   // true -> now() returns _lastEndPs
    int                _udpSockTimeslice;
    struct sockaddr_in _ns3Addr;
    bool               _ns3Launched;
    std::vector<TaskEntry> _tasks;
    uint64_t           _ns3Ns = 0;       // NS3 current time (ns), from its reply
    uint32_t           _ns3Results = 0;  // results NS3 has emitted so far

    void    sendSliceToNS3(double ns);
    uint64_t diploNs();                  // Diplodocus current time (ns)
    void    syncNS3(uint64_t targetNs);  // NS3 runs up to targetNs
    int     sideChannelLines();
    void    waitResultsConsumed();
    unsigned int senderTransfers();
    bool    pushOneDone();
    uint64_t coreFreeTime(TMLNetTask* t);
    bool     modelCheck();
    bool    anyTaskPending();
    int64_t readSideChannelRaw(const std::string& taskName,
                               unsigned int iteration);
};

#endif