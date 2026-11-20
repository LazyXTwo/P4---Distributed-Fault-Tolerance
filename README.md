# Distributed Enrollment System

A distributed web-based university enrollment system built using C++, gRPC, Protocol Buffers, and a browser-based web interface.

The system uses multiple networked nodesservices so that the View, AdminCore, Student, and Faculty responsibilities are separated. Communication between backend nodes uses gRPC.

---

# 1. Project Overview

The system provides

- Student loginlogout
- Faculty loginlogout
- Administrator loginlogout
- JWT-based authentication
- Session tracking
- Course viewing
- Student enrollment
- Duplicate enrollment prevention
- Student grade viewing
- Faculty grade uploading
- Administrator student creation
- Administrator course creation
- Multiple independent backend services
- Browser-based web interface
- Inter-node communication using gRPC
- Node failure isolation
- Configurable IP addresses and ports

The project is designed so that Student and Faculty services can later be moved to separate physical PCs without changing the application logic.

---

# 2. Current Architecture

At the moment, all backend services are running on PC2 because additional PCs are not currently being used.

```text
                         ┌─────────────────────┐
                         │      Browser        │
                         │      PC1            │
                         └──────────┬──────────┘
                                    │
                              HTTP 8080
                                    │
                                    ▼
                         ┌─────────────────────┐
                         │    Web Server       │
                         │       PC1           │
                         │ 192.168.100.48080  │
                         └──────┬──────┬───────┘
                                │      │
                ┌───────────────┘      └────────────────┐
                │                                       │
                ▼                                       ▼
       ┌─────────────────┐                    ┌─────────────────┐
       │  Admin Service  │                    │ Student Service │
       │      PC2        │                    │      PC2        │
       │ 50051          │                    │ 50052          │
       └────────┬────────┘                    └────────┬────────┘
                │                                      │
                │             gRPC                     │
                │                                      │
                └───────────────┬──────────────────────┘
                                │
                                ▼
                       ┌─────────────────┐
                       │  Core Service   │
                       │      PC2        │
                       │   50051        │
                       │ Shared Data     │
                       └────────┬────────┘
                                │
                                │
                                ▼
                       ┌─────────────────┐
                       │ Faculty Service │
                       │      PC2        │
                       │     50053      │
                       └─────────────────┘
```

The AdminCore server is the central authority for shared data.

The Student and Faculty servers act as separate service nodes and communicate with the Core service using gRPC.

---

# 3. Planned Multi-PC Architecture

The configuration was designed so the system can later be moved to separate PCs.

Current

```text
PC1
192.168.100.4
│
└── Web Server 8080

PC2
192.168.100.22
│
├── AdminCore 50051
├── Student    50052
└── Faculty    50053
```

Future

```text
PC1
192.168.100.4
│
└── Web Server 8080
        │
        ├───────────────► PC2
        │                AdminCore 50051
        │
        ├───────────────► PC3
        │                Student 50052
        │
        └───────────────► PC4
                         Faculty 50053
```

Only `config.ini` needs to be changed when moving services to other machines.

---

# 4. Project Structure

The main project directory is

```text
DSchoolSTDISCIMGRPC Test
```

Important files

```text
GRPC Test
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
├── server.cpp
│
└── build
```

Generated files such as

```text
enrollment.pb.cc
enrollment.pb.h
enrollment.grpc.pb.cc
enrollment.grpc.pb.h
```

are generated automatically by CMakeprotoc.

---

# 5. Important Source Files

## `enrollment.proto`

Defines all gRPC services and messages.

Services

```text
AdminService
StudentService
FacultyService
CoreService
```

### AdminService

Handles administrator-facing functionality

```text
Login
Logout
AddStudent
AddCourse
```

### StudentService

Handles student functionality

```text
Login
Logout
GetCourses
Enroll
GetGrades
```

### FacultyService

Handles faculty functionality

```text
Login
Logout
UploadGrade
```

### CoreService

Contains the shared backend logic

```text
Login
Logout
GetCourses
Enroll
AddStudent
AddCourse
UploadGrade
GetGrades
```

The CoreService is the authoritative source for shared application data.

---

# 6. AdminCore Server

File

