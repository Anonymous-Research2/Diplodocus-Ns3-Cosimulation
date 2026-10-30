/*Copyright or (C) or Copr. GET / ENST, Telecom-Paris, Daniel Knorreck,
Ludovic Apvrille, Renaud Pacalet
 *
 * ludovic.apvrille AT telecom-paristech.fr
 *
 * This software is a computer program whose purpose is to allow the
 * edition of TURTLE analysis, design and deployment diagrams, to
 * allow the generation of RT-LOTOS or Java code from this diagram,
 * and at last to allow the analysis of formal validation traces
 * obtained from external tools, e.g. RTL from LAAS-CNRS and CADP
 * from INRIA Rhone-Alpes.
 *
 * This software is governed by the CeCILL  license under French law and
 * abiding by the rules of distribution of free software.  You can  use,
 * modify and/ or redistribute the software under the terms of the CeCILL
 * license as circulated by CEA, CNRS and INRIA at the following URL
 * "http://www.cecill.info".
 *
 * As a counterpart to the access to the source code and  rights to copy,
 * modify and redistribute granted by the license, users are provided only
 * with a limited warranty  and the software's author,  the holder of the
 * economic rights,  and the successive licensors  have only  limited
 * liability.
 *
 * In this respect, the user's attention is drawn to the risks associated
 * with loading,  using,  modifying and/or developing or reproducing the
 * software by the user in light of its specific status of free software,
 * that may mean  that it is complicated to manipulate,  and  that  also
 * therefore means  that it is reserved for developers  and  experienced
 * professionals having in-depth computer knowledge. Users are therefore
 * encouraged to load and test the software's suitability as regards their
 * requirements in conditions enabling the security of their systems and/or
 * data to be ensured and,  more generally, to use and operate it in the
 * same conditions as regards security.
 *
 * The fact that you are presently reading this means that you have had
 * knowledge of the CeCILL license and that you accept its terms.
 *
 */
#ifdef NS3LOG
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#endif

#include <TMLNetTask.h>
#include <TMLCommand.h>
#include <TMLStopCommand.h>
#include <CPU.h>
#include <FPGA.h>
#include <NetBridgeConfig.h>
#include <OntologyBridgeTask.h>
#include <vector>

// ===========================================================================
// Network rules for a single-core NI (co-simulation with NS3)
//
// Rule 1 - one network operation at a time per NI core:
//   while a task of an NI core has a network transfer in progress (NET_RUN),
//   no other task of that core may start a transaction. The core then really
//   is busy, so the start Diplodocus draws is the start NS3 simulates, and
//   transactions on the same core never overlap.
//
// Rule 2 - a read starts only when its data is on the way:
//   a receiver may start its read number k only after its paired sender has
//   issued its write number k. A read therefore never blocks its core waiting
//   for data that has not been sent yet (no deadlock between NI0 and NI1),
//   and it lasts at most transmission + propagation.
//
// When both a read and a write are allowed, the NI scheduler (RR) chooses,
// so reads and writes interleave on the core.
// ===========================================================================
static std::vector<TMLNetTask*>& allNetTasks() {
    static std::vector<TMLNetTask*> v;
    return v;
}

static unsigned int issuedCount(TMLNetTask* t) {
    NS3TaskInterface* n = t->getNetTask();
    return n ? static_cast<OntologyBridgeTask*>(n)->getTransferCount() : 0;
}

// Rule 1
static bool coreBusyWithNetwork(TMLNetTask* me) {
    const void* myCpu = (const void*)me->getCPU();
    for (TMLNetTask* t : allNetTasks()) {
        if (t == me) continue;
        if ((const void*)t->getCPU() != myCpu) continue;
        if (t->getNetStatus() == NET_RUN) return true;
    }
    return false;
}

// Rule 2: true if `me` is a receiver whose next read has no data sent yet
static bool readNotYetSent(TMLNetTask* me) {
    NetBridgeConfig& cfg = NetBridgeConfig::get();
    if (!cfg.isReceiver(me->getName())) return false;
    std::string sender = NetBridgeConfig::task(cfg.pairedSender(me->getName()));
    for (TMLNetTask* t : allNetTasks()) {
        if (NetBridgeConfig::task(t->getName()) != sender) continue;
        return issuedCount(t) <= issuedCount(me);
    }
    return false;                       // no paired sender found: do not block
}

TMLNetTask::TMLNetTask(ID iID, Priority iPriority, std::string iName, CPU** iCPU, unsigned int iNoOfCPUs, bool isDaemon): 
    TMLTask(iID, iPriority, iName, iCPU, iNoOfCPUs, isDaemon), _networkingStatus(NET_NONE), _netTask(0) {
    allNetTasks().push_back(this);
}

TMLNetTask::TMLNetTask(ID iID, Priority iPriority, std::string iName, FPGA** iFPGA, unsigned int iNoOfFPGAs, bool isDaemon):
    TMLTask(iID, iPriority, iName, iFPGA, iNoOfFPGAs, isDaemon), _networkingStatus(NET_NONE), _netTask(0) {
    allNetTasks().push_back(this);
}

void TMLNetTask::reset(){ _networkingStatus = NET_NONE; TMLTask::reset(); }

TMLTransaction* TMLNetTask::getNextTransaction(TMLTime iEndSchedule) const{
	if (_networkingStatus == NET_RUN)  return 0;                                // own transfer in progress
	if (_currCommand == 0)             return 0;
	if (_networkingStatus == NET_DONE) return _currCommand->getCurrTransaction();  // finished: give it back
	TMLNetTask* me = const_cast<TMLNetTask*>(this);
	if (coreBusyWithNetwork(me))       return 0;                                // rule 1
	if (_currCommand->getType() == NET && readNotYetSent(me)) return 0;         // rule 2
	return _currCommand->getCurrTransaction();
}

#ifdef NS3LOG
std::ostream * TMLNetTask::_ns3log = nullptr;
void TMLNetTask::setNs3log(std::string argv0, std::string name) {
    if (name.compare("stdout") == 0) {
        _ns3log = &std::cout;
        return;
    }
    if (name.compare("stderr") == 0) {
        _ns3log = &std::cerr;
        return;
    }
    if (name.compare("stdlog") == 0) {
        _ns3log = &std::clog;
        return;
    }
    if (name.empty()) name = "ns3sim.log";
    
    std::filesystem::path filepath;
    if (argv0.empty() || (name.rfind("./", 0) == 0)){
        filepath = name;
    }
    else {
        std::filesystem::path argv0path(argv0);
         filepath = argv0path.parent_path()  / name;
    }
    _ns3log = new std::ofstream(filepath, std::ios_base::trunc);
    if (!((std::ofstream *) TMLNetTask::_ns3log)->is_open()) {
        std::cerr << "Failed to create file: " << filepath << std::endl;
        delete _ns3log;
        _ns3log = new std::ofstream();
    }
}
void TMLNetTask::closeNs3log() {
    if (!(_ns3log == &std::cout || _ns3log == &std::cerr || _ns3log == &std::clog)) {
        ((std::ofstream *)_ns3log)->close();
        delete _ns3log;
    }
}
#endif