// java-bridge-sim-Visual-RT2_SynAuto.cc
//
// One script, transport chosen PER COMMAND by the ontology model check:
//   Port 9999 (from Java):
//     packetSize=X;id=Y;direction=D[;transport=UDP;errorRate=R]
//     packetSize=X;id=Y;direction=D[;transport=TCP;sndBuf=B]
//     (no transport field -> UDP without loss)
//   Port 9998 (from Diplodocus): timesliceNs=X;senders=N   (0 = stop)
//     reply after each slice: now=<ns>;results=<count>
//     a slice stops early at each RESULT (NS3 never runs past a result)
//   direction 0: node0 -> node1 ; direction 1: node1 -> node0
//
// Output read by Java (unchanged):
//   INJECTION_TIME commandId=<id> injectionTimeNs=<t>
//   RESULT_START / commandId=<id> / sendMeasuredLatencyNs=<t>    / RESULT_END
//   RESULT_START / commandId=<id> / receiveMeasuredLatencyNs=<t> / RESULT_END
// Extra info line: PACKET_LOST commandId=<id> t=<t>
//
// SEND end    = sender NIC finished transmitting the last data packet/segment
// RECEIVE end = last byte received by the receiver application
//               (UDP lost packet: the moment it was dropped at the receiver)

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <cerrno>
#include <map>
#include <chrono>
#include <algorithm>

using namespace ns3;

static int                    g_udpSock = -1;      // control socket (port 9999)
static NodeContainer          g_nodes;             // node0 = NI0, node1 = NI1
static Ipv4InterfaceContainer g_interfaces;
static uint32_t               g_commandsReceived = 0;
static bool                   g_shared = false;    // true: CSMA, false: P2P
static uint32_t               g_resultCount = 0;   // results emitted so far

struct IdInfo {
    uint32_t srcNode   = 0;
    uint32_t size      = 0;
    bool     tcp       = false;
    double   errorRate = 0.0;     // UDP only
    uint32_t sndBuf    = 0;       // TCP only (0 = ns-3 default)
    uint32_t sent      = 0;       // TCP bytes handed to the socket
    uint32_t recvBytes = 0;
    bool     sendDone  = false;
    bool     recvDone  = false;
    int64_t  startNs   = -1;      // Diplodocus start time (ns)
};
static std::map<uint32_t, IdInfo>   g_ids;          // command id -> state
static std::map<uint64_t, uint32_t> g_uidToId;      // UDP packet uid -> id
static std::map<uint16_t, uint32_t> g_portToId;     // port -> id

static void EmitResult(uint32_t id, const char* key) {
    std::cout << "RESULT_START" << std::endl;
    std::cout << "commandId=" << id << std::endl;
    std::cout << key << "=" << Simulator::Now().GetNanoSeconds() << std::endl;
    std::cout << "RESULT_END" << std::endl;
    std::cout.flush();
    g_resultCount++;
    Simulator::Stop();          // pause NS3 exactly at this result
}

static void ReportReceive(uint32_t id) {
    IdInfo& info = g_ids[id];
    if (info.recvDone) return;
    info.recvDone = true;
    std::cout << "[bridge] RECV_END id=" << id
              << " t=" << Simulator::Now().GetNanoSeconds() << std::endl;
    EmitResult(id, "receiveMeasuredLatencyNs");
}

// Remove the link-layer header (PPP or Ethernet) and, if asked, the trailer
static void StripL2(Ptr<Packet> p, bool trailer) {
    if (g_shared) {
        if (trailer) { EthernetTrailer tr; p->RemoveTrailer(tr); }
        EthernetHeader eh; p->RemoveHeader(eh);
    } else {
        PppHeader ppp; p->RemoveHeader(ppp);
    }
}

// ---------------------------------------------------------------------------
// Error model on the RECEIVING device: drops only the UDP packets of a flow
// whose command asked for errorRate > 0 (TCP data and ACKs are never touched).
// ---------------------------------------------------------------------------
class FlowErrorModel : public ErrorModel {
public:
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("FlowErrorModel")
            .SetParent<ErrorModel>()
            .AddConstructor<FlowErrorModel>();
        return tid;
    }
    FlowErrorModel() { m_rng = CreateObject<UniformRandomVariable>(); }
private:
    bool DoCorrupt(Ptr<Packet> p) override {
        Ptr<Packet> c = p->Copy();
        StripL2(c, false);
        Ipv4Header ip;
        if (c->RemoveHeader(ip) == 0 || ip.GetProtocol() != 17) return false;  // UDP only
        UdpHeader udp;
        c->RemoveHeader(udp);
        auto it = g_portToId.find(udp.GetDestinationPort());
        if (it == g_portToId.end()) return false;
        IdInfo& info = g_ids[it->second];
        if (info.tcp || info.errorRate <= 0.0) return false;
        if (m_rng->GetValue(0.0, 1.0) >= info.errorRate) return false;
        std::cout << "PACKET_LOST commandId=" << it->second
                  << " t=" << Simulator::Now().GetNanoSeconds()
                  << " (errorRate=" << info.errorRate << ")" << std::endl;
        ReportReceive(it->second);      // the receiver observes the loss now
        return true;
    }
    void DoReset() override {}
    Ptr<UniformRandomVariable> m_rng;
};

