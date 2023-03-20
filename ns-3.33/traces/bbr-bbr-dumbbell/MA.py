import numpy as np

# Read in x and y data from file
data1 = np.loadtxt('1/10.1.1.1_49153_10.1.3.2_5000_rtt.txt')
data2 = np.loadtxt('1/10.1.4.2_49153_10.1.5.2_5000_rtt.txt')
data3 = np.loadtxt('2/10.1.1.1_49153_10.1.3.2_5000_rtt.txt')
data4 = np.loadtxt('2/10.1.4.2_49153_10.1.5.2_5000_rtt.txt')
data5 = np.loadtxt('3/10.1.1.1_49153_10.1.3.2_5000_rtt.txt')
data6 = np.loadtxt('3/10.1.4.2_49153_10.1.5.2_5000_rtt.txt')
data7 = np.loadtxt('4/10.1.1.1_49153_10.1.3.2_5000_rtt.txt')
data8 = np.loadtxt('4/10.1.4.2_49153_10.1.5.2_5000_rtt.txt')

# Extract x and y data from array
x1 = data1[:, 0]
y1 = data1[:, 1]
x2 = data2[:, 0]
y2 = data2[:, 1]
x3 = data3[:, 0]
y3 = data3[:, 1]
x4 = data4[:, 0]
y4 = data4[:, 1]
x5 = data5[:, 0]
y5 = data5[:, 1]
x6 = data6[:, 0]
y6 = data6[:, 1]
x7 = data7[:, 0]
y7 = data7[:, 1]
x8 = data8[:, 0]
y8 = data8[:, 1]

# Define moving average function
def moving_average(y, window_size):
    weights = np.repeat(1.0, window_size) / window_size
    return np.convolve(y, weights, mode='valid')

# Calculate moving average with window size of 3
ma_y1 = moving_average(y1, 500)
ma_y2 = moving_average(y2, 500)
ma_y3 = moving_average(y3, 500)
ma_y4 = moving_average(y4, 500)
ma_y5 = moving_average(y5, 500)
ma_y6 = moving_average(y6, 500)
ma_y7 = moving_average(y7, 500)
ma_y8 = moving_average(y8, 500)

# Output results to file
with open('1/1_2_rtt_MA.txt', 'w') as f:
    for i in range(len(ma_y1)):
        f.write(f'{x1[i]} {ma_y1[i]}\n')
with open('1/4_5_rtt_MA.txt', 'w') as f:
    for i in range(len(ma_y2)):
        f.write(f'{x2[i]} {ma_y2[i]}\n')
with open('2/1_2_rtt_MA.txt', 'w') as f:
    for i in range(len(ma_y3)):
        f.write(f'{x3[i]} {ma_y3[i]}\n')
with open('2/4_5_rtt_MA.txt', 'w') as f:
    for i in range(len(ma_y4)):
        f.write(f'{x4[i]} {ma_y4[i]}\n')
with open('3/1_2_rtt_MA.txt', 'w') as f:
    for i in range(len(ma_y5)):
        f.write(f'{x5[i]} {ma_y5[i]}\n')
with open('3/4_5_rtt_MA.txt', 'w') as f:
    for i in range(len(ma_y6)):
        f.write(f'{x6[i]} {ma_y6[i]}\n')
with open('4/1_2_rtt_MA.txt', 'w') as f:
    for i in range(len(ma_y7)):
        f.write(f'{x7[i]} {ma_y7[i]}\n')
with open('4/4_5_rtt_MA.txt', 'w') as f:
    for i in range(len(ma_y8)):
        f.write(f'{x8[i]} {ma_y8[i]}\n')

# Read in x and y data from file
data1 = np.loadtxt('1/10.1.1.1_49153_10.1.3.2_5000_goodput.txt')
data2 = np.loadtxt('1/10.1.4.2_49153_10.1.5.2_5000_goodput.txt')
data3 = np.loadtxt('2/10.1.1.1_49153_10.1.3.2_5000_goodput.txt')
data4 = np.loadtxt('2/10.1.4.2_49153_10.1.5.2_5000_goodput.txt')
data5 = np.loadtxt('3/10.1.1.1_49153_10.1.3.2_5000_goodput.txt')
data6 = np.loadtxt('3/10.1.4.2_49153_10.1.5.2_5000_goodput.txt')
data7 = np.loadtxt('4/10.1.1.1_49153_10.1.3.2_5000_goodput.txt')
data8 = np.loadtxt('4/10.1.4.2_49153_10.1.5.2_5000_goodput.txt')

# Extract x and y data from array
x1 = data1[:, 0]
y1 = data1[:, 1]
x2 = data2[:, 0]
y2 = data2[:, 1]
x3 = data3[:, 0]
y3 = data3[:, 1]
x4 = data4[:, 0]
y4 = data4[:, 1]
x5 = data5[:, 0]
y5 = data5[:, 1]
x6 = data6[:, 0]
y6 = data6[:, 1]
x7 = data7[:, 0]
y7 = data7[:, 1]
x8 = data8[:, 0]
y8 = data8[:, 1]

# Calculate moving average with window size of 3
ma_y1 = moving_average(y1, 100)
ma_y2 = moving_average(y2, 100)
ma_y3 = moving_average(y3, 100)
ma_y4 = moving_average(y4, 100)
ma_y5 = moving_average(y5, 100)
ma_y6 = moving_average(y6, 100)
ma_y7 = moving_average(y7, 100)
ma_y8 = moving_average(y8, 100)

# Output results to file
with open('1/1_2_goodput_MA.txt', 'w') as f:
    for i in range(len(ma_y1)):
        f.write(f'{x1[i]} {ma_y1[i]}\n')
with open('1/4_5_goodput_MA.txt', 'w') as f:
    for i in range(len(ma_y2)):
        f.write(f'{x2[i]} {ma_y2[i]}\n')
with open('2/1_2_goodput_MA.txt', 'w') as f:
    for i in range(len(ma_y3)):
        f.write(f'{x3[i]} {ma_y3[i]}\n')
with open('2/4_5_goodput_MA.txt', 'w') as f:
    for i in range(len(ma_y4)):
        f.write(f'{x4[i]} {ma_y4[i]}\n')
with open('3/1_2_goodput_MA.txt', 'w') as f:
    for i in range(len(ma_y5)):
        f.write(f'{x5[i]} {ma_y5[i]}\n')
with open('3/4_5_goodput_MA.txt', 'w') as f:
    for i in range(len(ma_y6)):
        f.write(f'{x6[i]} {ma_y6[i]}\n')
with open('4/1_2_goodput_MA.txt', 'w') as f:
    for i in range(len(ma_y7)):
        f.write(f'{x7[i]} {ma_y7[i]}\n')
with open('4/4_5_goodput_MA.txt', 'w') as f:
    for i in range(len(ma_y8)):
        f.write(f'{x8[i]} {ma_y8[i]}\n')