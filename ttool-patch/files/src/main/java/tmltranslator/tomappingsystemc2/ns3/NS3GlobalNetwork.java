/* Copyright or (C) or Copr. GET / ENST, Telecom-Paris, Ludovic Apvrille
 *
 * ludovic.apvrille AT enst.fr
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
 */

package tmltranslator.tomappingsystemc2.ns3;
import tmltranslator.*;
import myutil.TraceManager;
import tmltranslator.tomappingsystemc2.NamesGenerationHelper;

import java.util.*;
import static tmltranslator.tomappingsystemc2.ns3.IPv4Address.makeIPv4Address;

/**
 *
 *
 * NS3 script code generator, entry point
 *
 * <p>This class implements the ns3 script generation process described
 * in the {@link tmltranslator.tomappingsystemc2.ns3 ns3 package}.</p>
 * <ul><li>
 *     {@link #getNs3Includes getNs3Includes} returns {@link #ns3Includes ns3Includes}, the network
 *     dedicated include directives, that
 *     must be put before
 *     the ones of diplodocus.
 * </li></ul>
 * <h2> Producing the build-architecture part of the ns3 script </h2>
 * <p><a id="constructor"></a>
 * The {@link #NS3GlobalNetwork constructor} computes the part of the script that is dedicated
 * to the creation of the network architecture and configuration. It successively calls the
 * four methods:
 * <ol>
 *     <li><a id="analyze"></a>
 *         {@link #analyzeNetwork analyzeNetwork} collects the pieces of code associated with the network information of the Diplodocus
 *         instance model, in a structured way. The following global attributes are updated, where {@link NS3GlobalNetwork#ns3MainNodes
 *         ns3MainNodes} and {@link NS3GlobalNetwork#ns3LocallyCreatedFeatures ns3AutoNodes} contain all the {@link NS3Node NS3Node}s of the architecture.
 *         <ul>
 *             <li>
 *                 {@link NS3GlobalNetwork#ns3MainNodes s3MainNodes} the nodes that are directly created in their declaration
 *                 at the toplevel of the script. Currently, they have no dependency and will thus be simply declared at the beginning of the script.
 *                 script.
 *             </li><li>
 *                 {@link NS3GlobalNetwork#ns3LocallyCreatedFeatures ns3AutoNodes} the nodes that created while building another feature of the global
 *                 network. For example, when building a network, some ns3 helpers automatically create some nodes inside this network. The
 *                 creating code of these nodes is associated to a specific network, thus memorized inside {@link NS3Subnet NS3Subnet}s structures.
 *             </li><li>
 *                 {@link NS3GlobalNetwork#subNets subNets} the networks, which contain the list of their specific {@link NS3CodePiece
 *                 NS3CodePiece}s. At this stage, each {@link NS3CodePiece
 *                 NS3CodePiece} exports
 *                 <ul>
 *                     <li>
 *                         {@link NS3CodePiece#createdFeatures NS3CodePiece.createdNodes}: the list of nodes it creates (any code that depends
 *                         on them must occur after the code the {@link NS3CodePiece NS3CodePiece} itself in the script)
 *                     </li><li>
 *                         {@link NS3CodePiece#creationDependency NS3CodePiece.nodeDependency}: its indirect dependencies, the list of node it
 *                         depends on (their creation code must occur before).
 *                     </li><li>
 *                         {@link NS3CodePiece#ctrlDependency NS3CodePiece.ctrlDependency}: the direct dependency, i.e. the list of
 *                         {@link NS3CodePiece
 *                 NS3CodePiece}s that must be written before. At this moment, it only reflect dependencies that are internal to the network.
 *                 It will be later completed by following indirect dependencies.
 *                     </li>
 *                 </ul>
 *             </li><li>
 *                 {@link NS3GlobalNetwork#niMap niMap} the link between Diplodocus {@link tmltranslator.HwNetworkInterface} and their
 *                 associated {@link NS3NodeNetworkInterface NS3NodeNetworkInterface}s. ({@link NS3Node NS3Node}s may also be
 *                 {@link NS3NodeRouter}, whose linking with Diplodocus original features is useless in the sequel)
 *             </li>
 *         </ul>
 *     </li><li><a id="initialize"></a>
 *         {@link #collectAndInitializeSubnetCodes collectAndInitializeSubnetCodes} prepares the ordering algorithm of next step:
 *         <ul>
 *             <li> {@link #structStates structStates}, the attribute containing the state of this algorithm (seen as a statemachine) is filled and
 *             initialized. It represents a set containing one {@link NS3CodePiece.StructureSubState NS3CodePiece.StructureSubState} per {@link NS3Subnet
 *             NS3Subnet} in {@link #subNets}, which maintains for each network:
 *                 <ul>
 *                     <li>{@link NS3CodePiece.StructureSubState#allCodes NS3CodePiece.StructureSubState.allCodes}:
 *                         the set of {@link NS3CodePiece NS3CodePiece}s to print in the script (the one obtained from its associated
 *                         {@link NS3Subnet NS3Subnet}).
 *                     </li>
 *                     <li>{@link NS3CodePiece.StructureSubState#readyCodes NS3CodePiece.StructureSubState.readyCodes}:
 *                         the set of {@link NS3CodePiece NS3CodePiece}s that are ready to print as all their dependencies
 *                         are satisfied, and they have not yet been handled.
 *                     </li><li>
 *                         {@link NS3CodePiece#preceding NS3CodePiece.preceding}: the number of backward
 *                         dependencies that are still pending.
 *                     </li>
 *                 </ul>
 *                 By the way, dependency information is developped:
 *                 <ul>
 *                     <li>
 *                         {@link NS3CodePiece#ctrlDependency NS3CodePiece.ctrlDependency} direct dependencies of each {@link NS3CodePiece
 *                         NS3CodePiece} are completed with the creating  {@link NS3CodePiece NS3CodePiece}s of the nodes they depend on
 *                         through {@link NS3CodePiece#ctrlDependency S3CodePiece.nodeDependency}. <br>
 *                         These dependencies are called "backward", as they contain the pieces that must be "before".
 *                     </li><li>
 *                         {@link NS3CodePiece#following following}, the attribute dedicated to the symmetric dependencies is filled.<br>
 *                         These dependencies are called "forward", as they contain the pieces that must be "after".
 *                     </li>
 *                 </ul>
 *             </li><li>
 *                 Fills {@link #featureLocalCreationCode autoNodeCreationCode}, the relation between automatic nodes and their unique creating pieces
 *                 of code.
 *             </li>
 *         </ul>
 *     </li> <li><a id="order"></a>
 *         {@link #linearize linearize} is the ordering algorithm. It tries to keep pieces of a same network as much grouped as possible, for
 *         readability of the result.
 *         <ul><li>
 *             {@link #subnetScriptCodes subnetScriptCodes} is the attribute that memorizes the result: a well-ordered list of {@link NS3CodePiece
 *             NS3CodePiece}s.
 *         </li></ul>
 *
 *     </li><li><a id="print"></a>
 *         {@link #buildNS3NetworkScript buildNS3NetworkScript} which generates the architectural part of the ns3 script.
 *         <ul><li> {@link #ns3NetworkScript ns3NetworkScript} is filled. This attribute then contains the final string text
 *         of the architecture script, build  browsing {@link #subnetScriptCodes subnetScriptCodes}. <br>
 *          The {@link #NS3GlobalNetwork constructor} computes the part of the script that is dedicated
 * to the creation of the network architecture and configuration. It successively calls the
 * four methods:</li></ul>
 *     </li>
 * </ol>
 *
 * <p> {@link #getNs3NetworkScript getNs3NetworkScript} provides access to the generated architectural script, as soon as the class constructor
 * has run. thiece script part must be put (in <code>appmodel.cpp</code> generated file) after the Diplodocus declarations
 * and before the declarations associated to the ns3 tasks.</p>
 *
 * <h2> Producing the task part of the ns3 script </h2>
 * <p> The process takes as input a list of {@link NS3NetTask NS3NetTask}s associated with Diplodocus networking tasks to generate
 * the task part script.
 * </p>
 * <ul>
 *     <li>
 *         {@link NS3NetTask NS3NetTask}s is a structure that contains the minimal information to install a task.
 *     </li>
 *     <li>
 *         {@link #getNs3TasksScript getNs3TasksScrip}  builds the script installing the tasks relying on
 *         <ul>
 *             <li>
 *                 the attribute {@link #niMap niMap} (filled by constructor) to identify
 *                 {@link NS3Node NS3Node}s associated to Diplodocus
 *                 {@link HwNetworkInterface HwNetworkInterface}s.
 *             </li><li>
 *                 the list containing the {@link NS3NetTask NS3NetTask}s associated with the diplodocus networking
 *                 tasks of the instance model (build in the caller
 *                 {@link tmltranslator.tomappingsystemc2.DiploSimulatorCodeGenerator#generateSystemC(boolean, boolean, String, String, boolean)
 *                 DiploSimulatorCodeGenerator.generateSystemC}).
 *             </li>
 *         </ul>
 *     </li>
 * </ul>
 * Currently, the process also generates the call to Ipv4GlobalRoutingHelper::PopulateRoutingTables() to
 * easily obtain a centralized static routing.
 *
 *
 * @author Sophie Coudert
 * @version 0.1 18/06/2025
 */


