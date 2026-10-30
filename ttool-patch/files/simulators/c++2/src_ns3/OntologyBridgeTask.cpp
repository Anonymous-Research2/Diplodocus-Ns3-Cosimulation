#include <OntologyBridgeTask.h>
#include <NetBridgeConfig.h>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

OntologyBridgeTask::OntologyBridgeTask(TMLNetTask* diploTask,
                                       const std::string& ttlPath,
                                       const std::string& commandId)
    : _diploTask(diploTask), _ttlPath(ttlPath),
      _commandId(commandId), _transferCount(0), _lastStartCycle(0) {
    diploTask->setNetTask(this);
}

void OntologyBridgeTask::transfer(uint64_t amount, uint64_t startCycle) {
    _transferCount++;
    _lastStartCycle = startCycle;

    // Determine if this is a sender or receiver task
    // Convention: task name contains "Sender" → sends to NS3
    //             task name contains "Receiver" → handled by paired sender
    bool isSender   = NetBridgeConfig::get().isSender(_commandId);
    bool isReceiver = NetBridgeConfig::get().isReceiver(_commandId);

    if (isReceiver) {
        // Write startCycle to per-task receiver file
        // File name: net_done.sidechannel.receiver_start.<commandId>
        std::string rcvFile =
            "/home/USER/TTool-ns3/simulators/c++2/"
            "net_done.sidechannel.receiver_start."
            + _commandId;
        std::ofstream rcv(rcvFile, std::ios::app);
        if (rcv.is_open()) {
            rcv << startCycle << " " << _transferCount << "\n";
            rcv.close();
        }
        std::cout << "[OntoBridge] " << _commandId
                  << " iter=" << _transferCount
                  << " startCycle=" << startCycle
                  << " (receiver, handled by paired sender)"
                  << std::endl;
        return;
    }

    if (!isSender) {
        // Unknown task type — skip
        std::cout << "[OntoBridge] WARNING: unknown task type "
                  << _commandId << " — skipping" << std::endl;
        return;
    }

    // Sender: write unique TTL individual
    std::string iterCommandId = _commandId + "_iter" +
                                std::to_string(_transferCount);

    // Same lock as the Java side (FileChannel.lock on diplodocus.owl.ttl.lock)
    int lockFd = open((_ttlPath + ".lock").c_str(), O_CREAT | O_RDWR, 0666);
    struct flock fl{};
    fl.l_type = F_WRLCK; fl.l_whence = SEEK_SET; fl.l_start = 0; fl.l_len = 0;
    fcntl(lockFd, F_SETLKW, &fl);

    std::ofstream ttl(_ttlPath, std::ios::app);
    if (!ttl.is_open()) {
        fl.l_type = F_UNLCK; fcntl(lockFd, F_SETLK, &fl); close(lockFd);
    }
    if (!ttl.is_open())
        throw std::runtime_error(
            "OntologyBridgeTask: cannot open " + _ttlPath);

    std::string dataQtyId    = iterCommandId + "_DataQty";
    std::string startCycleId = iterCommandId + "_StartCycle";
    std::string writeCmdId   = iterCommandId + "_WriteCmd";

    ttl << "\n"
        << ":" << dataQtyId << " rdf:type :DataQuantity ;\n"
        << "    :DataSize \"" << amount << "\"^^xsd:integer .\n"
        << "\n"
        << ":" << startCycleId << " rdf:type :CycleTimePoint ;\n"
        << "    :cycleValue \"" << startCycle << "\"^^xsd:double .\n"
        << "\n"
        << ":" << writeCmdId << " rdf:type :WriteChannelCommand ;\n"
        << "    :hasDataQuantity :" << dataQtyId << " ;\n"
        << "    :startsAtCycle   :" << startCycleId << " .\n";
    ttl.close();
    fl.l_type = F_UNLCK;
    fcntl(lockFd, F_SETLK, &fl);
    close(lockFd);

    std::cout << "[OntoBridge] " << _commandId
              << " iter=" << _transferCount
              << " startCycle=" << startCycle
              << " written to TTL as " << iterCommandId << std::endl;
}