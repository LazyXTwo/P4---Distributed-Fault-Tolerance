# Distributed Enrollment System

A distributed online enrollment system built with **C++**, **gRPC**, and a simple **HTTP/MVC-style web interface** running across two physical PCs.

---

## 1. Project Overview

This project is a distributed enrollment system where the **web interface** and **backend/API** run on separate computers.

The browser connects to the Web/View node, which communicates with the Backend node using **gRPC**.

### Current Architecture

```text
                    USER BROWSER
                         |
                         | HTTP :8080
                         v
              +-----------------------+
              | PC1 - WEB / VIEW NODE |
              | IP: 192.168.100.4     |
              |                       |
              | web_server.exe         |
              +-----------------------+
                         |
                         | gRPC :50051
                         v
             +------------------------+
             | PC2 - BACKEND NODE     |
             | IP: 192.168.100.22     |
             |                        |
             | server.exe             |
             | EnrollmentService      |
             +------------------------+
```

### Responsibilities

**PC1 – Web/View Node**

- Serves the web pages
- Handles browser HTTP requests
- Maintains browser sessions
- Sends requests to the backend through gRPC
- Displays courses and enrollment results
- Provides admin pages

**PC2 – Backend Node**

- Handles authentication
- Maintains student accounts
- Maintains course information
- Handles enrollment
- Tracks backend sessions
- Enforces permissions
- Handles concurrent enrollment requests

---

# 2. Technologies

- C++
- gRPC
- Protocol Buffers
- CMake
- MinGW / MSYS2
- Winsock
- HTTP
- VS Code
- Windows
- Two physical PCs

### gRPC Installation

```text
D:\grpc\grpc
D:\grpc\grpc\cmake\build
D:\grpc\install
```

### Compiler

```text
D:\MSYS2\ucrt64\bin\c++.exe
```

---

# 3. Project Files

```text
GRPC Test/
│
├── CMakeLists.txt
├── enrollment.proto
├── server.cpp
├── client.cpp
├── web_server.cpp
│
└── build/
    ├── server.exe
    ├── client.exe
    ├── web_server.exe
    ├── enrollment.pb.cc
    ├── enrollment.pb.h
    ├── enrollment.grpc.pb.cc
    └── enrollment.grpc.pb.h
```

---

# 4. Network Configuration

## PC1

```text
IP: 192.168.100.4
Web Server: 0.0.0.0:8080
```

The browser accesses:

```text
http://192.168.100.4:8080
```

## PC2

```text
IP: 192.168.100.22
gRPC Server: 0.0.0.0:50051
```

PC1 connects to:

```text
192.168.100.22:50051
```

---

# 5. Current Features

## Login

The system currently supports:

- Admin login
- Student login
- Password checking
- Session creation
- Role detection

### Admin Account

```text
Username: admin123
Password: password123
```

### Student Accounts

```text
student1 / password123
student2 / password123
```

---

## Logout

Users can log out through the web interface.

Logout performs two actions:

1. Calls the backend `Logout` gRPC method.
2. Removes the browser-side web session.

---

## View Available Courses

Students can view the currently available courses.

Current courses:

```text
CS101 - Introduction to Computing - 30 slots
CS102 - Data Structures            - 25 slots
CS103 - Database Systems           - 20 slots
```

The course list is retrieved from the backend through gRPC.

---

## Student Enrollment

Students can enroll in available courses.

The backend checks:

- Whether the session is valid
- Whether the user is a student
- Whether the course exists
- Whether the student is already enrolled
- Whether available slots remain

When enrollment succeeds:

```text
Student enrollment is added
Course slot count decreases by 1
```

---

# 6. Concurrent Enrollment Handling

The backend uses a mutex to protect shared enrollment data.

The important operations are performed while holding the same mutex:

```text
Check course
Check remaining slots
Check duplicate enrollment
Add enrollment
Decrease available slots
```

This prevents a race condition where two students attempt to take the final available slot simultaneously.

For example:

```text
Course has 1 slot remaining

Student A ---> Enroll
Student B ---> Enroll
```

The mutex ensures that only one request can successfully claim the final slot.

This is important because the application is intended to support multiple users concurrently.

---

# 7. Admin Features

The admin page currently supports:

### Add Student

Admin can create a new student account.

Validation includes:

- Username cannot be empty
- Password cannot be empty
- Username must not already exist
- `admin123` cannot be created as a normal student account

### Add Course

Admin can create a new course.

Validation includes:

- Course code cannot be empty
- Course name cannot be empty
- Slots cannot be negative
- Course code must be unique

---

# 8. gRPC Service

The system uses the following gRPC service:

```proto
service EnrollmentService {
    rpc Login (LoginRequest) returns (LoginReply);
    rpc Logout (LogoutRequest) returns (LogoutReply);
    rpc GetCourses (GetCoursesRequest) returns (GetCoursesReply);
    rpc Enroll (EnrollRequest) returns (EnrollReply);
    rpc AddStudent (AddStudentRequest) returns (AddStudentReply);
    rpc AddCourse (AddCourseRequest) returns (AddCourseReply);
}
```