public class NS3GlobalNetwork {
    /** time per cycle (w.r.t. global clock of the diplodocus model) in picoseconds */
    private int timePerCycle;
    /** part of the ns3 script that builds the network structure */
    private String ns3NetworkScript;
    /** <code>#include</code> directives part of the ns3 script, to put before Diplodocus ones */
    
    //the ontology bridge modifivcation includes
    private String ns3Includes =
            "//Includes for Ontology Bridge\n" +
            "#include <OntologyBridgeTask.h>\n" +
            "#include <OntologyBridgeRunner.h>\n" +
            "//End of Ontology Bridge includes\n";

    /**
     * Constructor, which fills {@link #timePerCycle timePerCycle} and {@link #ns3NetworkScript ns3NetworkScript} and collects information useful
     * for the generation of the task dedicated script part
     * <dl>
     *     <dt><span class="paramLabel">Algorithm:</span></dt>
     *     <dd>
     *         <ul>
     *             <li>
     *                 initialize the {@link #timePerCycle timePerCycle} attribute and call successively
     *                 {@link #analyzeNetwork analyzeNetwork},
     *                 {@link #collectAndInitializeSubnetCodes collectAndInitializeSubnetCodes}, {@link #linearize linearize} and
     *                 {@link #buildNS3NetworkScript buildNS3NetworkScript}.
     *             </li>
     *        </ul>
     *     </dd>
     * </dl>
     * @param links links between networks and nodes, i.e. couple ({@link tmltranslator.Network Network},
     * {@link tmltranslator.NetworkNode NetworkNode}) provided with a name and a bandwidth
     * @param _timePerCycle Time per cycle (w.r.t. global clock of the diplodocus model) in picoseconds
     */
    public NS3GlobalNetwork(List<NetLink> links, int _timePerCycle) {
        timePerCycle = _timePerCycle;
        analyzeNetwork(links);
        if (!collectAndInitializeSubnetCodes()) return;
        if (!linearize()) return;
        buildNS3NetworkScript();
    }

