import networkx as nx
import csv
import random
import collections
import pandas as pd
from networkx.readwrite.gml import write_gml
from networkx.readwrite.edgelist import write_edgelist
from networkx.classes.function import density
from graphs import *
from datasets.channels_statistics import *
from datasets.statistics import *

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
def initialize_attributes(graph, lightning, multigraph):
    if (lightning):
        lightning_capacity = get_channel_stats('capacity')
        selected_capacities = get_random_statistics(lightning_capacity, graph.number_of_edges())
        selected_capacities_euros = convert_bitcoins_to_euro(selected_capacities)
        selected_capacities_euros = round(selected_capacities_euros)
        capacities = selected_capacities_euros.tolist()


        lightning_fee_base = get_channel_stats('fee_base_msat')
        selected_fees_base = get_random_statistics(lightning_fee_base, graph.number_of_edges())
        selected_fees_euros = convert_bitcoins_to_euro(selected_fees_base)
        fees = selected_fees_euros.tolist()

        lightning_min_htlc = get_channel_stats('min_htlc')
        selected_min_htlc = get_random_statistics(lightning_min_htlc, graph.number_of_edges())
        selected_min_htlc_euros = convert_bitcoins_to_euro(selected_min_htlc)
        HTLC_minimum_msats = selected_min_htlc_euros.tolist()

        lightning_fee_rate = get_channel_stats('fee_rate_milli_msat')
        selected_fee_rate = get_random_statistics(lightning_fee_rate, graph.number_of_edges())
        selected_fee_rate_euros = convert_bitcoins_to_euro(selected_fee_rate)
    else:
        capacities = generate_random_list(n_edges, min_value=0, max_value=10)
        HTLC_minimum_msats = [0.1] * n_edges
        fees = generate_random_list(n_edges, min_value=1e-5, max_value=1e-4)



    attrib_list = ['capacity','link_quality','fee','max_accepted_HTLCs','HTLC_minimum_msat','channel_reserve_satoshis','delay']
    n_edges = graph.number_of_edges()
    attribs = collections.defaultdict(dict)
    link_qualities = generate_random_list(n_edges, min_value=0, max_value=1)
    max_accepted_HTLCs = [483] * n_edges
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
            if (graph.degree(node) == 1):
                end_hosts.append(node)
    else:
        for node in graph.nodes():
            end_hosts.append(node)
    return end_hosts

def create_workload(n_payments, min_payment, max_payment, end_hosts, credit_card, e_commerce):
    payment_list = []
    if credit_card:
        dataset = check_dataset('credit-card')
        values = get_random_elements_from_dataset(dataset, 'Amount', n_payments)
        values = values.tolist()
        print (values)
        for i in range(0,n_payments):
            pair = random.sample(end_hosts, 2)
            timestamp = random.randint(1,5000)
            payment_tuple = (pair[0],pair[1], values.pop(), timestamp)
            payment_list.append(payment_tuple)
    elif e_commerce:
        dataset = check_dataset('e-commerce')
        values = get_random_elements_from_dataset(dataset, 'UnitPrice', n_payments)
        values = values.tolist()
        for i in range(0,n_payments):
            pair = random.sample(end_hosts, 2)
            timestamp = random.randint(1,5000)
            payment_tuple = (pair[0],pair[1], values.pop(), timestamp)
            payment_list.append(payment_tuple)

    else:
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

