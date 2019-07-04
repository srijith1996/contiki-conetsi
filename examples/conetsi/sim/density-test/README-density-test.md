# Density Test Simulation
## for CONETSI algorithm

This simulation topology is designed for testing the scalability of CONETSI
with network density.  The nodes are arranged in a basic tree topology with 5
neighbours to the root node.  Each branch is disconnected fromthe others, and a
cluster is developed on each branch.  The details of the edges formed are
given in the table below.  Nodes have number of neighbours varying from 1-11.

| # of Neighbours | # of nodes | Node ID's                                |
| --------------- |:----------:| ----------------------------------------:|
| 1               | 10         | 58-66, 69                                |
| 2               |  4         | 2, 5-6, 35                               |
| 3               |  5         | 15, 17, 67-68, 57                        |
| 4               |  5         | 13, 16, 19-21                            |
| 5               |  6         | 14, 18, 24-25, 27-28                     |
| 6               |  7         | 22, 26, 29-31, 33-34                     |
| 7               |  7         | 23, 32, 36-38, 40, 42                    |
| 8               |  3         | 7, 39, 41                                |
| 9               |  9         | 3, 9-12, 53-56                           |
| 10              | 11         | 4, 8, 43-45, 47-52                       |
| 11              |  1         | 46                                       |