    /**
     * Get the architecture dedicated part of the final ns3 script.
     *
     * Available immediately after creation.
     * @return the text of the script's architecture part
     */
    public String getNs3NetworkScript() { return ns3NetworkScript; }

    /**
     * Get the ns3 specific includes directives to put first, in <code>appmodel.cpp</code>.
     * @return the text of the includes directives
     */
    public String getNs3Includes() { return ns3Includes; }

    // PURE INTERNAL ALGORITHMIURE SCRIPT (all is private until handling of tasks) ====================================

    // Structures for algorithmic intermediate results -------------------------------------
    /** All subnets (networks in the whole model), Filled by {@link #analyzeNetwork  analyzeNetwork}*/
    private HashSet<NS3Subnet> subNets;
    /** All not automatic nodes (not created by other components), Filled by {@link #analyzeNetwork  analyzeNetwork} */
    private HashSet<NS3Node> ns3MainNodes; // TODO future: generalize to all relevant ns3 components
    /** All locally created features (automatically created while creating some other component), Filled by {@link #analyzeNetwork  analyzeNetwork} */
    private HashSet<NS3Feature> ns3LocallyCreatedFeatures; // TODO future: generalize to all relevant ns3 components
    /** (injective) Association of NS3 nodes to Network interface execution nodes (used to map ns3 tasks), Filled by {@link #analyzeNetwork
     * analyzeNetwork} */
    private HashMap<HwNetworkInterface,NS3NodeNetworkInterface> niMap;
    /** Associates creation code to automatic nodes: each automatic node will have exactly one creating code piece,
     * Filled by {@link #collectAndInitializeSubnetCodes collectAndInitializeSubnetCodes}*/
    private HashMap<NS3Feature, NS3CodePiece> featureLocalCreationCode; // TODO future: generalize to all relevant ns3 components
    /** All pieces of codes of subnetworks, grouped by subnetwork, This structure also implement a state on which the
     *  code-pieces ordering algorithm {@link #linearize linearize} relies, Filled and initialized by
     *  {@link #collectAndInitializeSubnetCodes collectAndInitializeSubnetCodes}, used by {@link #linearize linearize}  */
    private ArrayList<NS3CodePiece.StructureSubState> structStates;
    /** All pieces of codes of subnetworks, ordered respecting dependencies, Filled by {@link #linearize linearize}  */
    private List<NS3CodePiece> subnetScriptCodes;

    // methods for successive algorithmic steps ----------------------------------------

