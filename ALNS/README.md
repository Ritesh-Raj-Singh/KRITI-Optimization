Compile Command -> 
g++ -std=c++17 -O3 main.cpp ALNS.cpp CSVReader.cpp CostFunction.cpp DestroyOperators.cpp RepairOperators.cpp -o vrp_runner.exe


.\vrp_runner.exe ..\Vehicle2.csv ..\emp2.csv
