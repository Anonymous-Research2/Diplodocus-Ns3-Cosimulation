/*Copyright or (C) or Copr. GET / ENST, Telecom-Paris, Daniel Knorreck,
Ludovic Apvrille, Renaud Pacalet
 *
 * ludovic.apvrille AT telecom-paristech.fr
 *
 * This software is governed by the CeCILL license under French law and
 * abiding by the rules of distribution of free software.
 * See http://www.cecill.info
 */

#include <TMLNetCommand.h>
#include <TMLTask.h>
#include <TMLTransaction.h>
#include <TMLNetTask.h>
#include <NS3TaskInterface.h>
#include <NetBridgeConfig.h>

// 'w' for a sender NI task, 'r' for a receiver NI task, ' ' otherwise.
// Roles come from netbridge.conf, not from the task names.
static char netKind(TMLTask* task) {
    std::string name = static_cast<TMLNetTask*>(task)->getName();
    if (NetBridgeConfig::get().isSender(name))   return 'w';
    if (NetBridgeConfig::get().isReceiver(name)) return 'r';
    return ' ';
}

TMLNetCommand::TMLNetCommand(ID iID, TMLTask* iTask, LengthFuncPointer iLengthFunc, const char* iLiveVarList, bool iCheckpoint, TMLLength iStatLength)
    : TMLCommand(iID, iTask, 1, 1, iLiveVarList, iCheckpoint),_lengthFunc(iLengthFunc){
    _length = iStatLength;
    _type=NET;
}

void TMLNetCommand::execute(){
    if (((TMLNetTask*)_task)->getNetStatus() == NET_NONE) {
        uint64_t startCycle = (uint64_t)_currTransaction->getStartTime();
        if (!(_lengthFunc == 0))
            ((TMLNetTask*)_task)->getNetTask()->transfer(
                (((TMLNetTask*)_task)->*_lengthFunc)(), startCycle);
        else
            ((TMLNetTask*)_task)->getNetTask()->transfer(_length, startCycle);
        ((TMLNetTask*)_task)->setNetStatus(NET_RUN);
    } else { // getNetStatus() == NET_DONE
        _progress=_length;
        _task->addTransaction(_currTransaction);
        ((TMLNetTask*)_task)->setNetStatus(NET_NONE);
        prepare(false);
    }
}

TMLCommand* TMLNetCommand::prepareNextTransaction(){
    _currTransaction = new TMLTransaction(this, 1,_task->getEndLastTransaction());
    return this;
}

std::string TMLNetCommand::toString() const {
    std::ostringstream outp;
    char k = netKind(_task);
    if (k == 'w')      outp << "Net write in "    << TMLCommand::toString();
    else if (k == 'r') outp << "Net read in "     << TMLCommand::toString();
    else               outp << "Net transfer in " << TMLCommand::toString();
    return outp.str();
}

std::string TMLNetCommand::toString(TMLLength aLength, TMLLength aProgress) const{
    std::ostringstream outp;
    char k = netKind(_task);
    if (k == 'w')      outp << "Net write in "    << TMLCommand::toString(aLength, aProgress);
    else if (k == 'r') outp << "Net read in "     << TMLCommand::toString(aLength, aProgress);
    else               outp << "Net transfer in " << TMLCommand::toString(aLength, aProgress);
    return outp.str();
}

// IMPORTANT: no space after ':' — TMLTask.cpp takes substr(index+1, 2)
std::string TMLNetCommand::toShortString() const{
    std::ostringstream outp;
    char k = netKind(_task);
    if (k == 'w')      outp << _task->toString() << ":Nw";
    else if (k == 'r') outp << _task->toString() << ":Nr";
    else               outp << _task->toString() << ":N ";
    return outp.str();
}

std::string TMLNetCommand::toShortString(TMLLength aLength, TMLLength aProgress) const{
    return toShortString();
}

#ifdef ADD_COMMENTS
std::string TMLNetCommand::getCommentString(Comment* iCom) const{
    std::ostringstream comment;
    comment << "Net transfer, length " << _length << "=" << iCom->_actionCode;
    return comment.str();
}
#endif