    /** Computes {@link #subNets subNets}, {@link #ns3MainNodes ns3MainNodes}, {@link #ns3LocallyCreatedFeatures ns3AutoNodes} and {@link #niMap niMap},
     * described <a href="#analyze">above</a>.
     * <br>
     * <dl>
     *     <dt><span class="paramLabel">Algorithm:</span></dt>
     *     <dd>
     *         <ol>
     *             <li>
     *                 Create all the {@link NS3Node NS3Node}s and {@link NS3Subnet NS3Subnet}s from the {@link NetLink NetLinks} provided
     *                 as parameters while filling {@link #niMap niMap}. Created {@link NS3Node NS3Node}s may be
     *                 {@link NS3NodeNetworkInterface NS3NodeNetworkInterface}s or {@link NS3NodeRouter NS3NodeRouter}s.
     *             </li><li>
     *                 Allocates network addresses to created networks while filling {@link #subNets subNets}. For this,
     *                 <ol>
     *                     <li>
     *                         Calls {@link IPv4Address#makeIPv4Address IPv4Address.makeIPv4Address} which creates address values from string to
     *                         create the first (i.e. lowest) network address and the only network mask used. Both values are fixed in this code
     *
     *                     </li><li>
     *                         Iteratively calls {@link IPv4Address#makeIPv4Address IPv4Address.makeIPv4Address} on networks, which makes the
     *                         current network memorizes the addresses it needs and returns the next available network address.
     *                     </li>
     *                 </ol>
     *             </li><li>
     *                 Fill {@link #ns3MainNodes ns3MainNode}s and {@link #ns3LocallyCreatedFeatures ns3AutoNode}s by sorting the created {@link NS3Node NS3Node}s.
     *             </li>
     *        </ol>
     *     </dd>
     * </dl>
     * @param links network information from the diplodocus model, i.e. links between networks and nodes which are couple
     *              ({@link tmltranslator.Network
     * Network}. This provides all the networks and nodes with their connections and configurations.
     * **/
    private void analyzeNetwork(List<NetLink> links) {
        niMap = new HashMap<>();

        //=============================================
        // Browsing links to collect generation dedicated NS3Nodes and NS3Subnets.
        // Diplodocus NetworkNodes and Networks may appear in several links, then...
        // Associated generation dedicated structures are created once, and potentially later re-accessed, recovered

        HashMap<NetworkNode,NS3Node> ndMap = new HashMap<>(); // to recover yet created  NS3Nodes
        HashMap<Network,NS3Subnet> ntMap = new HashMap<>(); // to recover yet created NS3Subnets
        NS3Subnet net; // current created/recovered network while browsing links
        NS3Node node;  // current created/recovered node while browsing links

        for (NetLink link : links) { // browsing (...
            // get current NS3Subnet from link, create it if relevant
            net = ntMap.get(link.net);
            if (net == null) {
                net = new NS3SubnetCSMA(link.net); // TODO: future -> several kinds of network
                ntMap.put(link.net, net);
            }

            // get current NS3Node from link, create it if relevant.
            // Currently, there are two kinds of NS3Nodes: network interfaces and routers.
            // The linking between Diplodocus routers and NS3 routers is useless in the sequel, thus not memorized.
            node = ndMap.get(link.node);
            if (node == null) {
                if (link.node instanceof NetworkInterfaceNode) {
                    node = new NS3NodeNetworkInterface((NetworkInterfaceNode) link.node);
                    niMap.put(((NetworkInterfaceNode) link.node).hwni, (NS3NodeNetworkInterface) node);
                } else if (link.node instanceof NetworkRouter)
                    node = new NS3NodeRouter(((NetworkRouter) link.node));
                ndMap.put(link.node, node);
            }

            // add current NS3Node to current NS3Subnet
            net.addNode(node, null); // TODO: future -> second argument for potential link options
        }

        //=============================================
        // Allocates network addresses to created networks while filling subNets

        IPv4Address netAddress = makeIPv4Address("10.0.0.0"); // the first network address
        IPv4Address netMask = makeIPv4Address("255.255.0.0"); // the only network mask used
        subNets = new HashSet<NS3Subnet>();
        for (NS3Subnet subnet : ntMap.values()) {
            // subnet reserves and memorizes the addresses it needs and returns the next available network address
            netAddress = subnet.takeNetworkAddresses(netAddress, netMask);
            subnet.close(); // finalization: updates at least subnet's automatic node information
            subNets.add(subnet);
        }

        //=============================================
        // Sorts created NS3Nodes to fill ns3MainNodes and ns3AutoNodes
         // TODO future: handle generalization to all relevant ns3 components
        ns3MainNodes = new HashSet<NS3Node>();
        ns3LocallyCreatedFeatures = new HashSet<NS3Feature>();
        for (NS3Node nd : ndMap.values()) {
            if (nd.isLocallyCreated())
                ns3LocallyCreatedFeatures.add(nd);
            else
                ns3MainNodes.add(nd);
        }
    }

