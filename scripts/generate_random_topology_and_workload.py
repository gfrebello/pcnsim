import networkx as nx
import csv
import random
import collections
from networkx.readwrite.gml import write_gml
from networkx.readwrite.edgelist import write_edgelist
from networkx.classes.function import density
from graphs import *

def generate_random_list(n, min_value, max_value):
    l = []
    for i in range (0,n):
        element = random.uniform(min_value,max_value)
        l.append(element)
    return l



# Remove self loops
def adjust_edges (graph):
    graph.remove_edges_from(nx.selfloop_edges(graph))
    inv_edges = []

    # Create edges on the opposite direction and add them to the graph
    for edge in graph.edges():
        inv_edge = edge[::-1]
        inv_edges.append(inv_edge)
   

    for inv_edge in inv_edges:
        graph.add_edge(*inv_edge)

    return graph

# Initialize channel attributes
def initialize_attributes(graph, multigraph):
    attrib_list = ['capacity','link_quality','fee','max_accepted_HTLCs','HTLC_minimum_msat','channel_reserve_satoshis','delay']
    n_edges = graph.number_of_edges()
    attribs = collections.defaultdict(dict)
    capacities = generate_random_list(n_edges, min_value=0, max_value=10)
    link_qualities = generate_random_list(n_edges, min_value=0, max_value=1)
    fees = generate_random_list(n_edges, min_value=1e-5, max_value=1e-4)
    max_accepted_HTLCs = [483] * n_edges
    HTLC_minimum_msats = [0.1] * n_edges
    channel_reserve_satoshis = [0.01] * n_edges
    delays = [100] * n_edges


    for edge in graph.edges():
        capacity = capacities.pop()
        link_quality = link_qualities.pop()
        fee = fees.pop()
        max_accepted_HTLC = max_accepted_HTLCs.pop()
        HTLC_minimum_msat = HTLC_minimum_msats.pop()
        channel_reserve_satoshi = channel_reserve_satoshis.pop()
        delay = delays.pop()

        if (multigraph):
            attribs[edge+(0,)]['capacity'] = capacity
            attribs[edge+(0,)]['link_quality'] = link_quality
            attribs[edge+(0,)]['fee'] = fee
            attribs[edge+(0,)]['max_accepted_HTLCs'] = max_accepted_HTLC
            attribs[edge+(0,)]['HTLC_minimum_msat'] = HTLC_minimum_msat
            attribs[edge+(0,)]['channel_reserve_satoshis'] = channel_reserve_satoshi
            attribs[edge+(0,)]['delay'] = delay
        else:
            attribs[edge]['capacity'] = capacity
            attribs[edge]['link_quality'] = link_quality
            attribs[edge]['fee'] = fee
            attribs[edge]['max_accepted_HTLCs'] = max_accepted_HTLC
            attribs[edge]['HTLC_minimum_msat'] = HTLC_minimum_msat
            attribs[edge]['channel_reserve_satoshis'] = channel_reserve_satoshi
            attribs[edge]['delay'] = delay

    nx.set_edge_attributes(graph, attribs)


    write_edgelist(graph, "../topologies/topology", data=attrib_list)
    save_graph(graph)

def get_end_hosts_list(graph, complete):
    end_hosts = []
    if (complete == False):
        for node in graph.nodes():
            if (graph.out_degree(node)== 1):
                end_hosts.append(node)
    else:
        for node in graph.nodes():
            end_hosts.append(node)
    return end_hosts

def create_workload(n_payments, min_payment, max_payment, end_hosts):
    payment_list = []

    for i in range(0,n_payments):
        pair = random.sample(end_hosts, 2)
        payment_value = random.uniform(min_payment, max_payment)
        timestamp = random.randint(1,5000)
        payment_tuple = (pair[0], pair[1], payment_value, timestamp)
        payment_list.append(payment_tuple)

    sorted_payments = sorted(payment_list, key=lambda x: x[3])

    with open('../workloads/random-workload.txt','w') as out:
        csv_out=csv.writer(out, delimiter=' ')
        for row in sorted_payments:
            csv_out.writerow(row)

