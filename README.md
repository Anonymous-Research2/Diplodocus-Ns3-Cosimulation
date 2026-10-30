# Diplodocus-Ns3-Cosimulation


# DIPLODOCUS ↔ ns-3 Semantic Bridge

An OWL-based bridge for co-simulating the **DIPLODOCUS** System-on-Chip simulator (TTool) with the **ns-3** network simulator.
DIPLODOCUS and ns-3 don't talk to each other directly. Each one works with its own ontology, and a **mapper** (with the Openllet reasoner) connects them in three phases:

---

## 1. Prerequisites

- **TTool / DIPLODOCUS**: install it by following https://ttool.telecom-paris.fr
  
  *Important*: To use the Diplodocus–NS-3 integration, download the diplodocus-ns3 branch instead of the standard TTool version.
  
- **ns-3**: install it by following https://www.nsnam.org
- **Java** (JDK) and **Maven**, used to build the mapper.

## 2. The ontologies

The three ontologies are in [`ontologies/`](ontologies/):

- **`diplodocus.owl.ttl`** 
- **`ns3.owl.ttl`**
- **`mapping.owl.ttl`** 

The mapper adds individuals to these ontologies at run time. Start each run from the original files.

## 3. NS-3 script
The script that is used to configure on runtime NS-3 is in [`ns3/`](ns3/):
It should be put in: /home/your-machine/ns-3-dev/scratch 

## 4. The mapper


The mapper is in [`mapper/`](mapper/) .
It reads the TTool model, runs the reasoner, configures ns-3 command by command, and writes the results back into the ontologies.


## 5. Run
Lauch TTool and upload an example from [`models/`](models/): Nominal example: `models/WACS_WADC_nominal.xml`, Fault example: `models/WACS_WADC_fault.xml`
Launch TTool/DIPLODOCUS simulator
Launch the mapper (DiploToNs3Mapper.java). 
Start the simulation of the model from TTool/DIPLODOCUS simulator.
Wait for results

### 6. Execution flow

1. DIPLODOCUS requests the model check → Java reasons and returns the verdict and the transport per channel.
2. A network transaction starts on an NI → it is written to the ontology → Java maps it and sends it to NS-3 → NS-3 injects it at the same time instant.
3. DIPLODOCUS and NS-3 advance together, slice by slice.
4. NS-3 produces a result → Java converts it to cycles → DIPLODOCUS ends the transaction at that cycle.
5. Java gives the hand to the user to enter their SPARQL queries for tracability and requierements check.

## 5. Results

The log of the co-simulation and the html file showing the co-simulation are in [`results/`](results/):


## Appendix: Some details of implementation of the semantic bridge and it works

The co-simulation links three parts: an extension of the **DIPLODOCUS** C++ simulator, a **Java ontology bridge**, and an **NS-3 script**. They exchange data through the ontologies.

### 1. DIPLODOCUS side (main files added or modified in TTool)**

| File | Role |
|---|---|
| `OntologyBridgeTask.cpp/.h` | Writes the network transactions into the DIPLODOCUS ontology. |
| `OntologyBridgeRunner.cpp/.h` | Launches ns-3 and synchronises it with DIPLODOCUS. |
| `TMLNetTask.cpp` | Behaviour of the networking tasks on the Network Interfaces. |
| `TMLNetCommand.cpp` | Display of the network transactions in the trace. |

### 2. Ontologies (Turtle)

| File | Content |
|---|---|
| `diplodocus.owl.ttl` | DIPLODOCUS concepts. |
| `ns3.owl.ttl` | ns-3 concepts. |
| `mapping.owl.ttl` | Correspondences between the two ontologies. |

### 3. Java bridge (Jena + Openllet)

| Class | Role |
|---|---|
| `ModelChecker.java` | Model check and ns-3 configuration before the simulation. |
| `DiploToNs3Mapper.java` | Mapping of the transfers between DIPLODOCUS and ns-3, then SPARQL queries. |
| `Ns3RunnerVisualSyncUDP.java` | Communication with ns-3. |

### 4. ns-3 script

`scratch/java-bridge-sim-Visual-RT2_SynAuto.cc` builds the network and simulates the transfers sent by the bridge.


## 5. Diplodocus ↔ NS3 synchronisation — WACS nominal model (UDP + TCP)
We provide also the table of synchronization to prove both clocks advance consistently. It takes examples of iterations from the beginning, the middle and the end of the cosimulation. 

Session: 20 network commands (10 iterations per direction), 20 simulated, 0 failed.

**Synchronisation table (1 cycle = 5 ns, router topology at 1 Gb/s)**

| Phase | Iter | Flow | Transport | Diplo start | NS-3 inject (ns) | Diplo send end | NS-3 SEND_END (ns) | Diplo recv end | NS-3 RECV_END (ns) | Sync |
|---|---:|---|---|---:|---:|---:|---:|---:|---:|:---:|
| Start | 1 | SubTxNI → CCRxNI | UDP | 20 | 100 | 145 | 724 | 290 | 1448 | ✅ |
| Start | 1 | CCTxNI → SubRxNI | TCP | 20 | 100 | 1118 | 5588 | 1250 | 6248 | ✅ |
| Start | 2 | SubTxNI → CCRxNI | UDP | 1284 | 6420 | 1461 | 7304 | 1606 | 8028 | ✅ |
| Start | 2 | CCTxNI → SubRxNI | TCP | 1153 | 5765 | 2385 | 11924 | 2517 | 12584 | ✅ |
| Middle (loss) | 7 | SubTxNI → CCRxNI | UDP | 7620 | 38100 | 7797 | 38984 | 7942 (lost) | 39708 (PACKET_LOST) | ✅ |
| Middle | 7 | CCTxNI → SubRxNI | TCP | 7492 | 37460 | 8721 | 43604 | 8853 | 44264 | ✅ |
| End | 10 | SubTxNI → CCRxNI | UDP | 11419 | 57095 | 11598 | 57992 | 11743 | 58716 | ✅ |
| End | 10 | CCTxNI → SubRxNI | TCP | 11291 | 56455 | 12522 | 62612 | 12654 | 63272 | ✅ |

*Observations*
- Every command is injected in NS3 exactly at its Diplodocus start cycle, from the first to the last iteration.
- No drift between the two clocks over the whole session.