    /** Builds an initial state in the dedicated {@link #structStates structStates} attribute for the ordering in next step and
     * fills {@link #featureLocalCreationCode autoNodeCreationCode}, as explained <a href="#initialize">above</a>.
     *
     * States are distributed, made of one substate  per subnetwork in {@link #subNets subNets}.
     * <ul>
     *     <li>
     *         {@link NS3CodePiece.StructureSubState} is the class of the local states associated with networks
     *     </li><li>
     *         {@link #structStates structStates}, filled here, is the attribute containing the global state made of one
     *         {@link NS3CodePiece.StructureSubState} per subnetwork.
     *     </li>
     * </ul>
     * By the way:
     * <ol>
     *     <li>
     *         Coherency: verifies that each automatic node has exactly one associated creating piece of code and that each node created by a
     *         piece of code is automatic.
     *     </li><li>
     *         Completes dependency information between {@link NS3CodePiece NS3CodePiece}s (memorized inside
     * {@link NS3CodePiece NS3CodePiece}s)
     *     </li><li>
     *         Initializes code pieces to make them ready for linearization (i.e.  next algorithmic step)
     *     </li>
     * </ol>
      * <dl>
     *     <dt><span class="paramLabel">Algorithm:</span></dt>
     *     <dd>
     *         <ol>
     *             <li>
     *                 Browse {@link NS3Subnet NS3Subnet}s in {@link #subNets subNets} to create their associated
     *                 {@link NS3CodePiece.StructureSubState  StructureSubState} containing the same {@link NS3CodePiece NS3CodePiece}s,
     *                 and so fill {@link #structStates structStates}. <br>
     *                 On this occasion, all network specific {@link NS3CodePiece NS3CodePiece}s are browsed and
     *                 <ol>
     *                     <li>
     *                         We verify that all nodes they create are automatic nodes, and each automatic node has at most one creating
     *                         {@link NS3CodePiece NS3CodePiece}.
     *                     </li><li>
     *                         {@link NS3CodePiece NS3CodePiece}s are added to the forward dependencies (pieces that must be "after")
     *                         of their direct backward dependencies (pieces that must be "before").
     *                     </li>
     *                 </ol>
     *             </li><li>
     *                 Verify that all automatic nodes in {@link #ns3LocallyCreatedFeatures ns3AutoNodes} have got an associated creating
     *                 {@link NS3CodePiece NS3CodePiece} in {@link #featureLocalCreationCode autoNodeCreationCode} during the previous step.
     *             </li><li>
     *                 Fill {@link #ns3MainNodes ns3MainNode}s and {@link #ns3LocallyCreatedFeatures ns3AutoNode}s by sorting the created {@link NS3Node NS3Node}s.
     *             </li><li>
     *                 Complete the opérational dépendency information in each {@link NS3CodePiece NS3CodePiece}
     *                 ({@link NS3CodePiece#preceding .preceding}, {@link NS3CodePiece#following .following})
     *             </li>
     *        </ol>
     *     </dd>
     * </dl>
    * @return false if some automatic node has not exactly one associated creating piece of code. Otherwise, returns true.
     */
    private boolean collectAndInitializeSubnetCodes() {
        featureLocalCreationCode = new HashMap<>();
        structStates = new ArrayList<>();

        //=============================================
        // Browsing NS3Subnets to create their associated NS3CodePieces in structStates, while
        //   - Verifying the one-to-one relation between automatic nodes and node creating code pieces exported by NS3Subnets
        //   - Register forward control dependency (internal to subnet)

        for (NS3Subnet nt : subNets) { // Browsing NS3Subnets
            List<NS3CodePiece> codes = nt.getCodes();
            for (NS3CodePiece cod : codes) { // Browsing their NS3CodePieces
                // complete nodeLinkings with the 'automatic node - creating code piece' relation the NS3CodePiece provides
                for (NS3Feature ft : cod.createdFeatures) {  // Browsing the nodes created by the NS3CodePiece
                    // First, Verify that each node created by such a NS3CodePiece
                    // - has been identified as a locally created node
                    if (!(ns3LocallyCreatedFeatures.contains(ft))) {
                        TraceManager.addDev("In network " + nt.getName() + ", " +
                                "Feature " + ft.getName() + " has local creating code but no creation name");
                        return false;
                    }
                    // - has not already been provided with a creating code
                    if (featureLocalCreationCode.get(ft) != null) {
                        TraceManager.addDev("In network " + nt.getName() + ", "  + "Feature " + ft.getName() +
                                " cannot be created twice");
                        return false;
                    }
                    // Then, add the 'automatic node - creating code piece' to nodeLinkings
                    else featureLocalCreationCode.put(ft, cod);
                }
                // add the NS3CodePiece to the forward dependencies of each of its direct backward dependencies.
                // (indirect dependency, through used features, can only be handled with an up-to-date
                // featureLocalCreationCode, thus not yet)
                for (NS3CodePiece c : cod.ctrlDependency) c.following.add(cod);
            }
            // fill structStates with created StructureSubState that containes the same NS3CodePieces as their associated NS3Subnets
            structStates.add(new NS3CodePiece.StructureSubState(codes, nt));
        }

        //=============================================
        // verify that all automatic nodes have an associated creating code (TODO: modify "node" anywhere where relevant...)

        // nodes having an associated creating code
        Set<NS3Feature> locallyCreatedFeatures = featureLocalCreationCode.keySet();

        boolean error = false;
        for (NS3Feature ft : ns3LocallyCreatedFeatures) // Browse automatic nodes
            if (!(locallyCreatedFeatures.contains(ft))) { // verify that the node has a creating NS3CodePiece
                TraceManager.addDev("NS3 locally created feature  " + ft.getOriginalName() + " has no associated creating code");
                error = true;
            }
        if (error) return false;

        //=============================================
        // Update dependency through used automatic features (TODO: modify "node")

        // browse all NS3CodePieces of all structures and...
        for (NS3CodePiece.StructureSubState structState : structStates) {
            for (NS3CodePiece code : structState.allCodes) {
                for (NS3Feature depFT : code.creationDependency)
                    if (ns3LocallyCreatedFeatures.contains(depFT)) {  // for each automatic node it depends on,
                        NS3CodePiece creationCode = featureLocalCreationCode.get(depFT); // get the creating NS3CodePiece of this automatic node
                        if (!creationCode.following.contains(code)) {//.... + add the code as forward dependency to the linking code of the
                            creationCode.following.add(code); //............  automatic node if this has not been already done (avoids duplicates)
                            code.preceding++;             //............... + updae backward dependency
                        }
                    }
            }
            //=============================================
            // Initialise all StructureSubState of all networks
            structState.init();
        }
        return true;
    }

