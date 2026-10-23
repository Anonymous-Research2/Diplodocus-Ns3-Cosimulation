# Diplodocus-Ns3-Cosimulation


# DIPLODOCUS ↔ ns-3 Semantic Bridge

An OWL-based bridge for co-simulating the **DIPLODOCUS** System-on-Chip simulator (TTool) with the **ns-3** network simulator.
DIPLODOCUS and ns-3 don't talk to each other directly. Each one works with its own ontology, and a **mapper** (with the Openllet reasoner) connects them in three phases:

| Phase | What the bridge does |
|---|---|
| **I. Before the simulation** | Checks that the model can be co-simulated faithfully and derives the ns-3 configuration (TCP/UDP and their parameters) from the channel semantics. |
| **II. During the simulation** | Translates every network transfer (size, cycles ↔ ns) between DIPLODOCUS and ns-3. |
| **III. After the simulation** | Keeps the results linked to the model, so that requirements can be checked with SPARQL queries. |

---

## Repository content

```
.
├── models/
│   ├── WACS_WADC_nominal.xml     # nominal case study (co-simulable)
│   └── WACS_WADC_fault.xml       # fault case study (rejected before cycle 0)
├── ontologies/
│   ├── diplodocus.owl.ttl        # DIPLODOCUS ontology
│   ├── ns3.owl.ttl               # ns-3 ontology
│   └── mapping.owl.ttl           # mapping ontology (equivalences, SWRL rules, transport axioms)
├── mapper/                       # <!-- path / description of the mapper (Java, Maven) -->
├── ns3/                          # <!-- path of the ns-3 script (SynAuto.cc) -->
├── results/
│   ├── nominal/                  # <!--  console output + DIPLODOCUS trace (.html) -->
│   └── fault/                    # <!-- console output -->
└── Makefile
```

---

## 1. Prerequisites

- **TTool / DIPLODOCUS**: install it by following https://ttool.telecom-paris.fr
  
  *Important*: To use the Diplodocus–NS-3 integration, download the diplodocus-ns3 branch instead of the standard TTool version.
  
- **ns-3**: install it by following https://www.nsnam.org
- **Java** (JDK) and **Maven**, used to build the mapper.

<!-- TODO: versions used (TTool, ns-3, Java) -->

## 2. The ontologies

The three ontologies are in [`ontologies/`](ontologies/):

- **`diplodocus.owl.ttl`** describes DIPLODOCUS concepts: tasks, channels (blocking, lossy, capacity, width), their mapping on buses or networks, and network transfers.
- **`ns3.owl.ttl`** describes ns-3 concepts: packets, TCP/UDP sockets and their parameters, and transfer results.
- **`mapping.owl.ttl`** relates the two ontologies through class/property equivalences, SWRL rules (cycles ↔ ns, socket parameters) and transport axioms (lossless or blocking → TCP, lossy → UDP).

The mapper adds individuals to these ontologies at run time. Start each run from the original files.

## 3. NS-3 script
The script that is used to configure on runtime NS-3 is in [`ns3/`](ns3/):
It should be put in: /home/your-machine/ns-3-dev/scratch 

## 4. The mapper


The mapper is in [`mapper/`](mapper/) <!-- TODO: exact path -->.
It reads the TTool model, runs the reasoner, configures ns-3 command by command, and writes the results back into the ontologies.


## Implementation of the semantic bridge

The co-simulation links three parts: an extension of the **DIPLODOCUS** C++ simulator, a **Java ontology bridge**, and an **NS-3 script**. They exchange data through the ontologies, and two local UDP ports.

### 1. DIPLODOCUS extension (C++)

| File | Role |
|---|---|
| `OntologyBridgeTask.cpp/.h` | Called when a network transaction starts on an NI. A write is added to `diplodocus.owl.ttl` as a `WriteChannelCommand` individual (data size + start cycle); a read records its start cycle. |
| `OntologyBridgeRunner.cpp/.h` | Replaces the default NS-3 runner. It launches NS-3 and requests the start-up model check. It drives the time synchronisation (one time slice per DIPLODOCUS transaction, or NS-3 runs until its next result when no transaction is left). It reads the results and sets the end of each network transaction. |
| `TMLNetTask.cpp` | Single-core NI rules: one network operation at a time on an NI, and a read starts only once its data has been sent. |
| `TMLNetCommand.cpp` | Labels network transactions `Nw` / `Nr` in the HTML trace. |

### 2. Ontologies (Turtle)

| File | Content |
|---|---|
| `diplodocus.owl.ttl` | DIPLODOCUS concepts: channels (blocking, lossy), network tasks, data size, cycle time points, master clock. Receives the network transactions at run time. |
| `ns3.owl.ttl` | NS-3 concepts: packets, sockets (`UdpSocket`, `TcpSocket`), time points, transfer results. |
| `mapping.owl.ttl` | Equivalences between the two ontologies, axioms choosing UDP or TCP for each channel, and SWRL rules: cycle ↔ ns conversion, `errorRate`, `sndBufSize`. |

### 3. Java bridge (Jena + Openllet)