// ---------------------------------------------------------------------------
// SEND end, for UDP (packet uid) and TCP (segment carrying the last byte)
// ---------------------------------------------------------------------------
static void PhyTxEndCb(uint32_t node, Ptr<const Packet> pkt) {
    // UDP: the very packet created for this command
    auto uit = g_uidToId.find(pkt->GetUid());
    if (uit != g_uidToId.end()) {
        IdInfo& info = g_ids[uit->second];
        if (!info.tcp && !info.sendDone && info.srcNode == node) {
            info.sendDone = true;
            std::cout << "[bridge] SEND_END id=" << uit->second << " NI" << node << " UDP"
                      << " t=" << Simulator::Now().GetNanoSeconds() << std::endl;
            EmitResult(uit->second, "sendMeasuredLatencyNs");
        }
        return;
    }
    // TCP: inspect the segment
    Ptr<Packet> p = pkt->Copy();
    StripL2(p, true);
    Ipv4Header ip;
    if (p->RemoveHeader(ip) == 0 || ip.GetProtocol() != 6) return;
    TcpHeader tcp;
    p->RemoveHeader(tcp);
    uint32_t payload = p->GetSize();
    if (payload == 0) return;                                  // SYN / ACK / FIN
    auto pit = g_portToId.find(tcp.GetDestinationPort());
    if (pit == g_portToId.end()) return;
    IdInfo& info = g_ids[pit->second];
    if (!info.tcp || info.sendDone || info.srcNode != node) return;
    uint32_t lastByte = tcp.GetSequenceNumber().GetValue() + payload - 1;  // data starts at seq 1
    if (lastByte >= info.size) {
        info.sendDone = true;
        std::cout << "[bridge] SEND_END id=" << pit->second << " NI" << node << " TCP"
                  << " t=" << Simulator::Now().GetNanoSeconds() << std::endl;
        EmitResult(pit->second, "sendMeasuredLatencyNs");
    }
}

// ---------------------------------------------------------------------------
// Receive side
// ---------------------------------------------------------------------------
static void HandleRead(uint32_t id, Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from))) {
        if (packet->GetSize() == 0) break;
        IdInfo& info = g_ids[id];
        info.recvBytes += packet->GetSize();
        if (info.recvBytes >= info.size) ReportReceive(id);
    }
}

static void HandleAccept(uint32_t id, Ptr<Socket> s, const Address& from) {
    s->SetRecvCallback(MakeBoundCallback(&HandleRead, id));
}

// TCP: write as much as the send buffer accepts; continue when space frees up
// (this is the blocking write of a Diplodocus BRBW channel)
static void TcpSendMore(uint32_t id, Ptr<Socket> s, uint32_t /*available*/) {
    IdInfo& info = g_ids[id];
    while (info.sent < info.size) {
        uint32_t avail = s->GetTxAvailable();
        if (avail == 0) return;                        // buffer full: wait
        uint32_t chunk = std::min(avail, info.size - info.sent);
        int r = s->Send(Create<Packet>(chunk));
        if (r <= 0) return;
        info.sent += (uint32_t)r;
    }
}

static void TcpConnectOk(uint32_t id, Ptr<Socket> s) {
    std::cout << "[bridge] TCP connected id=" << id
              << " t=" << Simulator::Now().GetNanoSeconds() << std::endl;
    s->SetSendCallback(MakeBoundCallback(&TcpSendMore, id));
    TcpSendMore(id, s, 0);
}

static void TcpConnectFail(uint32_t id, Ptr<Socket> s) {
    std::cout << "[bridge] ERROR TCP connect failed id=" << id << std::endl;
}

