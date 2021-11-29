import networkx as nx
import csv
import random
import collections
from networkx.readwrite.gml import write_gml
from networkx.readwrite.edgelist import write_edgelist
from networkx.classes.function import density

def generate_random_list(n, min_value, max_value):
    l = []
    for i in range (0,n):
        element = random.uniform(min_value,max_value)
        l.append(element)
    return l


graph = nx.scale_free_graph(n=10, alpha=0.5, beta=0.00001, gamma=0.49999)
#graph = nx.barabasi_albert_graph(n=10,m=1)

# Remove self loops
graph.remove_edges_from(nx.selfloop_edges(graph))
inv_edges = []
print(graph.number_of_edges())

# Create edges on the opposite direction and add them to the graph
for edge in graph.edges():
#    print(edge)
    inv_edge = edge[::-1]
    inv_edges.append(inv_edge)
   

#print(inv_edges)   
for inv_edge in inv_edges:
    graph.add_edge(*inv_edge)

print(graph.number_of_edges())

# Initialize channel attributes
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
 #   print(edge)
    capacity = capacities.pop()
    link_quality = link_qualities.pop()
    fee = fees.pop()
    max_accepted_HTLC = max_accepted_HTLCs.pop()
    HTLC_minimum_msat = HTLC_minimum_msats.pop()
    channel_reserve_satoshi = channel_reserve_satoshis.pop()
    delay = delays.pop()

    attribs[edge+(0,)]['capacity'] = capacity
    attribs[edge+(0,)]['link_quality'] = link_quality
    attribs[edge+(0,)]['fee'] = fee
    attribs[edge+(0,)]['max_accepted_HTLCs'] = max_accepted_HTLC
    attribs[edge+(0,)]['HTLC_minimum_msat'] = HTLC_minimum_msat
    attribs[edge+(0,)]['channel_reserve_satoshis'] = channel_reserve_satoshi
    attribs[edge+(0,)]['delay'] = delay

#print(attribs)
nx.set_edge_attributes(graph, attribs)

#for edge in attribs:
#    graph.add_edges_from([edge], attribs[edge])

#for edge in graph.edges():
    
    #print(graph[edge[0]][edge[1]])
    #print(attribs[edge]['capacity'])
    #graph[edge[0]][edge[1]]['capacity'] = attribs[edge]['capacity']
    #graph[edge[0]][edge[1]]['link_quality'] = attribs[edge]['link_quality']
    #edge['link_quality'] = attribs[edge]['link_quality']

print(density(graph))

write_edgelist(graph, "../topologies/scale-free.txt", data=attrib_list)
#write_gml(graph,"scale-free.gml")

end_hosts = []
for node in graph.nodes():
#    print(graph.degree[node])
    if (graph.out_degree(node)== 1):
        end_hosts.append(node)

n_payments = 1
min_payment = 0.01
max_payment = 1
payment_list = []

for i in range(0,n_payments):
    pair = random.sample(end_hosts, 2)
    payment_value = random.uniform(min_payment, max_payment)
    timestamp = random.randint(1,5000)
    payment_tuple = (pair[0], pair[1], payment_value, timestamp)
    payment_list.append(payment_tuple)

sorted_payments = sorted(payment_list, key=lambda x: x[3])

print(sorted_payments)

with open('../workloads/random-workload.txt','w') as out:
    csv_out=csv.writer(out, delimiter=' ')
    #csv_out.writerow(['src','dst'])
    for row in sorted_payments:
        csv_out.writerow(row)

#print(end_hosts)