The `.proto` file is used to generate:

```text
enrollment.pb.cc
enrollment.pb.h
enrollment.grpc.pb.cc
enrollment.grpc.pb.h
```

---

# 9. Session System

There are currently two session layers.

## Backend Session

The backend stores:

```text
session token -> username
```

Example:

```text
session_1 -> student1
session_2 -> student2
```

The backend uses a mutex to protect the session map.

## Web Session

The web server stores:

```text
browser session -> gRPC token + role
```

Example:

```text
web_1 -> session_1 + student
```

The browser receives a cookie:

```text
session=web_1
```

The web server uses this cookie to identify the logged-in browser.

---

# 10. Current Authentication Limitation

The assignment requires:

> Login/logout. Track sessions across different nodes. (Use OAuth / JWT)

The current system **does track sessions across the two nodes**, but it does **not yet use JWT or OAuth**.

Currently, the backend generates simple tokens:

```text
session_1
session_2
session_3
```

Therefore requirement #1 is currently only partially complete.

### Required Improvement

Replace the current simple session token with a properly signed **JWT**.

The intended flow is:

```text
Browser
   |
   | Login
   v
PC1 Web Server
   |
   | gRPC Login
   v
PC2 Backend
   |
   | Generate JWT
   v
PC1
   |
   | JWT cookie
   v
Browser
```

For subsequent requests:

```text
Browser
   |
   | JWT
   v
PC1
   |
   | gRPC + JWT
   v
PC2
   |
   | Validate JWT
   v
Request accepted/rejected
```

JWT should contain information such as:

```text
username
role
session ID / jti
expiration time
```

The backend should also maintain a server-side revocation/session registry so that logout can invalidate the JWT before its expiration.

---

# 11. Database Status

SQLite was previously investigated but is **not currently being used**.

SQLite is installed in the MSYS2 environment:

```text
/ucrt64/include/sqlite3.h
/ucrt64/lib/libsqlite3.a
/ucrt64/lib/libsqlite3.dll.a
```

However, the SQLite implementation was abandoned because:

- Build times became excessive
- CMake/linking became complicated
- Runtime persistence was not successfully verified
- The current in-memory implementation is working

The current backend therefore uses:

```text
C++ vectors
C++ unordered_map
```

for its data.

### Current Limitation

All data is lost when:

```text
server.exe
```

is stopped.

---

# 12. Assignment Requirement Status

| Requirement | Status |
|---|---|
| 1. Login/logout + distributed sessions + OAuth/JWT | 🟡 Partially Complete |
| 2. View available courses | 🟢 Complete |
| 3. Students enroll in open courses | 🟢 Complete |
| 4. Students view previous grades | 🔴 Not Implemented |
| 5. Faculty upload grades | 🔴 Not Implemented |
| 6. Web-based using MVC | 🟡 Partially Complete |
| 7. View is a separate node | 🟢 Complete |
| 8. Networked physical PCs/VMs | 🟢 Complete |
| 9. Node failure affects only its features | 🟢 Mostly Complete |
| 10. gRPC inter-node communication | 🟢 Complete |
| Bonus: Persistent/redundant DB | 🔴 Not Implemented |

---

# 13. MVC / Distributed Design

The project separates the View/Web node from the Backend node.

### View

Located on:

```text
PC1
```

Handled by:

```text
web_server.cpp
```

### Controller / API

Located on:

```text
PC2
```

Handled by:

```text
server.cpp
```

### Communication

Handled using:

```text
gRPC
```

This means the browser does not directly communicate with the backend.

Instead:

```text
Browser
   ↓
Web/View
   ↓
gRPC
   ↓
Backend
```

---

# 14. Node Failure Behavior

The architecture allows different parts of the application to fail independently.

## If PC2 / Backend goes down

PC1's web server can still technically be running, but backend-dependent features will fail because gRPC requests cannot reach:

```text
192.168.100.22:50051
```

Affected functionality includes:

- Login
- Course retrieval
- Enrollment
- Admin operations
- Logout

## If PC1 / Web Node goes down

The browser cannot access the web interface.

The backend itself can still be running and listening for gRPC requests.

---

# 15. Building the Project

From:

```text
D:\School\STDISCIM\GRPC Test
```

create/use the build directory:

```cmd
cd /d "D:\School\STDISCIM\GRPC Test\build"
```

Configure:

```cmd
cmake .. -G "MinGW Makefiles"
```

Build:

```cmd
cmake --build . --parallel 4
```

Successful builds generate:

```text
server.exe
client.exe
web_server.exe
```

---

# 16. Running the System

## PC2 — Backend

On PC2:

```cmd
cd /d "D:\School\STDISCIM\GRPC Test\build"
server.exe
```

The server should listen on:

```text
0.0.0.0:50051
```

Expected output:

```text
Server listening on 0.0.0.0:50051
```

