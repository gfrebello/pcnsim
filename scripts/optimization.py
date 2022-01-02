import csv
import numpy as np
from scipy.optimize import linprog
np.set_printoptions(linewidth=200)

class Arc:
    def __init__(self, origin, destination, cost, capacity):
        self.From = origin
        self.To = destination
        self.Cost = cost
        self.Capacity = capacity

class Commodity:
    def __init__(self, source, sink, value):
        self.Source = source
        self.Sink = sink
        self.Value = value

class Node:
    def __init__(self, nodeid):
        self.Id = nodeid
        self.InLinks = []
        self.OutLinks = []

    def addInLink(self, Node):
        self.InLinks.append(Node)

    def addOutLink(self, Node):
        self.OutLinks.append(Node)

def init_nodes():
    nodes = []
    node_ids = []
    with open('arcs.csv') as arcsfile:
        reader = csv.reader(arcsfile)
        for row in reader:
            origin_id = row[0]
            dest_id = row[1]
            if origin_id not in node_ids:
                node_ids.append(origin_id)
                nodes.append(Node(origin_id))
            if dest_id not in node_ids:
                node_ids.append(dest_id)
                nodes.append(Node(dest_id))
    nodes.sort(key=lambda x: x.Id)
    return nodes

def init_arcs(nodes):
    arcs = []
    with open('arcs.csv') as arcsfile:
        reader = csv.reader(arcsfile)
        for row in reader:
            arc = Arc(int(row[0]),int(row[1]),float(row[2]),float(row[3]))
            origin = nodes[int(row[0])]
            destination = nodes[int(row[1])]
            origin.addOutLink(destination)
            destination.addInLink(origin)
            arcs.append(arc)
    return arcs

def init_comms():
    comms = []
    with open('comms.csv') as commsfile:
        reader = csv.reader(commsfile)
        for row in reader:
            comm = Commodity(int(row[0]),int(row[1]),float(row[2]))
            comms.append(comm)
    return comms


def init_incmatrix(arcs):
    incmatrix = np.zeros((len(nodes),len(arcs)))
    i = 0
    for arc in arcs:
        incmatrix[arc.From,i] = 1
        incmatrix[arc.To,i] = -1
        i += 1
    return incmatrix

def build_A_ub(arcs, comms):
    A_ub1 = np.concatenate([np.identity(len(arcs))*comm.Value for comm in comms], axis=1)   
    A_ub2 = np.concatenate([np.identity(len(arcs)) for comm in comms], axis=1)
    
    k=0
    for comm in comms:
        for i in range(len(arcs)):
            for j in range(k,k+len(arcs)):
                if A_ub2[i][j] == 1:
                    A_ub2[i][j] = comm.Value
                    if i%2 == 0:
                        A_ub2[i][j+1] = -comm.Value
                    else:
                        A_ub2[i][j-1] = -comm.Value
        k+=len(arcs)
    
    A_ub3 = np.copy(-A_ub2)
    
    A_ub = np.concatenate((A_ub1,A_ub2,A_ub3), axis=0)
    return A_ub

def build_b_ub(arcs, ro):
    caps = np.array([ arc.Capacity for arc in arcs ])
    ro_vector = np.array([ro for arc in arcs])
    minusro_vector = np.copy(ro_vector)
    b_ub = np.concatenate((caps, ro_vector, minusro_vector))
    return b_ub

def build_A_eq(arcs, nodes, comms, incmatrix):
    A_eq = np.zeros((len(nodes)*len(comms),len(arcs)*len(comms)))
    i = j = 0
    for comm in comms:
        A_eq[i:i+len(nodes),j:j+len(arcs)] = incmatrix
        i+=len(nodes)
        j+=len(arcs)
    return A_eq

def build_b_eq(nodes, comms):
    b_eq = np.zeros(len(nodes)*len(comms))
    i = 0
    for comm in comms:
        b_eq[i+comm.Source] = comm.Value
        b_eq[i+comm.Sink] = -comm.Value
        i += len(nodes)
    return b_eq

def optimize(nodes, arcs, comms, ro):
    incmatrix = init_incmatrix(arcs)
    global c, A_ub, b_ub, A_eq, b_eq
    c = np.array([ arc.Cost for arc in arcs ] * len(comms))
    A_ub = build_A_ub(arcs,comms)
    b_ub = build_b_ub(arcs,ro)
    A_eq = build_A_eq(arcs,nodes,comms,incmatrix)
    b_eq = build_b_eq(nodes,comms)
   
    res = linprog(c, A_ub, b_ub, A_eq, b_eq, bounds=(0,1))
    print(res)

if __name__ == "__main__":
    nodes = init_nodes()
    arcs  = init_arcs(nodes)
    comms = init_comms()
    optimize(nodes,arcs,comms,ro=1.)
