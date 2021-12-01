import networkx as nx

FILENAME = 'current-graph'

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
    nx.write_graphml(graph, FILENAME)

def read_graph():
    graph = nx.read_graphml(FILENAME)
    return graph