```text
admin_server.cpp
```

Executable

```text
admin_server.exe
```

Port

```text
50051
```

Responsibilities

- Authentication
- JWT creation
- Session management
- Student management
- Course management
- Enrollment management
- Grade management
- Shared data management
- CoreService implementation
- AdminService implementation

The server implements both

```cpp
CoreServiceService
```

and

```cpp
AdminServiceService
```

This allows the same server to provide the shared Core API and the Admin API.

---

# 7. Student Server

File

```text
student_server.cpp
```

Executable

```text
student_server.exe
```

Port

```text
50052
```

The Student server exposes

```text
StudentService
```

It does not own a separate copy of the application's shared data.

Instead, it forwards requests to

```text
CoreService
```

on the AdminCore server.

This keeps authentication, courses, enrollments, and grades consistent.

---

# 8. Faculty Server

File

```text
faculty_server.cpp
```

Executable

```text
faculty_server.exe
```

Port

```text
50053
```

The Faculty server exposes

```text
FacultyService
```

It forwards faculty requests to

```text
CoreService
```

on the AdminCore server.

This allows the Faculty service to be moved to another PC later without duplicating the application's shared state.

---

# 9. Web Server

File

```text
web_server.cpp
```

Executable

```text
web_server.exe
```

The web server runs on PC1

```text
192.168.100.48080
```

The browser connects to the web server using HTTP.

The web server then communicates with the backend services using gRPC.

The browser never directly communicates with gRPC.

Architecture

```text
Browser
   │
   │ HTTP
   ▼
Web Server
   │
   ├── gRPC → AdminService
   │
   ├── gRPC → StudentService
   │
   └── gRPC → FacultyService
```

---

# 10. Configuration

File

```text
config.ini
```

Current configuration

```ini
[NETWORK]

WEB_SERVER_IP=192.168.100.4
WEB_SERVER_PORT=8080

ADMIN_SERVER_IP=192.168.100.22
ADMIN_SERVER_PORT=50051

STUDENT_SERVER_IP=192.168.100.22
STUDENT_SERVER_PORT=50052

FACULTY_SERVER_IP=192.168.100.22
FACULTY_SERVER_PORT=50053
```

The application reads these values instead of hardcoding network addresses.

---

# 11. Moving a Service to Another PC

For example, if the Student server is moved to

```text
192.168.100.23
```

change

```ini
STUDENT_SERVER_IP=192.168.100.23
```

The Student server's own configuration should still point its Core connection toward

```ini
ADMIN_SERVER_IP=192.168.100.22
ADMIN_SERVER_PORT=50051
```

The Faculty server works the same way.

This means the architecture can evolve from

```text
PC1 → PC2
```

into

```text
PC1 → PC2
   ↘ PC3
   ↘ PC4
```

without rewriting the application.

---

# 12. Authentication

The system uses JWT-based authentication.

JWT stands for

```text
JSON Web Token
```

The JWT is generated by the Core server after successful login.

The token contains information such as

```text
sub
role
jti
iat
exp
```

Where

```text
sub = username
role = account role
jti = unique session ID
iat = issued-at time
exp = expiration time
```

The JWT is signed using

```text
HMAC-SHA256
```

The token lifetime is

```text
3600 seconds
```

or

```text
1 hour
```

---

# 13. Server-Side Session Tracking

JWTs are also tracked on the server.

Each active token has a corresponding session entry.

The server maintains session information including

```text
JTI
username
role
expiration
```

This allows logout to immediately invalidate a token.

Without server-side session tracking, a JWT could normally remain valid until its expiration time.

---

# 14. Logout

When a user logs out

1. The token is received.
2. The JWT is validated.
3. The JTI is found.
4. The server removes the session.
5. Future requests using that session are rejected.

Therefore, logout immediately invalidates the session.

---

# 15. Thread Safety

The Core server contains shared application data.

Examples

```text
students
faculty
courses
sessions
enrollments
grades
```

Access to shared state is protected using

```cpp
stdmutex data_mutex;
```

Operations involving shared data lock the mutex before modifying or reading protected state.

This prevents race conditions when multiple clients access the server simultaneously.

---

# 16. Important Deadlock Fix

