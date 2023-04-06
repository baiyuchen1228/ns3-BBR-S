import numpy as np
# Define moving average function
def moving_average(y, window_size):
    weights = np.repeat(1.0, window_size) / window_size
    return np.convolve(y, weights, mode='valid')
# Read in x and y data from file
try:
    data1 = np.loadtxt('1/10.1.1.1_49153_10.1.3.2_5000_rtt.txt')
    data2 = np.loadtxt('1/10.1.4.2_49153_10.1.5.2_5000_rtt.txt')
    x1 = data1[:, 0]
    y1 = data1[:, 1]
    x2 = data2[:, 0]
    y2 = data2[:, 1]
    ma_y1 = moving_average(y1, 500)
    ma_y2 = moving_average(y2, 500)

    # Output results to file
    with open('1/1_2_rtt_MA.txt', 'w') as f:
        for i in range(len(ma_y1)):
            f.write(f'{x1[i]} {ma_y1[i]}\n')
    with open('1/4_5_rtt_MA.txt', 'w') as f:
        for i in range(len(ma_y2)):
            f.write(f'{x2[i]} {ma_y2[i]}\n')
    data1 = np.loadtxt('1/10.1.1.1_49153_10.1.3.2_5000_goodput.txt')
    data2 = np.loadtxt('1/10.1.4.2_49153_10.1.5.2_5000_goodput.txt')
    x1 = data1[:, 0]
    y1 = data1[:, 1]
    x2 = data2[:, 0]
    y2 = data2[:, 1]
    ma_y1 = moving_average(y1, 100)
    ma_y2 = moving_average(y2, 100)
    with open('1/1_2_goodput_MA.txt', 'w') as f:
        for i in range(len(ma_y1)):
            f.write(f'{x1[i]} {ma_y1[i]}\n')
    with open('1/4_5_goodput_MA.txt', 'w') as f:
        for i in range(len(ma_y2)):
            f.write(f'{x2[i]} {ma_y2[i]}\n')
except:
    None

# 2
try:
    data1 = np.loadtxt('2/10.1.1.1_49153_10.1.3.2_5000_rtt.txt')
    data2 = np.loadtxt('2/10.1.4.2_49153_10.1.5.2_5000_rtt.txt')
    x1 = data1[:, 0]
    y1 = data1[:, 1]
    x2 = data2[:, 0]
    y2 = data2[:, 1]
    ma_y1 = moving_average(y1, 500)
    ma_y2 = moving_average(y2, 500)

    # Output results to file
    with open('2/1_2_rtt_MA.txt', 'w') as f:
        for i in range(len(ma_y1)):
            f.write(f'{x1[i]} {ma_y1[i]}\n')
    with open('2/4_5_rtt_MA.txt', 'w') as f:
        for i in range(len(ma_y2)):
            f.write(f'{x2[i]} {ma_y2[i]}\n')
    data1 = np.loadtxt('2/10.1.1.1_49153_10.1.3.2_5000_goodput.txt')
    data2 = np.loadtxt('2/10.1.4.2_49153_10.1.5.2_5000_goodput.txt')
    x1 = data1[:, 0]
    y1 = data1[:, 1]
    x2 = data2[:, 0]
    y2 = data2[:, 1]
    ma_y1 = moving_average(y1, 100)
    ma_y2 = moving_average(y2, 100)
    with open('2/1_2_goodput_MA.txt', 'w') as f:
        for i in range(len(ma_y1)):
            f.write(f'{x1[i]} {ma_y1[i]}\n')
    with open('2/4_5_goodput_MA.txt', 'w') as f:
        for i in range(len(ma_y2)):
            f.write(f'{x2[i]} {ma_y2[i]}\n')
except:
    None
# 3
try:
    data1 = np.loadtxt('3/10.1.1.1_49153_10.1.3.2_5000_rtt.txt')
    data2 = np.loadtxt('3/10.1.4.2_49153_10.1.5.2_5000_rtt.txt')
    x1 = data1[:, 0]
    y1 = data1[:, 1]
    x2 = data2[:, 0]
    y2 = data2[:, 1]
    ma_y1 = moving_average(y1, 500)
    ma_y2 = moving_average(y2, 500)

    # Output results to file
    with open('3/1_2_rtt_MA.txt', 'w') as f:
        for i in range(len(ma_y1)):
            f.write(f'{x1[i]} {ma_y1[i]}\n')
    with open('3/4_5_rtt_MA.txt', 'w') as f:
        for i in range(len(ma_y2)):
            f.write(f'{x2[i]} {ma_y2[i]}\n')
    data1 = np.loadtxt('3/10.1.1.1_49153_10.1.3.2_5000_goodput.txt')
    data2 = np.loadtxt('3/10.1.4.2_49153_10.1.5.2_5000_goodput.txt')
    x1 = data1[:, 0]
    y1 = data1[:, 1]
    x2 = data2[:, 0]
    y2 = data2[:, 1]
    ma_y1 = moving_average(y1, 100)
    ma_y2 = moving_average(y2, 100)
    with open('3/1_2_goodput_MA.txt', 'w') as f:
        for i in range(len(ma_y1)):
            f.write(f'{x1[i]} {ma_y1[i]}\n')
    with open('3/4_5_goodput_MA.txt', 'w') as f:
        for i in range(len(ma_y2)):
            f.write(f'{x2[i]} {ma_y2[i]}\n')
except:
    None

# 4
try:
    data1 = np.loadtxt('4/10.1.1.1_49153_10.1.3.2_5000_rtt.txt')
    data2 = np.loadtxt('4/10.1.4.2_49153_10.1.5.2_5000_rtt.txt')
    x1 = data1[:, 0]
    y1 = data1[:, 1]
    x2 = data2[:, 0]
    y2 = data2[:, 1]
    ma_y1 = moving_average(y1, 500)
    ma_y2 = moving_average(y2, 500)

    # Output results to file
    with open('4/1_2_rtt_MA.txt', 'w') as f:
        for i in range(len(ma_y1)):
            f.write(f'{x1[i]} {ma_y1[i]}\n')
    with open('4/4_5_rtt_MA.txt', 'w') as f:
        for i in range(len(ma_y2)):
            f.write(f'{x2[i]} {ma_y2[i]}\n')
    data1 = np.loadtxt('4/10.1.1.1_49153_10.1.3.2_5000_goodput.txt')
    data2 = np.loadtxt('4/10.1.4.2_49153_10.1.5.2_5000_goodput.txt')
    x1 = data1[:, 0]
    y1 = data1[:, 1]
    x2 = data2[:, 0]
    y2 = data2[:, 1]
    ma_y1 = moving_average(y1, 100)
    ma_y2 = moving_average(y2, 100)
    with open('4/1_2_goodput_MA.txt', 'w') as f:
        for i in range(len(ma_y1)):
            f.write(f'{x1[i]} {ma_y1[i]}\n')
    with open('4/4_5_goodput_MA.txt', 'w') as f:
        for i in range(len(ma_y2)):
            f.write(f'{x2[i]} {ma_y2[i]}\n')
except:
    None