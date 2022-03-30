# genTopo

## Description
The `genTopo` command generates a network topology to be used in the payment channel network simulator. PCNsim offers network topology modeling following a small-world topology or a scale-free topology as the Lighnint Network presents behavior similar to both types.

## genTopo
```
Usage: generate_topology_workload.py genTopo [OPTIONS]

  Generates a topology for the simulation

Options:
  -t, --topology [scale-free|barabasi-albert|watts-strogatz]
                                  Topology used in the simulation
  -n, --nodes INTEGER             Number of nodes in the topology
  --alpha FLOAT                   Alpha parameter for scale-free topology
  --beta FLOAT                    Beta parameter for scale-free topology
  --gamma FLOAT                   Gamma parameter for scale-free topology
  -k INTEGER                      K parameter for Watts-Strogatz graph
  -p FLOAT                        P parameter for Watts-Strogatz graph
  -m INTEGER                      M parameter for Barabasi-Albert graph
  --lightning                     Channel capacities are modeled following
                                  real-world lightning network channels

  --help                          Show this message and exit.
```
## Default Values
- `-t, --topology`: Type of network topology that will be used to run the PCN simulation. We offer 3 types of network topology: scale-free, Barabasi-Albert, and Watts-Strogatz. It's worth noting that Barabasi-Albert and Watts-Strogatz graph model are small-world topology. The user may choose the network topology by selecting the topology option followed by the string `"scale-free"`, `"barabasi-albert"`, or `"watts-strogatz"`, depending on the type of topology the the user choses.
    - *Default value:* `"scale-free"`
- `-n, --nodes`: Number of nodes that make up the network topology. This value has to be an **integer**.
    - *Default value:* `10`
- `--alpha`: Parameter alpha used to generate a scale-free network topology. This parameter is only used if the `scale-free` topology is chosen to model the network. Alpha is a **float** type parameter.
    - *Default value:* `0.5`
- `--beta`: Parameter beta used to generate a scale-free network topology. This parameter is only used if the `scale-free` topology is chosen to model the network. Beta is a **float** type parameter.
    - *Default value:* `1e-05`
- `--gamma`: Parameter gamma used to generate a scale-free network topology. This parameter is only used if the `scale-free` topology is chosen to model the network. It's worth noting that `alpha + gamma = 1`. Gamma is a **float** type parameter.
    - *Default value:* `0.49999`
- `--k`: Parameter k used to generate a scale-free network topology. This parameter is only used if the `watts-strogatz` topology is chosen to model the network. K is an **integer** type parameter.
    - *Default value:* ``

## Example Usage
 
 - Model channels following the real-world Lightning Network channel capacity. It's worth mentioning that this options **does not** model the network topology following the Lightning Network topology, only its channel parameters. The network topology follows a scale-free network model.

```
python3 generate_topology_workload.py genTopo --lightning

Setting topology to scale-free
Setting n to 10
Setting alpha to 0.5
Setting beta to 1e-05
Setting gamma to 0.49999
```