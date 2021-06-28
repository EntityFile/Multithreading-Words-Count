import sys
import subprocess
import numpy as np
import matplotlib.pyplot as plt
from alive_progress import alive_bar


def run_pocces(program):
    tmp = subprocess.Popen(program + str(sys.argv[2]), stdout=subprocess.PIPE)
    outcome = tmp.communicate()[0].decode("utf_8").strip().split("\n")

    time = outcome[0].split()[1]

    tmp.kill()

    return int(time)


def change_config(threads, filepath):
    f = open(filepath, "r")
    list_of_lines = f.readlines()
    list_of_lines[3] = str(threads)+'\n'

    f = open(filepath, "w")
    f.writelines(list_of_lines)
    f.close()


def create_graph(data, iterations, filename):
    columns = ('1 thread', '2 threads', '3 threads', '4 threads')

    n_rows = len(data)
    index = np.arange(len(columns)) + 0.3
    bar_width = 0.5

    y_offset = np.zeros(len(columns))

    cell_text = []
    for row in range(n_rows):
        plt.bar(index, data[row], bar_width, bottom=y_offset, color='gray')
        y_offset = y_offset + data[row]
        cell_text.append([x for x in y_offset])

    the_table = plt.table(cellText=cell_text,
                          colLabels=columns,
                          loc='bottom')
    plt.subplots_adjust(left=0.2, bottom=0.1)

    plt.ylabel('Time (seconds)')
    plt.xticks([])

    plt.title('Parsing directory for ' + iterations + ' iterations')

    fig = plt.gcf()
    plt.savefig('pyplot-table-original.png',
                bbox_inches='tight',
                dpi=150
                )


def main():
    print("Calculating for " + sys.argv[1] + " runs:\n")
    min_list = [sys.maxsize for i in range(4)]
    result_list = []

    with alive_bar(int(sys.argv[1])) as bar:
        for i in range(int(sys.argv[1])):
            bar()
            for k in range(1, 5):
                change_config(k, sys.argv[2])
                time = run_pocces("./WordsCount2.exe ")
                f = open("./res_n.txt", "r", encoding="utf_8")

                res_local = []

                while True:
                    line = f.readline()
                    if line:
                        res_local.append(line)
                    else:
                        break

                result_list.append(res_local)

                if time < min_list[k - 1]:
                    min_list[k - 1] = time

    print("Work time (microseconds):")
    for j in range(len(min_list)):
        print(str(j+1) + " Thread: " + str(min_list[j]))

    results_are_same = 1

    if (len(result_list[0]) == len(result_list[1])) and (len(result_list[1]) == len(result_list[2])) and (
            len(result_list[2]) == len(result_list[3])):
        for id in range(len(result_list[0])):
            if (result_list[0][id] != result_list[1][id]) or (result_list[1][id] != result_list[2][id]) or (
                    result_list[2][id] != result_list[3][id]):
                results_are_same = 0
                print("Not all results are the same")
                break

    if results_are_same:
        print("All results are the same")

    create_graph([min_list], str(sys.argv[1]), str(sys.argv[2]))


main()