The original server had an issue where session cleanup could attempt to lock the same mutex while the mutex was already held.

This was corrected so

```text
cleanup_expired_sessions()
```

manages its own mutex ownership.

The login process calls cleanup without already holding the same mutex.

This prevents a self-deadlock.

---

# 17. Initial Accounts

The current in-memory test data includes

## Students

```text
student1
password123
```

and

```text
student2
password123
```

## Faculty

```text
faculty1
password123
```

## Administrator

The administrator account is included in the Core server's authentication logic.

---

# 18. Initial Courses

The initial courses are

```text
CS101
Introduction to Computing
30 slots
```

```text
CS102
Data Structures
25 slots
```

```text
CS103
Database Systems
20 slots
```

---

# 19. Enrollment

Students can

1. Log in.
2. View available courses.
3. Select a course.
4. Enroll.

The Core server verifies

- The JWT is valid.
- The account is a student.
- The course exists.
- The student has not already enrolled.
- Slots remain available.

When enrollment succeeds, the course's available slot count is reduced.

---

# 20. Duplicate Enrollment Protection

A student cannot enroll in the same course twice.

Example

```text
Student 1 → CS101
```

First request

```text
Enrollment successful
```

Second request

```text
Enrollment rejected
```

This check is performed while holding the shared data mutex, preventing race conditions.

---

# 21. Grades

Faculty can upload grades.

The server verifies

- JWT validity
- Faculty role
- Student existence
- Course existence
- Student enrollment
- Grade range

Valid grades are

```text
0–100
```

A grade outside this range is rejected.

For example

```text
90
```

is valid.

While

```text
101
```

and

```text
-1
```

are invalid.

Students can view their previous grades through

```text
GetGrades
```

---

# 22. Current Data Storage

The current implementation uses

```text
in-memory storage
```

Data is stored in C++ containers while the server is running.

Examples include

```cpp
stdunordered_map
stdvector
```

This means data is lost when the Core server shuts down.

SQLite persistence was investigated but is not currently part of the final implementation.

A future version can add persistent database storage.

---

# 23. gRPC Communication

gRPC is used for communication between nodes.

The `.proto` file defines the RPC interface.

Example

```text
Student Server
      │
      │ gRPC
      ▼
Core Server
```

For example

```text
Student Login
      ↓
StudentService.Login
      ↓
CoreService.Login
      ↓
Core authentication logic
      ↓
LoginReply
```

The same architecture is used for enrollment, grades, and other operations.

---

# 24. Why Separate Services Are Used

The application is not one large monolithic server.

Instead

```text
Admin
Student
Faculty
```

are separate services.

This satisfies the distributed-system requirement and makes it possible to deploy them independently.

For example

```text
Student server down
```

does not automatically mean

```text
Faculty server down
```

The services can be restarted or moved independently.

---

# 25. Node Failure Behavior

The system was specifically tested for node failure.

## Student server failure

If

```text
StudentService 50052
```

is stopped

```text
Student features fail
```

while other backend services can remain available.

## Faculty server failure

If

```text
FacultyService 50053
```

is stopped

```text
Faculty features fail
```

while other services can continue operating.

## AdminCore failure

If

```text
AdminCore 50051
```

is stopped

```text
Shared backend operations fail
```

because both Student and Faculty servers depend on the Core service for shared data.

This demonstrates the dependency relationship between the services.

---

# 26. Development Environment

The project was developed on Windows using

```text
Windows
VS Code
CMake
MinGW
g++
gRPC
Protocol Buffers
```

Compiler

```text
DMSYS2ucrt64binc++.exe
```

gRPC source

```text
Dgrpcgrpc
```

gRPC build

```text
Dgrpcgrpccmakebuild
```

gRPC installation

```text
Dgrpcinstall
```

---

# 27. Building the Project

Open Command Prompt.

Go to the project

```cmd
cd d DSchoolSTDISCIMGRPC Test
```

Enter the build directory

```cmd
cd build
```

Configure

```cmd
cmake .. -G MinGW Makefiles
```

Build everything

```cmd
cmake --build . --parallel 4
```

Specific targets can also be built.

For example

```cmd
cmake --build . --target admin_server --parallel 4
```

