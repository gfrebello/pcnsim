Testing Your PCNsim 
===================

.. toctree::
   :maxdepth: 1
   :hidden:

   prerequisites
   installing
   testing

After downloading and installing the required components to run PCNsim, you can test your simulator by following this documentation. Our test simulation delivers the following scenario:

* ten nodes disposed in a scale-free network topology;
* one hundred transactions issued in the network. It's worth mentioning that the transactions are only issued by the end-hosts to simulate the behavior of the Lightning Network accurately;
* the channel values follow real-world Lightning Network values;
* the transaction values follow credit-card transaction values collected from a dataset.

Creating Network Topology and Transaction Workload
--------------------------------------------------
The first step in running the simulation is determining which network topology PCNsim will use to run the payment channel network. PCNsim offers scale-free and small-world topology by default given researches show that the Lightning Network behaves as both. It is possible to implement other network topologies by implementing them with NetworkX or by defining them in the topology file.
To build the scenario described in this documentation, from the `pcnsim` root directory, go to the scripts directory: ::

    cd script

To generate the network topology described in this section of the documentation, run the `genTopo` command specifying `10` as the number of nodes and the channel modelling following the Lightning Network: ::

    python3 generate_topology_workload.py genTopo -n 10 --lightning

This command will generate a file in the `topologies` directory. This file will be used by OMNET++ to establish the connections among the nodes and channel parameters.
After generating the topology, you'll have to generate the transaction workload, which defines the characteristics of the transactions in the simulation. PCNsim offers transaction modelling following a real-world data from a credit-card company or an e-commerce sales dataset. You can also customize the workload by directly modifying the workload file.
To generate the transaction workload of our scenario, run the `genWork`command specifying credit-card as the modelling reference and `100`as the number of transactions: ::

    python3 generate_topology_workload.py genWork --n_payments 100 --credit-card

This command generates a file in the `workloads` directory.

Running the Simulation
----------------------
After generating the topology and workload, you can run PCNsim by opening the project on OMNET++ and running the simulation.