static void InjectTransfer(uint32_t id, const IdInfo& cfg) {
    uint32_t    direction    = cfg.srcNode;
    Ptr<Node>   senderNode   = g_nodes.Get(direction);
    Ptr<Node>   receiverNode = g_nodes.Get(1 - direction);
    Ipv4Address receiverAddr = g_interfaces.GetAddress(1 - direction);
    uint16_t    port         = 9000 + id % 1000;

    g_ids[id] = cfg;
    g_portToId[port] = id;
    int64_t injectionTimeNs = Simulator::Now().GetNanoSeconds();

    if (cfg.tcp) {
        Ptr<Socket> listen = Socket::CreateSocket(receiverNode, TcpSocketFactory::GetTypeId());
        listen->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
        listen->Listen();
        listen->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
                                  MakeBoundCallback(&HandleAccept, id));
        Ptr<Socket> s = Socket::CreateSocket(senderNode, TcpSocketFactory::GetTypeId());
        if (cfg.sndBuf > 0) s->SetAttribute("SndBufSize", UintegerValue(cfg.sndBuf));
        s->Bind();
        s->SetConnectCallback(MakeBoundCallback(&TcpConnectOk, id),
                              MakeBoundCallback(&TcpConnectFail, id));
        s->Connect(InetSocketAddress(receiverAddr, port));
    } else {
        Ptr<Socket> r = Socket::CreateSocket(receiverNode, UdpSocketFactory::GetTypeId());
        r->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
        r->SetRecvCallback(MakeBoundCallback(&HandleRead, id));
        Ptr<Socket> s = Socket::CreateSocket(senderNode, UdpSocketFactory::GetTypeId());
        s->Connect(InetSocketAddress(receiverAddr, port));
        Ptr<Packet> packet = Create<Packet>(cfg.size);
        g_uidToId[packet->GetUid()] = id;
        s->Send(packet);
    }

    std::cout << "INJECTION_TIME commandId=" << id
              << " injectionTimeNs=" << injectionTimeNs << std::endl;
    std::cout << "[bridge] INJECT id=" << id
              << (direction == 0 ? " NI0->NI1" : " NI1->NI0")
              << " size=" << cfg.size
              << (cfg.tcp ? " TCP sndBuf=" + std::to_string(cfg.sndBuf)
                          : " UDP errorRate=" + std::to_string(cfg.errorRate))
              << " t=" << injectionTimeNs << std::endl;
    std::cout.flush();
}

static std::string Field(const std::string& msg, const std::string& key) {
    size_t p = msg.find(key + "=");
    if (p == std::string::npos) return "";
    size_t b = p + key.size() + 1, e = msg.find(';', b);
    return msg.substr(b, e == std::string::npos ? std::string::npos : e - b);
}

static void DrainTransferCommands() {
    char buf[512];
    ssize_t n;
    while ((n = recv(g_udpSock, buf, sizeof(buf) - 1, MSG_DONTWAIT)) > 0) {
        buf[n] = '\0';
        std::string msg(buf);
        IdInfo cfg;
        uint32_t id = 0;
        std::string v;
        if (!(v = Field(msg, "id")).empty())         id = std::stoul(v);
        cfg.size = 512;
        if (!(v = Field(msg, "packetSize")).empty()) cfg.size = std::stoul(v);
        if (!(v = Field(msg, "direction")).empty())  cfg.srcNode = std::min<uint32_t>(std::stoul(v), 1);
        cfg.tcp = (Field(msg, "transport") == "TCP");
        if (!(v = Field(msg, "errorRate")).empty())  cfg.errorRate = std::stod(v);
        if (!(v = Field(msg, "sndBuf")).empty())     cfg.sndBuf = std::stoul(v);
        if (!(v = Field(msg, "sndBufSize")).empty()) cfg.sndBuf = std::stoul(v);
        g_commandsReceived++;
        std::cout << "[bridge] Drained #" << g_commandsReceived
                  << " raw='" << msg << "'" << std::endl;
        if (!(v = Field(msg, "nsValue")).empty()) cfg.startNs = std::stoll(v);
        int64_t nowNs = Simulator::Now().GetNanoSeconds();
        if (cfg.startNs > nowNs) {
            Simulator::Schedule(NanoSeconds(cfg.startNs - nowNs), &InjectTransfer, id, cfg);
            std::cout << "[bridge] SCHEDULED id=" << id << " at " << cfg.startNs << " ns" << std::endl;
        } else {
            if (cfg.startNs >= 0)
                std::cout << "[bridge] LATE id=" << id << " start=" << cfg.startNs
                          << " now=" << nowNs << " (late " << (nowNs - cfg.startNs) << " ns)" << std::endl;
            InjectTransfer(id, cfg);
        }
    }
}