    /** Build in {@link #subnetScriptCodes subnetScriptCodes} an ordered list containing all pieces of codes of all subnets, as
     * explained <a href="#order">above</a>.
     *
     * The list respects dependency order, if possible (i.e., if there is no cycle).
     * <br>
     * Notice that in the future, the dependency mechanism will have to be extended to pieces of code at main level
      * <dl>
     *     <dt><span class="paramLabel">Algorithm:</span></dt>
     *     <dd><p><i>Roughly speaking</i></p>
     *         The principe is to start with an empty {@link #subnetScriptCodes subnetScriptCodes} and successively "write" all the code pieces
     *         in it, i.e., add them in {@link #subnetScriptCodes subnetScriptCodes} in a relevant order. This is done by a loop that
     *         <ol>
     *             <li>
     *                 Search in the current state {@link #structStates structStates} a piece of code that is ready to write (w.r.t
     *                 {@link NS3CodePiece.StructureSubState#readyCodes
     *                 StructureSubState.readyCodes})
     *              </li><li>
     *                  Write it (add it to {@link #subnetScriptCodes subnetScriptCodes})
     *              </li><li>
     *                  Update the state by updating the {@link NS3CodePiece.StructureSubState#readyCodes
     *                 StructureSubState.readyCodes}) of all networks that are impacted by the writing of this piece.
     *              </li>
     *         </ol>
     *         The loop completes when there is no more ready code. We then verify that all code pieces have been written to ensure that there is no
     *         dependency loop.<br>
     *         <i>Cosmetic:</i> A "current network" variable memorizes the network of the piece being currently written. The next ready piece is
     *         selected in this network, if possible, and so the generated
     *         code is more readable thanks to some grouping by network.
     *         <p><i>More precise algorithm</i></p>
     *
     *         <ol>
     *             <li>
     *                 Loop:
     *                 <ol style="list-style-type:upper-alpha">
     *                     <li>
     *                         Search a network ( a {@link NS3CodePiece.StructureSubState StructureSubState} in  {@link #structStates structStates} that has
     *                         {@link NS3CodePiece.StructureSubState#readyCodes readyCodes}<br>
     *                         If there is none, exit the loop.
     *                     </li><li>
     *                         As long as there are {@link NS3CodePiece.StructureSubState#readyCodes ready code pieces} in this network
     *                         <ol style="list-style-type:lower-alpha">
     *                             <li> select one of them </li>
     *                             <li> write it </li>
     *                             <li> remove it from the {@link NS3CodePiece.StructureSubState#readyCodes readyCodes} of its network</li>
     *                             <li> Decrement by 1 the backward dependencies ({@link NS3CodePiece#preceding
     *                             NS3CodePiece.preceding}) of all pieces of code that depend on it.<br>
     *                             (<i>note:</i> These pieces of code are the ones in the forward dependency ({@link NS3CodePiece#following
     *                             NS3CodePiece.following}) of the current piece) <br>
     *                             If the {@link NS3CodePiece#preceding backward dependency} of a piece reaches 0 during this process,
     *                             put this piece in the {@link NS3CodePiece.StructureSubState#readyCodes readyCodes} of its own network
     *                             </li>
     *                         </ol>
     *                     </li>
     *                 </ol>
     *             </li><li>
     *                 Detect dependency loop: browse all codes pieces in {@link #structStates structStates} relying on
     *                 {@link NS3CodePiece.StructureSubState#allCodes StructureSubState.allCodes}, and collect the ones that don't have all their backward
     *                 dependencies resolved ({@link NS3CodePiece#preceding NS3CodePiece.preceding} &#8800; 0).<br>
     *                 If there are some, there is at least one dependency cycle. Then, the blocked code is handled by
     *                 {@link #handleDepCycleError handleDepCycleError} which computes such a cycle to provide an informative error message.
     *             </li>
     *        </ol>
     *     </dd>
     * </dl>
     * @return true if linearization is possible (no dependency cycle). Otherwise, returns false.
     */
    private boolean linearize() {
        subnetScriptCodes = new ArrayList<>();
        // to limit interleaving between code of different networks
        NS3CodePiece.StructureSubState currentNt;

        // linearization algorithm
        while (true) { //.......................................# Search subnet with ready code and write as much code from this subnet as possible
            currentNt = null;
            for (NS3CodePiece.StructureSubState nt : structStates) { //........................@ search subnet with ready code
                if (!nt.readyCodes.isEmpty()) {
                    currentNt = nt;
                    break;
                }
            }
            if (currentNt == null) // all code written or building blocked (loop in dependencies)
                break; //................................................@ exit the building process if it is finished.
            String subNetIntro = "// ========== Subnet " + currentNt.ns3Struct.getName() + " ==========\n";
            subnetScriptCodes.add(new NS3CodePieceRaw("\n" + subNetIntro));
            do { //......................................................@ collect ready codes in current subnet, as long as this is possible
                int codeIndex = currentNt.readyCodes.size() - 1; //............. - select one ready code
                NS3CodePiece code = currentNt.readyCodes.get(codeIndex);
                subnetScriptCodes.add(code); //................................ - collect it
                currentNt.readyCodes.remove(codeIndex); //..................... - remove it from ready codes as it has now been handled.
                for (NS3CodePiece nextCode : code.following) { //.............. - browse codes that depend on it to...
                    nextCode.preceding--; //.......................................... + remove one of their dependency (now satisfied)
                    if (nextCode.preceding == 0) //................................... + and when it makes some code ready
                        nextCode.net.readyCodes.add(nextCode); //.....................   add it to the ready codes of its subnet.
                }
            } while (!currentNt.readyCodes.isEmpty()); // subnet (temporarily) blocked or finished
        }

        // Result analysis (dependency loop detection)
        List<NS3CodePiece> blockedCodes = new ArrayList<>(); // codes that have not been selected, i.e., codes with ctrlPrevs > 0
        for (NS3CodePiece.StructureSubState nt : structStates) //....................# collect unselected codes
            for (NS3CodePiece c : nt.allCodes) if (c.preceding > 0) blockedCodes.add(c);
        if (!blockedCodes.isEmpty()) {
            handleDepCycleError(blockedCodes);
            return false;
        }
        return true;
    }
    /**
     * Build the part of the ns3 script that builds the network structure, i.e., fills {@link #ns3NetworkScript ns3NetworkScript}, as
     * explained <a href="#order">print</a>..
     *
     * <dl>
     *     <dt><span class="paramLabel">Algorithm:</span></dt>
     *     <dd><p><i>Roughly speaking</i></p>
     *         The script is written in the following order
     *          <ol>
     *              <li>
     *                  put first some static declarations (in paritcular helper declarations and initialization)
     *              </li><li>
     *                  browse {@link #ns3MainNodes ns3MainNodes} (the nodes that are not created by network and thus must be declared and created at
     *                  toplevel) and generate the declaring/creating code for each node it contains.
     *              </li><li>
     *                  write the code of all {@link NS3CodePiece NS3CodePiece}s in {@link #subnetScriptCodes subnetScriptCodes} respecting the order
     *                  of {@link #subnetScriptCodes subnetScriptCodes} (<i>note: </i>the toString method of a {@link NS3CodePiece NS3CodePiece} provide it
     *                  script code contribution).
     *              </li>
     *          </ol>
     *     </dd>
     * </dl>
     */
     
private void buildNS3NetworkScript() {
    ns3NetworkScript = "// NS3 topology handled externally by Java ontology bridge.\n";
}