Keep this window open.

---

## PC1 — Web Server

On PC1:

```cmd
cd /d "D:\School\STDISCIM\GRPC Test\build"
web_server.exe
```

The web server should listen on:

```text
0.0.0.0:8080
```

The browser can then access:

```text
http://192.168.100.4:8080
```

---

# 17. Client.exe

`client.exe` is currently a development/testing client for the gRPC backend.

It is **not required for normal browser operation**.

The final application flow is:

```text
Browser
    ↓
web_server.exe
    ↓
server.exe
```

`client.exe` is useful for testing the gRPC service directly.

---

# 18. Windows Firewall

The following ports may need to be allowed through Windows Firewall.

## PC1

```text
TCP 8080
```

Required so other computers can access the web server.

## PC2

```text
TCP 50051
```

Required so PC1 can connect to the gRPC backend.

---

# 19. Current CMake Linking

The project uses static gRPC libraries.

The CMake configuration currently links a limited set of required libraries rather than every `.a` file in the gRPC installation.

Important libraries include:

```text
libabsl_*.a
libupb*.a
libutf8*.a
libre2.a
libcares.a
libzlibstatic.a
libprotobuf.a
libgrpc_unsecure.a
libgrpc++_unsecure.a
libgpr.a
libaddress_sorting.a
```

Windows libraries include:

```text
ws2_32
advapi32
iphlpapi
bcrypt
crypt32
dbghelp
```

`dbghelp` was required to resolve linker errors involving:

```text
SymSetOptions
SymInitialize
SymFromAddr
```

---

# 20. Previous Build Issue

The project previously had very long build/link times.

The main cause was excessive static library linking.

The CMake configuration was initially globbing a very large number of `.a` libraries from the gRPC installation.

This caused the linker to process many unnecessary libraries.

The configuration was changed to only include the required library groups.

The project now builds successfully.

---

# 21. Current Executable Sizes

The current statically linked executables are large:

```text
server.exe      ~119 MB
client.exe      ~116 MB
web_server.exe  ~116 MB
```

This is primarily because gRPC, Protocol Buffers, Abseil, and related dependencies are statically linked.

This is not currently preventing the application from working.

---

# 22. Current Data Model

### Student

```text
username
password
```

### Course

```text
course_code
course_name
slots
```

### Session

```text
session_token
username
```

### Enrollment

```text
username
course_code
```

At the moment, enrollment information is stored in memory.

---

# 23. Remaining Work

The next major features that need to be implemented are:

## 1. JWT Authentication

Replace:

```text
session_1
session_2
```

with properly signed JWT tokens.

This will complete the OAuth/JWT portion of requirement #1.

---

## 2. Previous Grades

Students need a way to view their previous grades.

This will require:

- Grade data
- Student/grade relationship
- gRPC request/reply
- Backend implementation
- Web page
- Student navigation

Possible flow:

```text
Student
   ↓
View Grades
   ↓
PC1 Web Server
   ↓
gRPC
   ↓
PC2 Backend
   ↓
Student's Grades
```

---

## 3. Faculty Accounts / Faculty Role

The current system has:

```text
admin
student
```

A faculty role should be added.

Example:

```text
faculty1
```

Faculty should be able to upload grades.

---

## 4. Faculty Grade Upload

Add a gRPC operation similar to:

```text
UploadGrade
```

The backend should validate:

- Session
- Faculty role
- Student
- Course
- Grade

Then store the grade.

---

## 5. Student Grade Viewing

Add a gRPC operation such as:

```text
GetGrades
```

Students should only be able to retrieve **their own grades**.

---

# 24. Recommended Development Order

The remaining implementation should be done in this order:

```text
1. JWT authentication
       ↓
2. Faculty role
       ↓
3. Grade data structure
       ↓
4. UploadGrade gRPC method
       ↓
5. GetGrades gRPC method
       ↓
6. Faculty web page
       ↓
7. Student grades web page
       ↓
8. Test concurrent users
       ↓
9. Test node failure
       ↓
10. Final documentation/demo
```

---

# 25. Current Overall State

The distributed foundation is working.

The following has already been demonstrated:

```text
PC1
Web Server
   ↓
HTTP
   ↓
PC2
gRPC Backend
```

The system currently supports:

```text
✓ Distributed web server
✓ Separate backend node
✓ gRPC communication
✓ Login
✓ Logout
✓ Session tracking
✓ Student accounts
✓ Admin accounts
✓ Course listing
✓ Course creation
✓ Student creation
✓ Student enrollment
✓ Concurrent enrollment protection
✓ Separate physical PCs
✓ Cross-PC browser access
```

The major missing assignment functionality is:

```text
✗ JWT/OAuth
✗ Faculty role
✗ Faculty grade upload
✗ Student previous grades
✗ Persistent database
```

The **core distributed architecture is already operational**, so the remaining work is primarily implementing the missing enrollment-system features rather than rebuilding the networking/gRPC foundation.