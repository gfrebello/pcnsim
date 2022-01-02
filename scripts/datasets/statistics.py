import pandas as pd
import numpy as np
import random
import collections
from scipy import stats
from matplotlib import pyplot as plt


def read_dataset(filepath):
    dataset = pd.read_csv(filepath, encoding='unicode_escape')
    return dataset

def get_random_elements_from_dataset (dataset, column, number_samples):
    random_value = random.randint(0,65536)
    dataset = dataset[(dataset[column] > 0)]
    random_samples = dataset[column].sample(n=number_samples, random_state=random_value)
    return random_samples

def check_dataset(dataset):
    if dataset == 'credit-card':
        filename = 'datasets/creditcard.csv'
    else:
        filename = 'datasets/data.csv'
    dataset = read_dataset(filename)
    return dataset
