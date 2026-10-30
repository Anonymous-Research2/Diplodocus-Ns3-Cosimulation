#ifndef ONTOLOGYBRIDGETASK_H
#define ONTOLOGYBRIDGETASK_H

#include <NS3TaskInterface.h>
#include <TMLNetTask.h>
#include <string>

class OntologyBridgeTask : public NS3TaskInterface {
public:
    OntologyBridgeTask(TMLNetTask* diploTask,
                       const std::string& ttlPath,
                       const std::string& commandId);

    void transfer(uint64_t amount, uint64_t startCycle) override;
    inline std::string getCommandId() const { return _commandId; }
	
	inline unsigned int getTransferCount() const { return _transferCount; }
	inline uint64_t getLastStartCycle() const { return _lastStartCycle; }
	

private:
    TMLNetTask*  _diploTask;
    std::string  _ttlPath;
    std::string  _commandId;
	unsigned int _transferCount; // incremented each time transfer() is called
	uint64_t     _lastStartCycle; // start cycle of the current transfer
};

#endif