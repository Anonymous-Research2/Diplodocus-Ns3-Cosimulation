#include <OntologyBridgeRunner.h>
#include <NetBridgeConfig.h>
#include <TMLCommand.h>
#include <TMLTransaction.h>
#include <SchedulableDevice.h>
#include <fstream>
#include <sstream>
#include <map>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>

// Own log file, because the simulator console is inside TTool
static std::ofstream& bridgeLog() {
    static std::ofstream f("/tmp/ontobridge.log", std::ios::trunc);
    return f;
}
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <algorithm>

OntologyBridgeRunner::OntologyBridgeRunner(unsigned int timePerCycle,
                                           const std::string& ttlPath,
                                           unsigned int pollSleepMs)
    : NS3Runner(timePerCycle),
      _ttlPath(ttlPath),
      _sideChannelPath(
          "/home/USER/TTool-ns3/simulators/c++2/net_done.sidechannel"),
      _pollSleepMs(pollSleepMs),
      _currentPs(0),
      _lastEndPs(0),
      _useLastEnd(false),
      _udpSockTimeslice(-1),
      _ns3Launched(false) {

    std::ofstream clear(_sideChannelPath, std::ios::trunc);
    clear.close();

    // Remove all per-receiver start files from a previous run
    system("rm -f /home/USER/TTool-ns3/simulators/c++2/"
           "net_done.sidechannel.receiver_start* 2>/dev/null");

    _udpSockTimeslice = socket(AF_INET, SOCK_DGRAM, 0);
    struct timeval tv{150, 0};   // wait at most 150 s for NS3's reply
    setsockopt(_udpSockTimeslice, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    memset(&_ns3Addr, 0, sizeof(_ns3Addr));
    _ns3Addr.sin_family = AF_INET;
    _ns3Addr.sin_port   = htons(9998);
    inet_pton(AF_INET, "127.0.0.1", &_ns3Addr.sin_addr);
}

OntologyBridgeRunner::~OntologyBridgeRunner() {
    sendSliceToNS3(0);
    if (_udpSockTimeslice >= 0) close(_udpSockTimeslice);
}

void OntologyBridgeRunner::registerTask(TMLNetTask* diploTask,
                                        OntologyBridgeTask* bridgeTask) {
    _tasks.push_back({diploTask, bridgeTask, 0});
    std::cout << "[OntoBridge] registered " << diploTask->getName() << std::endl;
}

// Number of transfers issued by the Sender tasks = commands Java must deliver
unsigned int OntologyBridgeRunner::senderTransfers() {
    unsigned int n = 0;
    for (auto& e : _tasks)
        if (NetBridgeConfig::get().isSender(e.diploTask->getName()))
            n += e.bridgeTask->getTransferCount();
    return n;
}

// Sends a slice, then waits for NS3's reply "now=..;results=.."
void OntologyBridgeRunner::sendSliceToNS3(double ns) {
    if (_udpSockTimeslice < 0 || ns < 0) return;
    std::string msg = "timesliceNs=" + std::to_string(ns)
                    + ";senders=" + std::to_string(senderTransfers()) + "\n";
    sendto(_udpSockTimeslice, msg.c_str(), msg.size(), 0,
           (struct sockaddr*)&_ns3Addr, sizeof(_ns3Addr));
    if (ns == 0) return;                                   // stop signal: no reply
    char buf[128];
    ssize_t n = recv(_udpSockTimeslice, buf, sizeof(buf) - 1, 0);
    if (n <= 0) { bridgeLog() << "[OntoBridge] no reply from NS3" << std::endl; return; }
    buf[n] = '\0';
    std::string r(buf);
    auto get = [&](const std::string& k) -> uint64_t {
        size_t p = r.find(k + "=");
        return p == std::string::npos ? 0 : std::stoull(r.substr(p + k.size() + 1));
    };
    _ns3Ns      = get("now");
    _ns3Results = (uint32_t)get("results");
}

// Diplodocus time in ns (timePerCycle is in ps: 5000 ps = 5 ns at 200 MHz)
uint64_t OntologyBridgeRunner::diploNs() {
    uint64_t cyc = (uint64_t)SchedulableDevice::getSimulatedTime();
    cyc = std::max(cyc, _lastEndPs / (uint64_t)timePerCycle);
    return cyc * (uint64_t)timePerCycle / 1000;
}

// LET: NS3 runs exactly up to Diplodocus time (continues after a result stop)
void OntologyBridgeRunner::syncNS3(uint64_t targetNs) {
    while (_ns3Ns < targetNs) {
        uint64_t before = _ns3Ns;
        sendSliceToNS3((double)(targetNs - _ns3Ns));
        waitResultsConsumed();      // never let Diplodocus pass a result Java has not delivered
        if (_ns3Ns <= before) break;                       // no progress: give up
    }
    bridgeLog() << "[OntoBridge] SYNC diploNs=" << diploNs()
                << " ns3Ns=" << _ns3Ns << " results=" << _ns3Results << std::endl;
}

int OntologyBridgeRunner::sideChannelLines() {
    std::ifstream f(_sideChannelPath);
    std::string line; int n = 0;
    while (std::getline(f, line)) if (line.rfind("NET_DONE ", 0) == 0) n++;
    return n;
}

// Do not let NS3 go further before Java has written every result it produced
void OntologyBridgeRunner::waitResultsConsumed() {
    auto t0 = std::chrono::steady_clock::now();
    while (sideChannelLines() < (int)_ns3Results) {
        if (std::chrono::steady_clock::now() - t0 > std::chrono::seconds(120)) {
            bridgeLog() << "[OntoBridge] timeout waiting Java results" << std::endl;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Returns endCycle (CYCLES) of line "NET_DONE <task> <endCycle> <iter>", or -1
int64_t OntologyBridgeRunner::readSideChannelRaw(const std::string& taskName,
                                                 unsigned int iteration) {
    std::ifstream f(_sideChannelPath);
    if (!f.is_open()) return -1;
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("NET_DONE ", 0) != 0) continue;
        std::istringstream ss(line.substr(9));
        std::string name; int64_t cycle = -1; unsigned int iter = 1;
        ss >> name >> cycle >> iter;
        if (name == taskName && iter == iteration) return cycle;
    }
    return -1;
}

// Pushes AT MOST ONE finished task (the one with the earliest end) into dones.
// Side-channel line: NET_DONE <task> <durationCycles> <iter>
//   durationCycles = time measured by NS3 (send: ~20, receive: ~20 + propagation)
// End = start of the NET transaction actually placed on the NI + penalties + duration,
// so Simulator.cpp computes length = now - start - penalties = durationCycles exactly.
// Per-NI (per CPU) end of the last network block placed by this runner
static std::map<const void*, uint64_t>& netEndPerCpu() {
    static std::map<const void*, uint64_t> m;
    return m;
}

// Earliest time the NI core of task `t` is free: end of the last transaction of
// every registered task mapped on the SAME NI, and of the last network block
// placed on it. Other NIs are NOT taken into account (they work in parallel).
uint64_t OntologyBridgeRunner::coreFreeTime(TMLNetTask* t) {
    const void* cpu = (const void*)t->getCPU();
    uint64_t free = netEndPerCpu()[cpu];
    for (auto& o : _tasks)
        if ((const void*)o.diploTask->getCPU() == cpu)
            free = std::max(free, (uint64_t)o.diploTask->getEndLastTransaction());
    return free;
}

// Real start cycle of each send, per sender task and iteration:
//   senderStart["App1__Sender1NI"][iter] = cycle where its NET block started
static std::map<std::string, std::map<unsigned int, uint64_t>>& senderStarts() {
    static std::map<std::string, std::map<unsigned int, uint64_t>> m;
    return m;
}

// Side-channel line: NET_DONE <task> <cycles> <iter>
//   Sender  : cycles = transmission time measured by NS3 (emission of last packet)
//   Receiver: cycles = time from the SENDER's start to the arrival of the last
//             packet (transmission + propagation), measured by NS3
// Sender   end = its real start + cycles
// Receiver end = arrival = start of the paired sender (same iteration) + cycles
//               (or start+1 if the packet already arrived before it started waiting)
bool OntologyBridgeRunner::pushOneDone() {
    TaskEntry*      best = nullptr;
    TMLTransaction* bestTr = nullptr;
    uint64_t bestEnd = 0, bestStart = 0, bestDur = 0, bestOrig = 0, bestFree = 0;
    int64_t  bestSendStart = -1;

    for (auto& e : _tasks) {
        if (e.diploTask->getNetStatus() != NET_RUN) continue;
        if (std::find(dones.begin(), dones.end(), e.diploTask) != dones.end())
            continue;
        unsigned int iter = e.pushCount + 1;
        int64_t dur = readSideChannelRaw(e.diploTask->getName(), iter);
        if (dur < 0) continue;
        if (dur < 1) dur = 1;

        TMLCommand*     cmd = e.diploTask->getCurrCommand();
        TMLTransaction* tr  = (cmd != nullptr) ? cmd->getCurrTransaction() : nullptr;
        uint64_t trStart = (tr != nullptr) ? (uint64_t)tr->getStartTime()
                                           : e.bridgeTask->getLastStartCycle();
        uint64_t pen       = (tr != nullptr) ? (uint64_t)tr->getPenalties() : 0;
        uint64_t freeAt    = coreFreeTime(e.diploTask);
        uint64_t realStart = std::max(trStart, freeAt);
        uint64_t end;
        int64_t  sendStart = -1;

        const std::string& name = e.diploTask->getName();
        if (NetBridgeConfig::get().isReceiver(name)) {
            std::string sender = NetBridgeConfig::get().pairedSender(name);
            auto it = senderStarts()[sender].find(iter);
            if (it == senderStarts()[sender].end()) continue;   // send not placed yet
            sendStart = (int64_t)it->second;
            uint64_t arrival = it->second + (uint64_t)dur;
            end = std::max(arrival, realStart + pen + 1);
        } else {
            end = realStart + pen + (uint64_t)dur;
        }

        if (best == nullptr || end < bestEnd) {
            best = &e; bestTr = tr; bestEnd = end; bestFree = freeAt;
            bestStart = realStart; bestOrig = trStart; bestDur = (uint64_t)dur;
            bestSendStart = sendStart;
        }
    }
    if (best == nullptr) return false;

    if (bestTr != nullptr && bestStart != bestOrig)
        bestTr->setStartTime(bestStart);
    netEndPerCpu()[(const void*)best->diploTask->getCPU()] = bestEnd;

    best->pushCount++;
    if (NetBridgeConfig::get().isSender(best->diploTask->getName()))
        senderStarts()[best->diploTask->getName()][best->pushCount] = bestStart;

    _lastEndPs  = bestEnd * (uint64_t)timePerCycle;
    _useLastEnd = true;
    _currentPs  = std::max(_currentPs, _lastEndPs);
    dones.push_back(best->diploTask);

    std::ostringstream m;
    m << "[OntoBridge] DONE " << best->diploTask->getName()
      << " iter=" << best->pushCount
      << " trStart=" << bestOrig << " niFree=" << bestFree
      << " start=" << bestStart;
    if (bestSendStart >= 0)
        m << " senderStart=" << bestSendStart << " arrival=" << (bestSendStart + (int64_t)bestDur);
    else
        m << " duration=" << bestDur;
    m << " end=" << bestEnd << " length=" << (bestEnd - bestStart);
    std::cout << m.str() << std::endl;
    bridgeLog() << m.str() << std::endl;
    return true;
}

bool OntologyBridgeRunner::anyTaskPending() {
    for (auto& e : _tasks)
        if (e.diploTask->getNetStatus() == NET_RUN) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Start-up model check (before cycle 0): write ONE dip:ModelCheckRequest in the
// ontology, wait for the Java verdict. Returns false if the model cannot be
// co-simulated.
// ---------------------------------------------------------------------------
static const char* CHECK_RESULT =
    "/home/USER/TTool-ns3/simulators/c++2/net_check.result";

bool OntologyBridgeRunner::modelCheck() {
    const std::string& model = NetBridgeConfig::get().modelPath;
    if (model.empty()) {
        std::cout << "[OntoBridge] No 'model' line in netbridge.conf: model check skipped." << std::endl;
        bridgeLog() << "[OntoBridge] No 'model' line in netbridge.conf: model check skipped." << std::endl;
        return true;
    }
    std::remove(CHECK_RESULT);

    // Write the request (same file lock as the Java side)
    int lockFd = open((_ttlPath + ".lock").c_str(), O_CREAT | O_RDWR, 0666);
    struct flock fl{};
    fl.l_type = F_WRLCK; fl.l_whence = SEEK_SET; fl.l_start = 0; fl.l_len = 0;
    fcntl(lockFd, F_SETLKW, &fl);
    {
        std::ofstream ttl(_ttlPath, std::ios::app);
        ttl << "\n:ModelCheck_startup rdf:type :ModelCheckRequest ;\n"
            << "    :modelPath \"" << model << "\" .\n";
    }
    fl.l_type = F_UNLCK; fcntl(lockFd, F_SETLK, &fl); close(lockFd);

    bridgeLog() << "[OntoBridge] ModelCheckRequest written to " << _ttlPath
                << " for model " << model << std::endl;
    std::cout << "[OntoBridge] Model check requested for " << model
              << " - waiting for the reasoner verdict..." << std::endl;

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(300);
    while (std::chrono::steady_clock::now() < deadline) {
        std::ifstream f(CHECK_RESULT);
        std::string first;
        if (f.is_open() && std::getline(f, first) && !first.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200)); // let Java finish writing
            std::ifstream all(CHECK_RESULT);
            std::string line;
            std::cout << "================ MODEL CHECK ================" << std::endl;
            while (std::getline(all, line)) {
                std::cout << line << std::endl;
                bridgeLog() << "[model-check] " << line << std::endl;
            }
            std::cout << "=============================================" << std::endl;
            return first == "CONSISTENT";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    std::cout << "[OntoBridge] Model check: no verdict after 300 s (is the Java bridge running?)." << std::endl;
    return false;
}

void OntologyBridgeRunner::run() {
    if (!_ns3Launched) {
        _ns3Launched = true;
        if (!modelCheck()) {
            std::cout << "[OntoBridge] The model cannot be co-simulated with the network:"
                      << " NS3 is NOT started, simulation stopped before cycle 0." << std::endl;
            bridgeLog() << "[OntoBridge] model check failed: simulation stopped" << std::endl;
            exit(1);
        }
        system("pkill -f 'java-bridge-sim-Visual-RT2_SynAuto' 2>/dev/null || true");
        system("fuser -k 9998/udp 9999/udp 2>/dev/null || true");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "[OntoBridge] Launching NS3..." << std::endl;
        pid_t pid = fork();
        if (pid == 0) {
            FILE* logf = fopen("/tmp/ns3_bridge.log", "w");
            if (logf) {
                setvbuf(logf, nullptr, _IOLBF, 0);
                dup2(fileno(logf), STDOUT_FILENO);
                dup2(fileno(logf), STDERR_FILENO);
            }
            execl("/home/USER/ns-3-dev/ns3", "ns3", "run",
                  "scratch/java-bridge-sim-Visual-RT2_SynAuto",
                  (char*)nullptr);
            perror("execl failed"); exit(1);
        } else if (pid > 0) {
            std::cout << "[OntoBridge] Waiting 8s..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(8));
        }
        return;
    }

    if (!anyTaskPending()) {
        std::cout << "[OntoBridge] All done." << std::endl;
        return;
    }

    std::cout << "[OntoBridge] Draining..." << std::endl;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(300);
    while (true) {
        waitResultsConsumed();               // Java has written all NS3 results
        if (pushOneDone()) return;           // one transaction gets its end -> Diplodocus moves
        if (!anyTaskPending()) return;
        if (std::chrono::steady_clock::now() >= deadline) {
            std::cout << "[OntoBridge] Drain timeout." << std::endl;
            return;
        }
        sendSliceToNS3(10000000.0);          // NS3 stops by itself at the NEXT result
        bridgeLog() << "[OntoBridge] IDLE diploNs=" << diploNs()
                    << " ns3Ns=" << _ns3Ns << " results=" << _ns3Results << std::endl;
    }
}

void OntologyBridgeRunner::run(long int timeslice) {
    _currentPs += (uint64_t)timeslice;
    syncNS3(diploNs());      // NS3 catches up with Diplodocus before this LET
    pushOneDone();
}

uint64_t OntologyBridgeRunner::now() {
    if (_useLastEnd) {
        _useLastEnd = false;
        std::cout << "[OntoBridge] now() -> endCycle "
                  << (_lastEndPs / timePerCycle) << std::endl;
        return _lastEndPs;
    }
    return _currentPs;
}