```cmd
cmake --build . --target student_server --parallel 4
```

```cmd
cmake --build . --target faculty_server --parallel 4
```

```cmd
cmake --build . --target web_server --parallel 4
```

```cmd
cmake --build . --target client --parallel 4
```

---

# 28. Generated Executables

After building, the build directory contains

```text
admin_server.exe
student_server.exe
faculty_server.exe
web_server.exe
client.exe
```

The old

```text
server.exe
```

is retained as a backupreference from the original architecture.

It should not be used for the current distributed architecture.

The current AdminCore server is

```text
admin_server.exe
```

---

# 29. Running the Backend

On PC2

Start

```cmd
admin_server.exe
```

Then

```cmd
student_server.exe
```

Then

```cmd
faculty_server.exe
```

The three services listen on

```text
50051
50052
50053
```

respectively.

---

# 30. Running the Web Server

On PC1

```cmd
web_server.exe
```

The web server listens on

```text
192.168.100.48080
```

A browser on the network can then access the web application through the PC1 address.

---

# 31. Checking Ports

On PC2

```cmd
netstat -ano  findstr 50051
```

```cmd
netstat -ano  findstr 50052
```

```cmd
netstat -ano  findstr 50053
```

All three should show a listening process when the services are running.

---

# 32. Testing Network Connectivity

From PC1

```powershell
Test-NetConnection 192.168.100.22 -Port 50051
```

```powershell
Test-NetConnection 192.168.100.22 -Port 50052
```

```powershell
Test-NetConnection 192.168.100.22 -Port 50053
```

Expected result

```text
TcpTestSucceeded  True
```

for each port.

---

# 33. gRPC Client Test

The project also contains

```text
client.exe
```

The client reads

```text
config.ini
```

and connects to the Student service.

It tests

```text
StudentService.Login
```

using

```text
student1
password123
```

The client prints

```text
Success
Message
Role
Token length
```

This is useful for testing gRPC without involving the browser.

---

# 34. Complete Test Cycle

The system was tested using the following sequence.

## Test 1 — Student Login

Login

```text
student1
password123
```

Expected

```text
Login successful
```

---

## Test 2 — View Courses

Student views available courses.

Expected

```text
CS101
CS102
CS103
```

with their available slot counts.

---

## Test 3 — Enrollment

Enroll

```text
CS101
```

Expected

```text
Enrollment successful
```

---

## Test 4 — Duplicate Enrollment

Attempt to enroll again

```text
CS101
```

Expected

```text
Enrollment rejected
```

---

## Test 5 — Student Grades

Before a grade is uploaded, the student should have no grade for the course.

---

## Test 6 — Faculty Login

Login

```text
faculty1
password123
```

Expected

```text
Login successful
```

---

## Test 7 — Faculty Grade Upload

Upload

```text
Student student1
Course CS101
Grade 90
```

Expected

```text
Grade upload successful
```

---

## Test 8 — Invalid Grade

Try

```text
101
```

Expected

```text
Rejected
```

Try

```text
-1
```

Expected

```text
Rejected
```

---

## Test 9 — Student Grade Verification

Log in again as

```text
student1
```

View grades.

Expected

```text
CS101
90
```

---

## Test 10 — Administrator Login

Login using the administrator credentials.

Expected

```text
Login successful
```

---

## Test 11 — Add Student

Create a new student.

Expected

```text
Student added successfully
```

The newly created account should then be able to log in.

---

## Test 12 — Student Node Failure

Stop

```text
student_server.exe
```

Try to use student functionality.

Expected

```text
Student functionality fails
```

Other services should remain available.

Restart

```text
student_server.exe
```

---

## Test 13 — Faculty Node Failure

Stop

```text
faculty_server.exe
```

Try to use faculty functionality.

Expected

```text
Faculty functionality fails
```

Restart

```text
faculty_server.exe
```

---

## Test 14 — Core Node Failure

Stop

```text
admin_server.exe
```

Student and Faculty proxy requests should fail because they can no longer reach CoreService.

Restart

```text
admin_server.exe
```

Then restart StudentFaculty if necessary.

---

# 35. Important Service Dependency

The dependency structure is

