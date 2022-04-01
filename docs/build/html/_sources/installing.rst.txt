Installing PCNsim
=================

After checking if you meet the correct requirements of PCNsim, you can install install PCNsim.

First, clone our Github respository: ::

    $ git clone https://github.com/gfrebello/pcnsim

After cloning the repository, install the necessary libraries to run PCNsim: ::

    $ cd pcnsim
    $ pip install -r requirements.txt

You may want to create a environment variable that points to the pcnsim source directory: ::

    $ export PCNSIM_DIR = $PWD

Installing the Dataset
----------------------
As real-world transaction data about the Lightning Network is not available due to privacy, PCNsim uses a credit-card dataset to model transactions on the network simulator. As PCNs aim to offer a payment method as fast as current credit-card companies, we argue that credit-card transactions are a good fit to model transactions size.
The credit-card dataset used to model transactions in PCNSim is available at `Kaggle <https://www.kaggle.com/datasets/mlg-ulb/creditcardfraud>`_.

After downloading the creditcard dataset, move the csv file to the datasets folder: ::

    $ mv creditcard.csv $PCNSIM_DIR/scripts/datasets

.. note:: If you are having problems installing PCNsim, you can contact our team. Our contact information is available at `the official PCNsim website <https://gta.ufrj.br/pcnsim>`_.
