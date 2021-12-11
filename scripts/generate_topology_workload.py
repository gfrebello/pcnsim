import click
from generate_random_topology_and_workload import *
from graphs import *

@click.group(chain=True, invoke_without_command=True)
def cli():
    pass

@cli.command(name='genTopo', help='Generates a topology for the simulation')
@click.option('-t','--topology', default='scale-free', type=click.Choice (['scale-free','barabasi-albert','watts-strogatz'], case_sensitive=False), help='Topology used in the simulation')
@click.option('-n','--nodes', type=int, default=10, help='Number of nodes in the topology')
@click.option('--alpha', default=0.5, help='Alpha parameter for scale-free topology')
@click.option('--beta', default=0.00001, help='Beta parameter for scale-free topology')
@click.option('--gamma', default=0.49999, help='Gamma parameter for scale-free topology')
@click.option('-k', default=2, help='K parameter for Watts-Strogatz graph')
@click.option('-p', default=0.1, help='P parameter for Watts-Strogatz graph')
@click.option('-m', default=2, help='M parameter for Barabasi-Albert graph')
def generate_topology (topology, nodes, alpha, beta, gamma, k, p, m):
    if (topology == 'scale-free'):
        print ('Setting topology to scale-free')
        print ('Setting n to ' + str(nodes))
        print ('Setting alpha to ' + str(alpha))
        print ('Setting beta to ' + str(beta))
        print ('Setting gamma to ' + str(gamma))
        graph = generate_scale_free(nodes, alpha, beta, gamma)
    if (topology == 'watts-strogatz'):
        print ('Setting topology to watts-strogatz')
        print ('Setting n to ' + str(nodes))
        print ('Setting k to ' + str(k))
        print ('Setting p to ' + str(p))
        graph = generate_watts_strogatz(nodes, k, p)
    if (topology == 'barabasi-albert'):
        print ('Setting topology to watts-strogatz')
        print ('Setting n to ' + str(nodes))
        print ('Setting m to ' + str(m))
        graph = generate_barabasi_albert(nodes, m) 

    graph = adjust_edges(graph)
    initialize_attributes(graph, True if topology == 'scale-free' else False)

@cli.command(name='genWork', help='Generates a payment workload for the simulation')
@click.option('--n_payments', default=1, help='Number of payments in th network simulation')
@click.option('--min_payment', default=0.1, help='Minimum value of a payment in the network')
@click.option('--max_payment', default=1., help='Maximum value of a payment in the network')
@click.option('--time_window', default=5000, help='Time window (in seconds) for generating transaction timestamps')
@click.option('--any_node', is_flag=True, help='Transactions are issued by any node in the network, not only end hosts')
def generate_workload(n_payments, min_payment, max_payment, any_node, time_window):
    graph = read_graph()
    end_hosts = get_end_hosts_list(graph, any_node)
    create_workload(n_payments, min_payment, max_payment, end_hosts, time_window)

if __name__ == "__main__":
    cli()