    /**
     * Prints with {@link TraceManager#addDev TraceManager.addDev} a message describing a dependency cycle found in a set of {@link NS3CodePiece
     * NS3CodePiece}s.
     *
     * <p> The input set must be such that <br>
     * - all pieces in the set are in a dependency cycle included in the set<br>
     * - {@link NS3CodePiece#ctrlDependency ctrlDependency}s and {@link NS3CodePiece#creationDependency
     * nodeDependency}s of {@link NS3CodePiece NS3CodePiece}s contain at least all the dependencies involved in
     * these cycles.</p>
     * <p> {@link #featureLocalCreationCode
     * autoNodeCreationCode} must be up-to-date.</p>
     *
     * <p> All this is ensured when this method is called from {@link #linearize linearize} on the cycles it detects.</p>
     * <dl>
     *     <dt><span class="paramLabel">Algorithm:</span></dt>
     *     <dd>
     *         <ol>
     *              <li> Complete the dependencies in the input {@link NS3CodePiece#ctrlDependency
     *                  NS3CodePiece.ctrlDependency}s w.r.t. {@link NS3CodePiece#creationDependency
     *                  NS3CodePiece.nodeDependency} and {@link #featureLocalCreationCode
     *                  autoNodeCreationCode} but restrict them to the dependencies on {@link NS3CodePiece
     *                  NS3CodePiece}s in the input set.<br>
     *                  This ensures that finally, the {@link NS3CodePiece#ctrlDependency
     *                  ctrlDependency}s of input {@link NS3CodePiece
     *                  NS3CodePiece}s contain exactly the dependencies involved in cycles.
     *              </li><li>
     *                  Arbitrarily chose an input {@link NS3CodePiece NS3CodePiece} (any is necessarily in a cycle)
     *              </li><li>
     *                  Walk a path (while memorizing it) starting from this {@link NS3CodePiece NS3CodePiece} and following dependencies
     *                  in {@link NS3CodePiece#ctrlDependency NS3CodePiece.ctrlDependency}s until encountering a {@link NS3CodePiece NS3CodePiece}
     *                  that is already in the memorized path.
     *              </li><li>
     *                  Write the error message containing the sub-path of the path from the first occurence of the terminating node (encountered
     *                  twice) to the terminating node itself.
     *              </li>
     *          </ol>
     *     </dd>
     * </dl>
     * @param codes the input set of NS3CodePiece {@link NS3CodePiece NS3CodePiece}s, which must be a coherent set of cycles.
     */
    private void handleDepCycleError(List<NS3CodePiece> codes) {
        if(codes.isEmpty()) return;
        for (NS3CodePiece code : codes) { // keep only unsatisfied dependencies
            HashSet<NS3CodePiece> remainingDeps = new HashSet<>(); // unsatisfied dependencies of code
            for (NS3CodePiece c : code.ctrlDependency) if (codes.contains(c)) remainingDeps.add(c);
            for (NS3Feature n : code.creationDependency) {
                NS3CodePiece c = featureLocalCreationCode.get(n);
                if (c != null && codes.contains(c)) remainingDeps.add(c);
            }
            code.ctrlDependency = remainingDeps; // ctrlDependency now contains all unsatisfied dependencies of code
        }
        // search one of the dependency cycles (exists, as no blocking is possible without such a cycle)
        List<NS3CodePiece> dependencyPath = new ArrayList<>(); // traveled path
        HashMap<NS3CodePiece, Integer> pathIndex = new HashMap<>(); // indexes of each encountered piece of code in the traveled path
        int currentIndex = 0; // current index in travel
        NS3CodePiece code = codes.get(0); // current piece of code in path, can be initialized with any piece of code in codes.
        Integer loop; // previous index of current piece of code, if found
        do {
            pathIndex.put(code, currentIndex);
            dependencyPath.add(code);
            code = new ArrayList<>(code.ctrlDependency).get(0);
            loop = pathIndex.get(code);
            currentIndex++;
        } while (loop == null); // found loop: dependencyPath[loop - currentIndex[

        // build the error message
        StringBuffer msg = new StringBuffer("Generation of NS3 network architecture script fails. " +
                "Dependency cycle:\n  " +
                dependencyPath.get(loop).getName() + " depends on\n");
        for (int i = loop + 1; i < currentIndex; i++)
            msg.append("  " + dependencyPath.get(i).getName() + " depends on\n");
        msg.append("  " + code.getName() + "\n");
        TraceManager.addDev(msg.toString());
    }

