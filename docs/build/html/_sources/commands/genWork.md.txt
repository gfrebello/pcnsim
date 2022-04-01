# genWork

## Description
The `genWork` command generates a transaction workload to be used in the payment channel network simulator. PCNsim offers genWork modeling following a real-world data from a credit-card and an e-commerce dataset as the Lighnint Network does not disclose transaction information due to privacy issues.

## genWork
```
Usage: generate_topology_workload.py genWork [OPTIONS]

  Generates a payment workload for the simulation

Options:
  --n_payments INTEGER   Number of payments in the network simulation
  --min_payment FLOAT    Minimum value of a payment in the network
  --max_payment INTEGER  Maximum value of a payment in the network
  --any_node             Transactions are issued by any node in the network,
                         not only end hosts

  --credit_card          Transactions are modeled following a credit card
                         dataset

  --e_commerce           Transactions are modeled following a e-commerce
                         dataset

  --help                 Show this message and exit.

```
## Default Values
- `--n_payments`: Number of payments in the PCN simulation. The selected number of payments will be issued by a randomly selected node to another randomly selected node. This value as to be an **integer**.
    - *Default value:* `1`
- `--min_payments`: Minimum value of a payment in the PCN simulation. This value has to be an **float**.
    - *Default value:* `0.1`
- `--max_payments`: Maximum value of a payment in the PCN simulation. This value has to be an **float**.
    - *Default value:* `1`

## Flags
- `--any_node`: By default, PCNsim restricts the act of issuing a transaction to end-hosts. Therefore, core nodes act only as intermediary, forwarding trasactions, but not issuing. This flag removes this restriction and includes core nodes in the act of issuing transactions.
- `--credit_card`: This flag makes PCNsim model transaction values following a credit-card dataset.
- `--e_commerce`: This flag makes PCNsim model transaction values following an e-commerce dataset.

## Example Usage