| Class | Role |
|---|---|
| `ModelChecker.java` | Before cycle 0: reads the TTool model, adds its NI channels to the ontology and runs the reasoner. If the model is inconsistent, it gives an explanation and DIPLODOCUS stops. Otherwise it fixes the transport per channel (UDP + `errorRate`, or TCP + `sndBufSize`). |
| `DiploToNs3Mapper3PelletJenaOldRT2D2cl2SyncUDP.java` | Watches `diplodocus.owl.ttl`. For each new transaction, it maps it to NS-3 (packet size, start time in ns) and sends it to NS-3. It writes each measured latency, converted to cycles, back to the ontologies and to `net_done.sidechannel`. |
| `Ns3RunnerVisualSyncUDP.java` | Sends the commands to NS-3 (`packetSize`, `nsValue`, `direction`, `transport`) and reads the NS-3 results. |

### 4. NS-3 script

`scratch/java-bridge-sim-Visual-RT2_SynAuto.cc` simulates a 1 Gbps point-to-point link between NI0 and NI1; a shared CSMA link is optional. It:
- injects each transfer at its DIPLODOCUS start time, over UDP (with a loss model) or TCP (with a send buffer);
- measures the send end and the receive end;
- stops at each result, and answers every time slice with its current time.

### Execution flow

1. DIPLODOCUS requests the model check → Java reasons and returns the verdict and the transport per channel.
2. A network transaction starts on an NI → it is written to the ontology → Java maps it and sends it to NS-3 → NS-3 injects it at the same time instant.
3. DIPLODOCUS and NS-3 advance together, slice by slice.
4. NS-3 produces a result → Java converts it to cycles → DIPLODOCUS ends the transaction at that cycle.
5. Java gives the hand to the user to enter their SPARQL queries for tracability and requierements check.












## 5. Run

```bash
make demo
```

<!-- TODO: Makefile targets (e.g. make nominal / make fault) -->

Then start the simulation of the model from TTool (DIPLODOCUS simulator).

### Nominal example: `models/WACS_WADC_nominal.xml`
- The measurement channel is non-blocking and lossy (2 %), so it gets **UDP** with `errorRate = 0.02`.
- The command channel is blocking and lossless, so it gets **TCP** with `sndBufSize = 32` bytes.
- The co-simulation runs, and both flows are transferred concurrently in a single ns-3 session.

### Fault example: `models/WACS_WADC_fault.xml`
- The measurement channel is **blocking and lossy**. TTool accepts this model, but no single transport can reproduce it.
- The reasoner reports the ontology as **inconsistent** and explains the conflict. DIPLODOCUS stops before cycle 0 and ns-3 is not started.

## 5. Results

The log of the co-simulation and the html file showing the co-simulation is in [`results/`](results/):

## Diplodocus ↔ NS3 synchronisation — WACS nominal model (UDP + TCP)
We provide also the table of synchronization to prove both clocks advance consistently. 

The table below covers 6 of the 10 iterations: the start, the middle and the end of a complete session of 20 network commands, all of which completed (none failed).

Session: 20 network commands (10 iterations per direction), 20 simulated, 0 failed.

| Phase | Iter | Command | Direction | Transport | Diplo start (cycle) | NS3 inject (ns) | Diplo send end (cycle) | NS3 SEND_END (ns) | Diplo recv end (cycle) | NS3 RECV_END (ns) | Sync |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Start | 1 | SubTxNI → CCRxNI | NI0 → NI1 | UDP | 20 | 100 | 145 | 724 | 165 | 824 | ✅ |
| Start | 1 | CCTxNI → SubRxNI | NI1 → NI0 | TCP | 20 | 100 | 720 | 3600 | 740 | 3700 | ✅ |
| Start | 2 | SubTxNI → CCRxNI | NI0 → NI1 | UDP | 774 | 3870 | 951 | 4756 | 971 | 4856 | ✅ |
| Start | 2 | CCTxNI → SubRxNI | NI1 → NI0 | TCP | 755 | 3775 | 1526 | 7632 | 1546 | 7732 | ✅ |
| Middle | 5 | SubTxNI → CCRxNI | NI0 → NI1 | UDP | 3193 | 15965 | 3370 | 16852 | 3390 | 16952 | ✅ |
| Middle | 5 | CCTxNI → SubRxNI | NI1 → NI0 | TCP | 3177 | 15885 | 3946 | 19728 | 3966 | 19828 | ✅ |
| Middle | 6 | SubTxNI → CCRxNI | NI0 → NI1 | UDP | 4000 | 20000 | 4177 | 20884 | 4197 | 20984 | ✅ |
| Middle | 6 | CCTxNI → SubRxNI | NI1 → NI0 | TCP | 3984 | 19920 | 4752 | 23760 | 4772 | 23860 | ✅ |
| End | 9 | SubTxNI → CCRxNI | NI0 → NI1 | UDP | 6419 | 32095 | 6596 | 32980 | 6616 | 33080 | ✅ |
| End | 9 | CCTxNI → SubRxNI | NI1 → NI0 | TCP | 6403 | 32015 | 7171 | 35856 | 7191 | 35956 | ✅ |
| End | 10 | SubTxNI → CCRxNI | NI0 → NI1 | UDP | 7223 | 36115 | 7402 | 37012 | 7422 | 37112 | ✅ |
| End | 10 | CCTxNI → SubRxNI | NI1 → NI0 | TCP | 7207 | 36035 | 7978 | 39888 | 7998 | 39988 | ✅ |

*Observations*
- Every command is injected in NS3 exactly at its Diplodocus start cycle, from the first to the last iteration.
- No drift between the two clocks over the whole session.
- Transports selected by the start-up ontology check: UDP (errorRate 0.02) for the lossy measurement channel, TCP (sndBufSize 32) for the blocking command channel.

