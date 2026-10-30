// NetBridgeConfig.h - roles and pairing of NI tasks, read from netbridge.conf
// so that nothing depends on the task names.
#ifndef NETBRIDGECONFIG_H
#define NETBRIDGECONFIG_H

#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>

#define NETBRIDGE_CONF "/home/USER/TTool-ns3/simulators/c++2/netbridge.conf"

struct NetBridgeConfig {
    std::map<std::string, std::string> senderToReceiver;
    std::map<std::string, std::string> receiverToSender;
    std::map<std::string, int>         senderNode;
    std::string                        modelPath;   // 'model <path>' line

    static NetBridgeConfig& get() {
        static NetBridgeConfig c;
        static bool loaded = false;
        if (!loaded) { c.load(); loaded = true; }
        return c;
    }

    void load() {
        std::ifstream f(NETBRIDGE_CONF);
        if (!f.is_open()) {
            std::cerr << "[NetBridgeConfig] ERROR cannot open " << NETBRIDGE_CONF << std::endl;
            return;
        }
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            if (line.rfind("model ", 0) == 0) {          // model <path of the TTool .xml>
                modelPath = line.substr(6);
                while (!modelPath.empty() && modelPath[0] == ' ') modelPath.erase(0, 1);
                std::cerr << "[NetBridgeConfig] model " << modelPath << std::endl;
                continue;
            }
            std::istringstream ss(line);
            std::string s, r; int sn = 0, rn = 1;
            if (!(ss >> s >> r)) continue;
            ss >> sn >> rn;
            s = task(s); r = task(r);
            senderToReceiver[s] = r;
            receiverToSender[r] = s;
            senderNode[s] = sn;
            std::cerr << "[NetBridgeConfig] pair " << s << " -> " << r
                      << " node " << sn << "->" << rn << std::endl;
        }
    }

    // Accepts "cmd_WACS__SubTxNI", "WACS__SubTxNI" or "SubTxNI" and returns
    // the short task name "SubTxNI" (part after the application prefix "__").
    static std::string task(const std::string& name) {
        std::string t = name.rfind("cmd_", 0) == 0 ? name.substr(4) : name;
        size_t p = t.rfind("__");
        return p == std::string::npos ? t : t.substr(p + 2);
    }
    bool isSender(const std::string& name)   { return senderToReceiver.count(task(name)) > 0; }
    bool isReceiver(const std::string& name) { return receiverToSender.count(task(name)) > 0; }
    // Returns the paired sender with the SAME application prefix as `receiver`
    std::string pairedSender(const std::string& receiver) {
        auto it = receiverToSender.find(task(receiver));
        if (it == receiverToSender.end()) return "";
        std::string r = receiver.rfind("cmd_", 0) == 0 ? receiver.substr(4) : receiver;
        size_t p = r.rfind("__");
        return (p == std::string::npos ? "" : r.substr(0, p + 2)) + it->second;
    }
};

#endif