    // GENERATING SCRIPT FOR NETWORK TASKS ====================================

    /**
     * Summarizes the required information for generating the script code that creates and installs a ns3 tasks
     * associated to a Diplodocus networking tasks.
     *
     * Each task corresponds to a channel end, which is implemented by an <code>IPV4_address:port_number</code>
     * couple in our modeling.
     */
    public static class NS3NetTask {
        /** the associated Diplodocus Task */
        public TMLTask task;
        /** the port number identifying the channel end */
        public int port;
        /** the task associated to the dual channel end */
        public NS3NetTask dualTask;
        /** the Diplodocus execution node (necessary a {@link HwNetworkInterface HwNetworkInterface}) the task is mapped on */
        public HwNetworkInterface networkInterface;
        /** the direction of the channel end: true if on the sending side */
        boolean write;

        /**
         * Memorizes task associated information for task script generation
         * @param task associated Diplodocus Task
         * @param port port identifying channel end
         * @param dualTask dual channel end Task
         * @param networkInterface hardware node the task is mapped on
         * @param write direction of the channel end
         */
        public NS3NetTask(TMLTask task, int port, NS3NetTask dualTask, HwNetworkInterface networkInterface, boolean write) {
            this.task = task;
            this.port = port;
            this.dualTask = dualTask;
            this.networkInterface = networkInterface;
            this.write = write;
        }
    }

    /**
     * Compute and returns the task part of the ns3 script for the tasks provided as input, which also populates the routing tables and finally
     * create the simulator object.
     * <ol>
     *     <li>
     *         browses the tasks and for each task, generates: its creation code, the code that put it on its ns3 node and the code that
     *         provides it with a start time. <br>
     *         By the way also generates code that puts the created tasks in a dedicated list.
     *     </li><li>
     *         add the code which calls the centralized static routing algorithm provided by ns3 to populate the routing tables
     *     </li><li>
     *         add the code which create the simulator. This code calls the simulator's constructor with the list of task collected in point 1 above
     *         as
     *         parameter,
     *         together with the {@link #timePerCycle timePerCycle} (provided while creating this instance of
     *         {@link NS3GlobalNetwork(List, int) NS3GlobalNetwork}).
     *     </li>
     *
     * </ol>
     *
     *
     * @param netTasks the tasks to be installed by the returned script.
     * @param namesGen links Diplodocus task to their c++ name.
     * @return the script that creates and install the tasks provided as input
     */
    
    /////////////////////////////////////////////  replacemeent for ontology bridge

    public String getNs3TasksScript(Collection<NS3NetTask> netTasks, NamesGenerationHelper namesGen) {
        // ONTOLOGY BRIDGE: replaces NS3TaskTCPWrite/Read + NS3RunnerImplementation
        StringBuffer sb = new StringBuffer(
            "// ONTOLOGY BRIDGE DECLARATIONS ################\n" +
            "// NS3 topology handled externally by Java ontology bridge process.\n" +
            "std::string _ontoBridgeTtlPath = \"/home/USER/eclipse/eclipse-workspace/Diplo-Ns3-mapper1/diplodocus.owl.ttl\";\n\n"
        );

        for (NS3NetTask netTask : netTasks) {
            String taskCppName = namesGen.taskInstanceName(netTask.task);
            String commandId   = "cmd_" + netTask.task.getName().replace("::", "_");
            String varName     = "bridgeTask_" + netTask.task.getName().replace("::", "_");

            sb.append("// Ontology bridge task for " + netTask.task.getName() + "\n");
            sb.append("OntologyBridgeTask* " + varName + " = new OntologyBridgeTask(" +
                      taskCppName + ", _ontoBridgeTtlPath, \"" + commandId + "\");\n\n");
        }

        // Build the runner and register all tasks
        sb.append("OntologyBridgeRunner* _ontoBridgeRunner = " +
                  "new OntologyBridgeRunner(" + timePerCycle + ", _ontoBridgeTtlPath);\n");

        for (NS3NetTask netTask : netTasks) {
            String taskCppName = namesGen.taskInstanceName(netTask.task);
            String varName     = "bridgeTask_" + netTask.task.getName().replace("::", "_");
            sb.append("_ontoBridgeRunner->registerTask(" + taskCppName + ", " + varName + ");\n");
        }

        sb.append("_ns3Runner = _ontoBridgeRunner;\n");
        sb.append("// END OF ONTOLOGY BRIDGE DECLARATIONS ################\n");
        return sb.toString();
    }
    /////////////////////////////////////////////  replacemeent for ontology bridge
    
}
