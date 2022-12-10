test_count = 5
segment_count = 10
processes = [1, 2, 4, 8, 16, 32, 64, 80]

file_list = [str(i) + '.txt' for i in range(1, test_count + 1)]
data = []
for file in file_list:
    with open('tests/united_tests/' + file, "r") as f:
        while text := f.readline():
            if text.find('The output (if any) follows:') == -1:
                continue
            else:
                f.readline()  # там пустая строка
                for i in range(segment_count * len(processes)):
                    text = f.readline()
                    struct = text.split()
                    data.append([int(struct[2][:-1]), int(struct[5][:-1]), float(struct[8][:-1]), float(struct[10])])

average_data = {}
for struct in data:
    if (struct[0], struct[1]) not in average_data:
        average_data[(struct[0], struct[1])] = struct
    else:
        average_data[(struct[0], struct[1])][2] += struct[2]
        average_data[(struct[0], struct[1])][3] += struct[3]

for key in average_data.keys():
    average_data[key][2] /= test_count
    average_data[key][3] /= test_count
    print(average_data[key], end=',\n')
