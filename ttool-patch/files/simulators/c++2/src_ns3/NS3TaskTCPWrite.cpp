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
 * File:   NS3TaskTCPWrite.cpp
 * Author: coudert
 *
 * Created on 2 juillet 2025, 09:56
 */

#ifdef NS3LOG
#include <NS3Runner.h>
#endif
#include <NS3TaskTCPWrite.h>
#include <ns3/simulator.h>
#include <ns3/address-utils.h>


using namespace ns3;
 
NS_LOG_COMPONENT_DEFINE("NS3TaskTCPWrite"); 

NS_OBJECT_ENSURE_REGISTERED(NS3TaskTCPWrite);


 
NS3TaskTCPWrite::NS3TaskTCPWrite(TMLNetTask* task,
                ns3::Address local_addr, uint16_t local_port,
                ns3::Address peer_addr, uint16_t peer_port) 
    : NS3TaskTCP(task, local_addr, local_port, peer_addr, peer_port)
    { NS_LOG_FUNCTION(this); 
#ifdef NS3LOG
    *TMLNetTask::_ns3log << "TCPWrite Task " << diploTask->getName() << " created\n"; 
#endif
}

TypeId NS3TaskTCPWrite::GetTypeId()
{
    static TypeId tid =
        TypeId("NS3TaskTCPWrite")
            .SetParent<NS3TaskTCP>();
    return tid;
}

bool NS3TaskTCPWrite::custom_true_callback(Ptr<Socket> s, const Address& a) { 
    NS_LOG_FUNCTION(this);
#ifdef NS3LOG
    *TMLNetTask::_ns3log << "TCPWrite Task " << diploTask->getName() << "  does not accepts at " << Now() << "\n";
#endif
    return true; 
}

void NS3TaskTCPWrite::StartApplication() {
    NS_LOG_FUNCTION(this);

#ifdef NS3LOG
    *TMLNetTask::_ns3log << "TCPWrite Task " << diploTask->getName() << " starts at " << Now() << "\n";
#endif
    NS3TaskTCP::StartApplication();
    socket->SetAcceptCallback(MakeCallback(&NS3TaskTCPWrite::custom_true_callback, this),
                              MakeCallback(&NS3TaskTCPWrite::handleAccept, this));
    socket->Listen();
}

void NS3TaskTCPWrite::handleAccept(Ptr<Socket> s, const Address& from) {
    NS_LOG_FUNCTION(this << s << from);
    Ptr<Socket> sock = socket;
    socket = s;
    sock->Close();
    sock->Dispose();
#ifdef NS3LOG
    *TMLNetTask::_ns3log << "TCPWrite Task " << diploTask->getName() << " accepts at " << Now() << "\n";
#endif
}


void NS3TaskTCPWrite::transfer(uint64_t _amount, uint64_t startCycle) {
    NS_LOG_FUNCTION(this << _amount);
    amount = _amount;
    Simulator::Schedule(PicoSeconds(timePerCycle), &NS3TaskTCPWrite::start, this);
}

void NS3TaskTCPWrite::start(){
    write(socket, 0);
}

void NS3TaskTCPWrite::write(Ptr<Socket> dummy1, uint32_t dummy2) {
    NS_LOG_FUNCTION(this);
    uint32_t available = socket->GetTxAvailable();
    while (available > 0 && amount > 0) {
        
        if (available >= amount) { // transfer completed
            // send amount data and reset amount
            int i = 0;
            while(amount) {
                int sent = socket->Send(0, amount, 0);
                if (sent == -1) {
                    Socket::SocketErrno err = socket->GetErrno();
                    NS_FATAL_ERROR("Task " + diploTask->getName() + " cannot send : " << err );
                }
                else
                amount = amount - sent;
#ifdef NS3LOG
                *TMLNetTask::_ns3log << "\n** Task " + diploTask->getName() + " has sent : " << sent << " (available was " << available << ", remaining " << available - sent << ")";
                NS3Runner::_hasLog = true;
#endif
                i++; if (i>4) break;
            }
            if(amount != 0)
                NS_FATAL_ERROR("BUG. semantical error to fix");
            // call "done" trace callback and stop simulation
            taskDoneTrace(this);
            socket->SetSendCallback(MakeNullCallback<void,Ptr<Socket>,uint32_t>());
            Simulator::Stop();
            return;
        }

        else {
            // send w.r.t available data space and update amount
            amount = amount - available;
            int sent = socket->Send(0, available, 0);
#ifdef NS3LOG
            *TMLNetTask::_ns3log << "\n** Task " + diploTask->getName() + " has sent : " << sent << " (available was " << available << ", remaining " << available - sent << ")";
            NS3Runner::_hasLog = true;
#endif
            available = available - sent;
            if(available != 0)
                NS_FATAL_ERROR("BUG. semantical error to fix");
            available = socket->GetTxAvailable();    
        }
    }
    // program remaining sends, if relevant
    if (amount)
        socket->SetSendCallback(MakeCallback(&NS3TaskTCPWrite::write, this));      
}