// Do not advance NS3 before all commands issued by Diplodocus have arrived
static void WaitForCommands(uint32_t expected) {
    auto t0 = std::chrono::steady_clock::now();
    DrainTransferCommands();
    while (g_commandsReceived < expected) {
        struct pollfd pfd = {g_udpSock, POLLIN, 0};
        poll(&pfd, 1, 50);
        DrainTransferCommands();
        if (std::chrono::steady_clock::now() - t0 > std::chrono::seconds(120)) {
            std::cout << "[bridge] GATE TIMEOUT received=" << g_commandsReceived
                      << " expected=" << expected << std::endl;
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    std::string dataRate = "1Gbps";
    double      delayMs  = 0.0001;   // 0.001 >> 1 us  //0.00001>>100 ns

    CommandLine cmd;
    cmd.AddValue("dataRate", "Link data rate", dataRate);
    cmd.AddValue("delay",    "Link propagation delay (ms)", delayMs);
    cmd.AddValue("shared",   "Use a shared (half-duplex) CSMA link", g_shared);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(1));

    g_nodes.Create(2);
    NetDeviceContainer devices;
    if (!g_shared) {
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
        p2p.SetChannelAttribute("Delay", TimeValue(Seconds(delayMs / 1000.0)));
        p2p.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("1000p"));
        devices = p2p.Install(g_nodes);
    } else {
        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue(dataRate));
        csma.SetChannelAttribute("Delay", TimeValue(Seconds(delayMs / 1000.0)));
        devices = csma.Install(g_nodes);
    }
    InternetStackHelper stack;
    stack.Install(g_nodes);
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    g_interfaces = address.Assign(devices);

    for (uint32_t i = 0; i < 2; i++) {
        devices.Get(i)->TraceConnectWithoutContext("PhyTxEnd",
            MakeBoundCallback(&PhyTxEndCb, i));
        devices.Get(i)->SetAttribute("ReceiveErrorModel",
            PointerValue(CreateObject<FlowErrorModel>()));
    }

    // Port 9999: transfer commands from Java
    g_udpSock = socket(AF_INET, SOCK_DGRAM, 0);
    int reuse = 1;
    setsockopt(g_udpSock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in addr{};
    addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9999);
    if (bind(g_udpSock, (sockaddr*)&addr, sizeof(addr)) < 0)
        std::cout << "[bridge] ERROR port 9999: " << strerror(errno) << std::endl;
    fcntl(g_udpSock, F_SETFL, fcntl(g_udpSock, F_GETFL, 0) | O_NONBLOCK);

    // Port 9998: timeslices from Diplodocus
    int udpSockTimeslice = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(udpSockTimeslice, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in addrSlice{};
    addrSlice.sin_family = AF_INET; addrSlice.sin_addr.s_addr = INADDR_ANY;
    addrSlice.sin_port = htons(9998);
    if (bind(udpSockTimeslice, (sockaddr*)&addrSlice, sizeof(addrSlice)) < 0)
        std::cout << "[bridge] ERROR port 9998: " << strerror(errno) << std::endl;

    AnimationInterface anim("/tmp/java-bridge-sim.xml");
    anim.SetConstantPosition(g_nodes.Get(0),  0.0, 0.0);
    anim.SetConstantPosition(g_nodes.Get(1), 10.0, 0.0);

    std::cout << "[bridge] NS3 ready (transport per command: UDP/TCP): NI0<->NI1 "
              << (g_shared ? "CSMA" : "P2P") << " rate=" << dataRate
              << " delay=" << delayMs << " ms" << std::endl;
    std::cout.flush();

    char buf[256];
    while (true) {
        sockaddr_in from{}; socklen_t fromLen = sizeof(from);
        ssize_t n = recvfrom(udpSockTimeslice, buf, sizeof(buf) - 1, 0,
                             (sockaddr*)&from, &fromLen);
        if (n <= 0) continue;
        buf[n] = '\0';
        std::string msg(buf);
        double   timesliceNs = 0;
        uint32_t senders     = 0;
        std::string v;
        if (!(v = Field(msg, "timesliceNs")).empty()) timesliceNs = std::stod(v);
        if (!(v = Field(msg, "senders")).empty())     senders = std::stoul(v);

        if (timesliceNs <= 0) {
            std::cout << "[bridge] Stop signal. Ending." << std::endl;
            break;
        }

        WaitForCommands(senders);

        double sliceStartNs = Simulator::Now().GetNanoSeconds();
        std::cout << "[bridge] Running NS3 for " << timesliceNs
                  << " ns (now=" << sliceStartNs << " ns, until="
                  << (sliceStartNs + timesliceNs) << " ns)" << std::endl;

        EventId eid = Simulator::Stop(NanoSeconds((uint64_t)timesliceNs));
        Simulator::Run();
        if (!eid.IsExpired()) eid.Cancel();

        std::cout << "[bridge] NS3 paused at "
                  << Simulator::Now().GetNanoSeconds() << " ns"
                  << " results=" << g_resultCount << std::endl;
        std::cout.flush();

        // reply to Diplodocus: where NS3 is and how many results it produced
        std::string ack = "now=" + std::to_string((uint64_t)Simulator::Now().GetNanoSeconds())
                        + ";results=" + std::to_string(g_resultCount) + "\n";
        sendto(udpSockTimeslice, ack.c_str(), ack.size(), 0, (sockaddr*)&from, fromLen);
    }

    Simulator::Destroy();
    close(g_udpSock);
    close(udpSockTimeslice);
    return 0;
}
