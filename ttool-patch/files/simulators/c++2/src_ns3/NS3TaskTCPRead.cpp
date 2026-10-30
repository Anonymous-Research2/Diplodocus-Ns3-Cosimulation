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

/* 
 * File:   NS3TaskTCPRead.cpp
 * Author: coudert
 *
 * Created on 4 juillet 2025, 14:35
 */

#ifdef NS3LOG
#include <NS3Runner.h>
#endif
#include <NS3TaskTCPRead.h>
#include <ns3/address-utils.h>
#include <ns3/simulator.h>


using namespace ns3;
 
NS_LOG_COMPONENT_DEFINE("NS3TaskTCPRead");
 
NS_OBJECT_ENSURE_REGISTERED(NS3TaskTCPRead);

 
NS3TaskTCPRead::NS3TaskTCPRead(TMLNetTask* task,
                ns3::Address local_addr, uint16_t local_port,
                ns3::Address peer_addr, uint16_t peer_port) 
    : NS3TaskTCP(task, local_addr, local_port, peer_addr, peer_port)
    { NS_LOG_FUNCTION(this); 
#ifdef NS3LOG
    *TMLNetTask::_ns3log << "TCPRead Task " << diploTask->getName() << " created\n"; 
#endif
}

TypeId NS3TaskTCPRead::GetTypeId()
{
    static TypeId tid =
        TypeId("NS3TaskTCPRead")
            .SetParent<NS3TaskTCP>();
    return tid;
}
void NS3TaskTCPRead::StartApplication() {
    NS_LOG_FUNCTION(this);
#ifdef NS3LOG
    *TMLNetTask::_ns3log << "TCPRead Task " << diploTask->getName() << " starts at " << Now() << "\n";
#endif
    NS3TaskTCP::StartApplication();
    auto peerSocketAddress = addressUtils::ConvertToSocketAddress(peerAddress, peerPort);
    socket->SetConnectCallback(MakeCallback(&NS3TaskTCPRead::ConnectionSucceeded, this),
                               MakeCallback(&NS3TaskTCPRead::ConnectionFailed, this));
    int check = socket->Connect(peerSocketAddress);
    if (check == -1) {
        Socket::SocketErrno err = socket->GetErrno();
        NS_FATAL_ERROR("Connecting task " + diploTask->getName() + " to peer fails" + ": " << err << ", address " << peerAddress);
    }
    //socket->ShutdownSend();
}
void NS3TaskTCPRead::ConnectionSucceeded(Ptr<Socket> socket) {
    NS_LOG_FUNCTION(this << socket);
#ifdef NS3LOG
    *TMLNetTask::_ns3log << "TCPRead Task " << diploTask->getName() << "  connect OK callback\n";
#endif
}
void NS3TaskTCPRead::ConnectionFailed(Ptr<Socket> socket) {
    NS_LOG_FUNCTION(this << socket);
#ifdef NS3LOG
    *TMLNetTask::_ns3log << "TCPRead Task " << diploTask->getName() << "  connect FAILURE callback\n";
#endif
    Socket::SocketErrno err = socket->GetErrno();
    NS_FATAL_ERROR("Can't connect task " + diploTask->getName() + " to peer" + ": " << err << ", address " << peerAddress);
}

void NS3TaskTCPRead::transfer(uint64_t _amount, uint64_t startCycle) {
    NS_LOG_FUNCTION(this);
    amount = _amount;
    read(socket);
}

void NS3TaskTCPRead::read(Ptr<Socket> dummy) {
    NS_LOG_FUNCTION(this);
    uint32_t available = socket->GetRxAvailable();
    if (available >= amount) { // transfer completed
        if (amount) {
            // receive amount data and reset amount
            uint8_t* buf = (uint8_t*) malloc(amount);
            int received = socket->Recv(buf, amount, 0);
#ifdef NS3LOG
            *TMLNetTask::_ns3log << "\n** Task " + diploTask->getName() + " has received : " << received;
            NS3Runner::_hasLog = true;
#endif
            amount = amount - received;
            free(buf);
            if(amount != 0)
                NS_FATAL_ERROR("BUG. semantical error to fix");
        }
        Simulator::Schedule(PicoSeconds(timePerCycle), &NS3TaskTCPRead::done, this);
        socket->SetRecvCallback(MakeNullCallback<void,Ptr<Socket>>());
    } 
    else {
        if (available) {
            // receive available data and update amount
            uint8_t *buf = (uint8_t*) std::malloc(available);
            int received = socket->Recv(buf, available, 0);
            amount = amount - received;
            available = available - received;
            free(buf);
            if(available != 0)
                NS_FATAL_ERROR("BUG. semantical error to fix");
#ifdef NS3LOG
            *TMLNetTask::_ns3log << "\n** Task " + diploTask->getName() + " has received : " << received;
            NS3Runner::_hasLog = true;
#endif
        }
        // program remaining reads
        socket->SetRecvCallback(MakeCallback(&NS3TaskTCPRead::read, this));      
    }
}

void NS3TaskTCPRead::done() {
    NS_LOG_FUNCTION(this);
        // call "done" trace callback and stop simulation
        taskDoneTrace(this);
        Simulator::Stop();                
    } 

