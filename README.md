# PCNsim: Payment Channel Network Simulator 
[![Documentation Status](https://readthedocs.org/projects/pcnsim/badge/?version=latest)](https://pcnsim.readthedocs.io/en/latest/?badge=latest)

This project presents PCNsim, an open-source, modular, and lightweight payment channel network. For more information, you can visit [our website](https://www.gta.ufrj.br/pcnsim) or the [official PCNsim documentation](https://pcnsim.readthedocs.io). PCNsim is designed to support multiple routing algorithms, topologies, and transactions workloads to meet the requirements of PCN researchers. 

PCNsim presents a unique simulator that accuratly mimics the behavior of the Lightning Network, such as message formats and message exchange.

## Requirements

Our PCN implementation uses Python 3 to implement the topology and workload generators. Furthermore, we use the [NetworkX](https://networkx.org/) to create the network topology and [Click](https://click.palletsprojects.com/). We assume that you already have Python 3 and pip installed in your computer. To install the necessary requirements, run the following command at the `pcnsim` directory:

    pip install -r requirements.txt

PCNsim also runs over OMNET++ network simulator to deliver an interactive user-friendly interface and a modular architecture. You can find informations on how to install OMNET++ at the [official OMNET++ documentation page](https://omnetpp.org/).

## Publications

A [demo paper](http://www.gta.ufrj.br/ftp/gta/TechReports/RCP22.pdf) of this project was accepted for publication at the IEEE INFOCOM 2022.

    Rebello, G. A. F., Camilo, G. F., Potop-Butucaru, M., Campista, M. E. M., de Amorim, M. D., Costa, L. H. M. K. - "PCNsim: A Flexible and Modular Simulator for Payment Channels Network", in Demos of the IEEE INFOCOM, May 2022, London, United Kingdom.