```text
                AdminCore
                   │
          ┌────────┴────────┐
          │                 │
          ▼                 ▼
      Student            Faculty
```

The Student and Faculty services are not independent databases.

They use the Core server for shared application state.

Therefore

```text
AdminCore DOWN
       ↓
Student proxy requests fail
Faculty proxy requests fail
```

But

```text
Student DOWN
       ↓
Faculty still exists
Admin still exists
```

and

```text
Faculty DOWN
       ↓
Student still exists
Admin still exists
```

---

# 36. Security

The system currently provides

- Password-based login
- JWT authentication
- HMAC-SHA256 signatures
- Token expiration
- Server-side session tracking
- JTI-based logout invalidation
- Role validation
- Protected RPC operations

Roles include

```text
admin
student
faculty
```

Each operation verifies that the authenticated user has the appropriate role.

---

# 37. Role Permissions

## Administrator

Can

```text
Login
Logout
Add students
Add courses
```

## Student

Can

```text
Login
Logout
View courses
Enroll in courses
View grades
```

## Faculty

Can

```text
Login
Logout
Upload grades
```

---

# 38. Error Handling

gRPC calls check

```cpp
status.ok()
```

If communication fails, the caller receives the gRPC error.

This allows networkservice failures to be detected rather than silently ignored.

The web server can then return an appropriate error to the browser.

---

# 39. Why `config.ini` Is Important

Network addresses are intentionally not hardcoded throughout the application.

Instead

```text
config.ini
     ↓
config.cpp
     ↓
ServerConfig
     ↓
gRPC connections
```

This makes deployment much easier.

For example, changing

```ini
STUDENT_SERVER_IP=192.168.100.22
```

to

```ini
STUDENT_SERVER_IP=192.168.100.23
```

moves the expected Student service to another machine without modifying the Student service code.

---

# 40. Current PC Deployment

## PC1 — WebView Node

```text
IP
192.168.100.4

Port
8080

Executable
web_server.exe
```

## PC2 — Backend Node

```text
IP
192.168.100.22
```

AdminCore

```text
admin_server.exe
50051
```

Student

```text
student_server.exe
50052
```

Faculty

```text
faculty_server.exe
50053
```

---

# 41. Network Flow Examples

## Student Login

```text
Browser
   ↓ HTTP
Web Server
   ↓ gRPC
AdminCore
   ↓
JWT generated
   ↓
Web Server
   ↓ HTTP
Browser
```

## Student Enrollment

```text
Browser
   ↓
Web Server
   ↓ gRPC
Student Server 50052
   ↓ gRPC
Core Server 50051
   ↓
Enrollment
   ↓
Student Server
   ↓
Web Server
   ↓
Browser
```

## Faculty Grade Upload

```text
Browser
   ↓
Web Server
   ↓ gRPC
Faculty Server 50053
   ↓ gRPC
Core Server 50051
   ↓
Grade stored
   ↓
Faculty Server
   ↓
Web Server
   ↓
Browser
```

---

# 42. Why the Architecture Is Distributed

The system has multiple independently running processes communicating over the network.

These include

```text
Web server
AdminCore server
Student server
Faculty server
```

They communicate through

```text
HTTP
gRPC
```

The services can run on separate physical computers.

Therefore, the system is not simply a single-process application.

---

# 43. Current Limitations

The current implementation intentionally has some limitations.

## In-memory database

Restarting the Core server clears

```text
students
courses
enrollments
grades
sessions
```

unless they are part of the initial hardcoded data.

## Single Core server

There is currently one authoritative Core server.

If it goes down, shared operations become unavailable.

## No HTTPS

The current development web server uses HTTP.

Production deployment should use HTTPSTLS.

## No persistent database

SQLitedatabase persistence is not currently enabled.

---

# 44. Possible Future Improvements

Potential future improvements include

```text
SQLite persistence
Database connection pooling
Database replication
HTTPS
TLS for gRPC
Password hashing with a dedicated password KDF
Refresh tokens
More administrative features
Course deletion
Course editing
Student enrollment management
Faculty course assignment
Grade editing
Audit logging
Centralized logging
Service health monitoring
Automatic service discovery
Load balancing
Redundant Core servers
```

---

