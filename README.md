Word counter program made to parse big directories using multithreading.
Version 1.01

Config file example:
../../Guttenberg_5/0/ - directory to parse (! Program uses folder directory, not single file path !)
./res_a.txt - Output file with results sorted in alphabetical order
./res_n.txt - Output file with results sorted by word count
4 - number of parsing threads
4 - number of merging threads
10000 - queue size

Implementation notes:
1) Program parses only one file in archive
2) There is no file extension filter in archive
3) There is no file size control inside an archive (program checks archive size to not exceed the limit of 10 000 000 bytes)
4) Merge does not use the MapReduce algorithm

Notes:
1) There is a Python script to verify the results of different runs for consistency (it uses res_n files)
2) Program were tested on the machine with Intel i5-4460 + 8 Gb RAM

----------------------------------------------------------------------------------------------------------------------------------

Це реалізація програми для парсингу директорії по кількості слів.

Приклад конфіг файлу:
../../Guttenberg_5/0/ - директорія, для парсингу (! Програма приймає адресу директорії, а не адресу одного файлу !)
./res_a.txt - Файл виводу результату в алфавітному порядку
./res_n.txt - Файл виводу результату по кількості
4 - кількість потоків для парсингу
4 - кількість потоків для мерджу
10000 - розмір черги

Особливості реалізації:
1) Програма читає лише 1 файл в архіві
2) Нема фільтру типів файлів у архіві
3) Нема перевірки розміру файлу у архіві (перевіряється лише сам архів, або тхт файл, щоб не перевищував 10 000 000 байт)
4) Мердж реалізований не способом MapReduce

Більшість з вищеперечислених проблем я не зміг розв'язати через libarchive, просто не знайшов інструментів.

Програма тестувалась скриптом на директорії guttenberg/1.

Примітка:
1) Скрипт перевіряє не повністю файли результату на ідентичність, лише перші 1000 слів у файлі res_n
2) Проргама тестувалась на Intel i5-4460 + 8 Gb RAM