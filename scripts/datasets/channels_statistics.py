import re
import pandas as pd
import numpy as np
from scipy import stats
from random import randint

bitcoin_to_euro_exchange = 43037.31

def get_channel_stats(stats):
    my_file = open('datasets/channels2.txt','r')
    Lines = my_file.readlines()

    channels = []
    for line in Lines:
        if stats in line:
            channels.append(re.split(r':', line)[1].replace(' ', '').replace(',','').replace('\n','').replace('"',''))
    data = pd.Series(channels)
    data = data[data != "null"]
    data = data.astype(int)
    statistics = data.agg(["mean","max","min","std","var"])
    print ("Data statistics for " + stats + ":")
    print (statistics)
    return data

def get_random_statistics (data, number_samples):
    random_value = randint(0,65536)
    random_samples = data.sample(n=number_samples, random_state=random_value)
    print ('Random values: ')
    print (random_samples)
    return random_samples

def convert_bitcoins_to_euro(data):
    data = data*10**(-8)
    print('In BTC')
    print(data)
    data = data*bitcoin_to_euro_exchange
    print ('In Euros')
    print(data)
    return data
