# Distributed Enrollment System
## Source Code and Build / Compilation Steps

## 1. Source Code

The project source code contains the following main files:

```text
DistributedEnrollmentSystem/
│
├── CMakeLists.txt
├── enrollment.proto
│
├── config.h
├── config.cpp
├── config.ini
│
├── admin_server.cpp
├── student_server.cpp
├── faculty_server.cpp
│
├── web_server.cpp
├── client.cpp
│
└── server.cpp
```

### Main Source Files

**`enrollment.proto`**

Defines the gRPC services and messages used for communication between the distributed nodes.

Services include:

- `AdminService`
- `StudentService`
- `FacultyService`
- `CoreService`

---

**`admin_server.cpp`**

Implements the Admin and Core services.

It handles:

- Authentication
- JWT generation
- Session management
- Student creation
- Course creation
- Enrollment
- Grade management
- Shared application data

The Admin/Core server runs on port:

```text
50051
```

---

**`student_server.cpp`**

Implements the Student service.

It handles:

- Student login/logout
- Viewing courses
- Course enrollment
- Viewing grades

The Student server communicates with the Admin/Core server through gRPC.

The Student server runs on:

```text
50052
```

---

**`faculty_server.cpp`**

Implements the Faculty service.

It handles:

- Faculty login/logout
- Uploading student grades

The Faculty server communicates with the Admin/Core server through gRPC.

The Faculty server runs on:

```text
50053
```

---

**`web_server.cpp`**

Implements the web/View node.

It handles HTTP requests from the browser and communicates with the backend services through gRPC.

The Web server runs on:

```text
8080
```

---

**`config.h` and `config.cpp`**

Handle loading the network configuration from `config.ini`.

This allows the IP addresses of the distributed nodes to be changed without modifying the source code.

---

**`config.ini`**

Contains the IP addresses and ports of the distributed nodes.

Example:

```ini
[NETWORK]

WEB_SERVER_IP=<WEB_SERVER_IP>
WEB_SERVER_PORT=8080

ADMIN_SERVER_IP=<ADMIN_SERVER_IP>
ADMIN_SERVER_PORT=50051

STUDENT_SERVER_IP=<STUDENT_SERVER_IP>
STUDENT_SERVER_PORT=50052

FACULTY_SERVER_IP=<FACULTY_SERVER_IP>
FACULTY_SERVER_PORT=50053
```

The IP addresses should be changed to match the network being used.

---

**`client.cpp`**

Provides a simple gRPC client used for testing the Student service.

It can be used to verify that the Student server is reachable and that login requests are working.

---

**`CMakeLists.txt`**

Contains the CMake build configuration.

It:

- Configures the C++ project
- Generates the gRPC/Protocol Buffer source files
- Builds the server executables
- Links the required gRPC libraries
- Builds the test client
- Copies the network configuration

---

# 2. Requirements

The following software is required to compile the project:

- Windows
- Visual Studio Code or another C++ IDE
- CMake
- MinGW / g++
- gRPC
- Protocol Buffers
- Git
- Required MinGW runtime DLLs when running the compiled executables

The project was developed using C++17.

---

# 3. Project Directory

The project should be placed in a directory such as:

```text
D:\School\STDISCIM\GRPC Test
```

The exact directory can be changed as long as the CMake paths to the gRPC installation are updated if necessary.

---

# 4. Configure the Project

Open Command Prompt and enter the project directory:

```cmd
cd /d "D:\School\STDISCIM\GRPC Test"
```

Create or enter the build directory:

```cmd
mkdir build
cd build
```

If the `build` directory already exists, simply enter it:

```cmd
cd /d "D:\School\STDISCIM\GRPC Test\build"
```

---

# 5. Run CMake

Configure the project using MinGW Makefiles:

```cmd
cmake .. -G "MinGW Makefiles"
```

If configuration succeeds, CMake will generate the required build files.

---

# 6. Compile the Project

Build all targets:

```cmd
cmake --build . --parallel 4
```

The build process generates the Protocol Buffer and gRPC source files and then compiles the servers and client.

---

# 7. Build Individual Components

Individual programs can also be built separately.

### Admin/Core Server

```cmd
cmake --build . --target admin_server --parallel 4
```

### Student Server

```cmd
cmake --build . --target student_server --parallel 4
```

### Faculty Server

```cmd
cmake --build . --target faculty_server --parallel 4
```

### Web Server

```cmd
cmake --build . --target web_server --parallel 4
```

### Test Client

```cmd
cmake --build . --target client --parallel 4
```

---

# 8. Generated Executables

After compilation, the build directory contains:

```text
admin_server.exe
student_server.exe
faculty_server.exe
web_server.exe
client.exe
```

The generated gRPC/Protocol Buffer files are also located in the build directory.

---

# 9. Runtime DLLs

When using MinGW, the following DLLs may be required beside the executables:

```text
libgcc_s_seh-1.dll
libstdc++-6.dll
libwinpthread-1.dll
```

If the target computer does not already have the required runtime libraries, copy these DLLs into the same directory as the executable.

---

# 10. Network Configuration

Before running the system on another network, update:

```text
config.ini
```

with the IP addresses of the computers being used.

Example:

```ini
[NETWORK]

WEB_SERVER_IP=192.168.1.10
WEB_SERVER_PORT=8080

ADMIN_SERVER_IP=192.168.1.20
ADMIN_SERVER_PORT=50051

STUDENT_SERVER_IP=192.168.1.30
STUDENT_SERVER_PORT=50052

FACULTY_SERVER_IP=192.168.1.40
FACULTY_SERVER_PORT=50053
```

The same `config.ini` can be placed beside the executables on each node.

---

# 11. Running the System

The backend services should be started first.

### Admin/Core

```cmd
admin_server.exe
```

### Student

```cmd
student_server.exe
```

### Faculty

```cmd
faculty_server.exe
```

### Web

```cmd
web_server.exe
```

The normal execution order is:

```text
Admin/Core
      ↓
Student
      ↓
Faculty
      ↓
Web
```

---

# 12. Testing the Network

The backend ports are:

```text
Admin/Core → 50051
Student    → 50052
Faculty    → 50053
Web        → 8080
```

Connectivity can be tested using PowerShell:

```powershell
Test-NetConnection <ADMIN_IP> -Port 50051
```

```powershell
Test-NetConnection <STUDENT_IP> -Port 50052
```

```powershell
Test-NetConnection <FACULTY_IP> -Port 50053
```

A successful connection should show:

```text
TcpTestSucceeded : True
```

---

# 13. Running the Web Application

After the Web server is running, access the system through:

```text
http://<WEB_SERVER_IP>:8080
```

For example:

```text
http://192.168.1.10:8080
```

---

# 14. Summary

The project is compiled using:

```cmd
cmake .. -G "MinGW Makefiles"
cmake --build . --parallel 4
```

The resulting system consists of separate distributed services:

```text
Web Server       : 8080
Admin/Core       : 50051
Student          : 50052
Faculty          : 50053
```

Communication between backend services uses gRPC, while the browser communicates with the Web/View node using HTTP.