# 45. Important Files for Final Submission

The most important source files are

```text
CMakeLists.txt
enrollment.proto

config.h
config.cpp
config.ini

admin_server.cpp
student_server.cpp
faculty_server.cpp

web_server.cpp
client.cpp
```

The old

```text
server.cpp
```

can be retained as a backup but is not the active server architecture.

---

# 46. Quick Start

## PC2

Start

```cmd
admin_server.exe
```

Then

```cmd
student_server.exe
```

Then

```cmd
faculty_server.exe
```

Verify

```cmd
netstat -ano  findstr 50051
netstat -ano  findstr 50052
netstat -ano  findstr 50053
```

---

## PC1

Start

```cmd
web_server.exe
```

Open the web application from a browser using the PC1 web server address.

---

# 47. Rebuilding After Code Changes

From the project

```cmd
cd d DSchoolSTDISCIMGRPC Test
```

Then

```cmd
cd build
```

Configure

```cmd
cmake .. -G MinGW Makefiles
```

Build

```cmd
cmake --build . --parallel 4
```

For a specific server

```cmd
cmake --build . --target admin_server --parallel 4
```

```cmd
cmake --build . --target student_server --parallel 4
```

```cmd
cmake --build . --target faculty_server --parallel 4
```

```cmd
cmake --build . --target web_server --parallel 4
```

---

# 48. Final System Status

The current implementation has successfully completed the core distributed enrollment functionality.

### Completed

- [x] C++ implementation
- [x] gRPC communication
- [x] Protocol Buffers
- [x] WebView node
- [x] AdminCore node
- [x] Student node
- [x] Faculty node
- [x] Configurable IP addresses
- [x] Configurable ports
- [x] JWT authentication
- [x] Session tracking
- [x] Logoutsession invalidation
- [x] Role-based access
- [x] Course viewing
- [x] Student enrollment
- [x] Duplicate enrollment prevention
- [x] Grade upload
- [x] Grade viewing
- [x] Student creation
- [x] Course creation
- [x] Thread synchronization
- [x] Deadlock issue fixed
- [x] Student service failure testing
- [x] Faculty service failure testing
- [x] Core service failure testing
- [x] Cross-PC communication
- [x] Config-based service addresses
- [x] Separate backend services

---

# 49. Remaining Tasks

The core implementation is finished.

The final two tasks should be treated as the remaining submissionpolishing work

1. Final documentation  presentation preparation
2. Final cleanup and submission verification

Before submission, verify

```text
CMakeLists.txt builds from a clean build directory
config.ini contains the correct deployment IPs
All required source files are present
No old server.exe is being used
admin_server.exe works
student_server.exe works
faculty_server.exe works
web_server.exe works
Client test works
All test cases pass
The README matches the final architecture
```

---

# 50. Final Architecture Summary

The completed system can be summarized as

```text
                    DISTRIBUTED ENROLLMENT SYSTEM


                         ┌──────────────┐
                         │   Browser    │
                         │     PC1      │
                         └──────┬───────┘
                                │
                              HTTP
                                │
                                ▼
                         ┌──────────────┐
                         │ Web  View   │
                         │     PC1      │
                         │    8080     │
                         └──────┬───────┘
                                │
             ┌──────────────────┼──────────────────┐
             │                  │                  │
           gRPC                gRPC               gRPC
             │                  │                  │
             ▼                  ▼                  ▼
      ┌─────────────┐   ┌─────────────┐   ┌─────────────┐
      │    Admin    │   │   Student   │   │   Faculty   │
      │   Service   │   │   Service   │   │   Service   │
      │    PC2      │   │    PC2      │   │    PC2      │
      │   50051    │   │   50052    │   │   50053    │
      └──────┬──────┘   └──────┬──────┘   └──────┬──────┘
             │                  │                  │
             │                  └────────┬─────────┘
             │                           │
             │                         gRPC
             │                           │
             └───────────────────────────┘
                         │
                         ▼
                  ┌──────────────┐
                  │  CoreService │
                  │ Shared State │
                  │    PC2       │
                  └──────────────┘
```

The architecture is ready to be expanded to separate physical Student and Faculty nodes simply by changing the service IP addresses in `config.ini`.