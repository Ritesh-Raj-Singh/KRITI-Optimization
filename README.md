# Steps to build and run the server:
- first clone the repo
- run make command in the repo directory
- it may show error regarding asio.h, it is because we are passing flags to use standalone asio source instead of boost lib
- fix it by running: sudo apt install libasio-dev
- after the build is completed successfully
- run the server_app executable
- the API will be live on port 5555 on your local network
# Testing the API
- we gonna test it with curl, we can send request via any other method too
- make sure the 3 files ( employees.csv, vehicles.csv, metadata.csv ) are present in your working directory
- NOTE: our api is handling the POST request at localhost:5555/upload
- run the curl command
- curl -X POST http://localhost:5555/upload   -F "employees=@employees.csv"   -F "vehicles=@vehicles.csv"   -F "metadata=@metadata.csv"
- u should get the response (if not then leave it)
- the logs will be printed in the server terminal
- respective logs for each algo will be saved in their respective directories

# NOTE: this is not the final API, there are some issues which can be fixed
