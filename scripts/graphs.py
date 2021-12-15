import networkx as nx

FILENAME = '../topologies/topology'

def generate_scale_free(n, alpha, beta, gamma):
    graph = nx.scale_free_graph(n, alpha, beta, gamma)
    return graph

def generate_watts_strogatz(n, k, p):
    graph = nx.watts_strogatz_graph(n, k, p)
    return graph

def generate_barabasi_albert(n, m):
    graph = nx.barabasi_albert_graph(n, m)
    return graph

def save_graph(graph):
    #nx.write_graphml(graph, FILENAME)
    pass

def read_graph():
    attrib_list = ['capacity','link_quality','fee','max_accepted_HTLCs','HTLC_minimum_msat','channel_reserve_satoshis', 'delay']
    graph = nx.read_edgelist(FILENAME, data=(("capacity",float),("link_quality", float), ("fee", float), ("max_accepted_HTLCs", int), ("HTLC_minimum_msat",float), ("channel_reserve_satoshis", float), ("delay", int)))
    return graph
