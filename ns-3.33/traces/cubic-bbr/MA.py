import numpy as np

# Read in x and y data from file
data7 = np.loadtxt('4/10.1.1.1_49153_10.1.3.2_5000_rtt.txt')
data8 = np.loadtxt('4/10.1.4.2_49153_10.1.5.2_5000_rtt.txt')

# Extract x and y data from array
x7 = data7[:, 0]
y7 = data7[:, 1]
x8 = data8[:, 0]
y8 = data8[:, 1]

# Define moving average function
def moving_average(y, window_size):
    weights = np.repeat(1.0, window_size) / window_size
    return np.convolve(y, weights, mode='valid')

# Calculate moving average with window size of 3
ma_y7 = moving_average(y7, 500)
ma_y8 = moving_average(y8, 500)

# Output results to file
with open('4/1_2_rtt_MA.txt', 'w') as f:
    for i in range(len(ma_y7)):
        f.write(f'{x7[i]} {ma_y7[i]}\n')
with open('4/4_5_rtt_MA.txt', 'w') as f:
    for i in range(len(ma_y8)):
        f.write(f'{x8[i]} {ma_y8[i]}\n')

# Read in x and y data from file
data7 = np.loadtxt('4/10.1.1.1_49153_10.1.3.2_5000_goodput.txt')
data8 = np.loadtxt('4/10.1.4.2_49153_10.1.5.2_5000_goodput.txt')

# Extract x and y data from array
x7 = data7[:, 0]
y7 = data7[:, 1]
x8 = data8[:, 0]
y8 = data8[:, 1]

# Calculate moving average with window size of 3
ma_y7 = moving_average(y7, 100)
ma_y8 = moving_average(y8, 100)

# Output results to file
with open('4/1_2_goodput_MA.txt', 'w') as f:
    for i in range(len(ma_y7)):
        f.write(f'{x7[i]} {ma_y7[i]}\n')
with open('4/4_5_goodput_MA.txt', 'w') as f:
    for i in range(len(ma_y8)):
        f.write(f'{x8[i]} {ma_y8[i]}\n')