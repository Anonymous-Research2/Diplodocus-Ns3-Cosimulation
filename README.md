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
- **ns-3**: install it by following https://www.nsnam.org
- **Java** (JDK) and **Maven**, used to build the mapper.

<!-- TODO: versions used (TTool, ns-3, Java) -->

## 2. The ontologies

The three ontologies are in [`ontologies/`](ontologies/):

- **`diplodocus.owl.ttl`** describes DIPLODOCUS concepts: tasks, channels (blocking, lossy, capacity, width), their mapping on buses or networks, and network transfers.
- **`ns3.owl.ttl`** describes ns-3 concepts: packets, TCP/UDP sockets and their parameters, and transfer results.
- **`mapping.owl.ttl`** relates the two ontologies through class/property equivalences, SWRL rules (cycles ↔ ns, socket parameters) and transport axioms (lossless or blocking → TCP, lossy → UDP).

The mapper adds individuals to these ontologies at run time. Start each run from the original files.

## 3. The mapper

The mapper is in [`mapper/`](mapper/) <!-- TODO: exact path -->.
It reads the TTool model, runs the reasoner, configures ns-3 command by command, and writes the results back into the ontologies.

## 4. Run

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




