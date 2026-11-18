# Distributed Enrollment System

A distributed web-based student enrollment system built using **C++**, **gRPC**, and a two-node network architecture.

The system separates the web/view layer from the backend services and runs them on two physically separate Windows computers connected through a local network.

---

# 1. System Architecture

The system consists of two physical PCs.

```text
                         ┌──────────────────────────────┐
                         │           PC1                │
                         │       192.168.100.4          │
                         │                              │
Browser ────────────────►│     Web / View Node          │
                         │       web_server.exe         │
                         │                              │
                         │          Port 8080           │
                         └──────────────┬───────────────┘
                                        │
                                        │ gRPC over LAN
                                        │
                                        ▼
                         ┌──────────────────────────────┐
                         │           PC2                │
                         │      192.168.100.22          │
                         │                              │
                         │       Backend Node           │
                         │         server.exe           │
                         │                              │
                         │          Port 50051          │
                         │                              │
                         │  Authentication              │
                         │  Sessions                    │
                         │  Courses                     │
                         │  Enrollment                  │
                         │  Grades                      │
                         └──────────────────────────────┘
```

## PC1 — Web/View Node

IP address:

```text
192.168.100.4
```

Runs:

```text
web_server.exe
```

Port:

```text
8080
```

Responsibilities:

- Serves the website
- Displays HTML pages
- Handles browser requests
- Maintains web sessions/cookies
- Sends requests to the backend using gRPC
- Does not directly access the backend's data

---

## PC2 — Backend Node

IP address:

```text
192.168.100.22
```

Runs:

```text
server.exe
```

Port:

```text
50051
```

Responsibilities:

- Authentication
- JWT creation and validation
- Session management
- Student management
- Course management
- Enrollment
- Grade management
- Business logic
- Concurrency protection

---

# 2. Technologies Used

- C++
- gRPC
- Protocol Buffers
- CMake
- MinGW / MSYS2
- Windows
- HTTP
- JWT
- HMAC-SHA256
- Winsock
- Visual Studio Code

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

## Important files

### `enrollment.proto`

Defines the gRPC service and messages.

RPC methods currently include:

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

### `server.cpp`

The backend server.

Contains:

- Authentication
- JWT implementation
- Session registry
- Course data
- Student data
- Faculty data
- Enrollment logic
- Grade logic
- Mutex synchronization
- gRPC service implementation

### `web_server.cpp`

The web/view node.

Contains:

- HTTP server
- Login page
- Admin pages
- Student pages
- Faculty pages
- Course display
- Enrollment interface
- Grade interface
- gRPC client communication

### `client.cpp`

A development/testing gRPC client.

It is useful for directly testing the backend without going through the browser.

It is **not required for the normal browser-based operation** of the system.

---

# 4. Requirements Implemented

## Authentication

Implemented:

- Login
- Logout
- JWT authentication
- JWT expiration
- Server-side session tracking
- Session invalidation on logout
- Role-based access

Roles:

```text
Admin
Student
Faculty
```

JWT uses:

```text
HS256 / HMAC-SHA256
```

JWT lifetime:

```text
3600 seconds
```

The server also maintains an active session registry using the JWT's JTI.

This allows a token to be invalidated immediately when the user logs out instead of waiting for the JWT to expire.

---

# 5. Student Features

Students can:

- Log in
- Log out
- View available courses
- See remaining course slots
- Enroll in courses
- Prevent duplicate enrollment
- View their grades

Example:

```text
CS101 - Introduction to Computing
CS102 - Data Structures
CS103 - Database Systems
```

When a student enrolls, the available course slots decrease.

The server also prevents the same student from enrolling in the same course twice.

---

# 6. Faculty Features

Faculty users can:

- Log in
- Log out
- Upload grades
- Update existing grades

Faculty must provide:

```text
Student Username
Course Code
Grade
```

Grades are restricted to:

```text
0 - 100
```

The server also verifies that the student is actually enrolled in the specified course before accepting the grade.

---

# 7. Admin Features

Administrators can:

- Log in
- Log out
- Add students
- Add courses

The system checks for:

- Duplicate usernames
- Duplicate course codes
- Invalid course slot values
- Invalid administrative operations

---

# 8. Current Accounts

## Administrator

```text
Username: admin123
Password: password123
```

## Faculty

```text
Username: faculty1
Password: password123
```

## Students

```text
Username: student1
Password: password123
```

```text
Username: student2
Password: password123
```

Additional students can be created through the Admin interface.

---

# 9. Initial Courses

The server starts with:

| Code | Course | Slots |
|---|---|---:|
| CS101 | Introduction to Computing | 30 |
| CS102 | Data Structures | 25 |
| CS103 | Database Systems | 20 |

Course slots decrease when students enroll.

---

# 10. Running the System

## Step 1 — Start the Backend

On **PC2**:

```cmd
cd /d "D:\School\STDISCIM\GRPC Test\build"
cmake --build . --parallel 4
server.exe
```

The server should display:

```text
Enrollment server running on 0.0.0.0:50051
JWT authentication enabled.
JWT lifetime: 3600 seconds.
Faculty account: faculty1 / password123
```

The backend listens on:

```text
0.0.0.0:50051
```

---

# 11. Verify the Backend Port

On PC2:

```cmd
netstat -ano | findstr :50051
```

Expected:

```text
TCP    0.0.0.0:50051    0.0.0.0:0    LISTENING
```

---

# 12. Test PC1 → PC2 Connectivity

On PC1:

```cmd
powershell -Command "Test-NetConnection 192.168.100.22 -Port 50051"
```

Expected:

```text
TcpTestSucceeded : True
```

ICMP ping does not need to work for this test. The important requirement is that TCP port `50051` is reachable.

---

# 13. Start the Web Server

On **PC1**:

```cmd
cd /d "D:\School\STDISCIM\GRPC Test\build"
cmake --build . --parallel 4
web_server.exe
```

Expected:

```text
Web server running on 0.0.0.0:8080
Backend gRPC server: 192.168.100.22:50051
```

---

# 14. Open the Website

From a browser on the network:

```text
http://192.168.100.4:8080
```

The browser connects to PC1.

PC1 then communicates with PC2 through gRPC.

---

# 15. Testing

The following tests have been completed successfully.

## Student Login

Login with:

```text
student1
password123
```

Expected:

```text
Login successful
```

The user is redirected to the course page.

---

## View Courses

The student should see:

```text
CS101 - Introduction to Computing
CS102 - Data Structures
CS103 - Database Systems
```

---

## Enrollment

Enroll:

```text
student1 → CS101
```

Expected:

```text
Enrollment successful
```

CS101 slots decrease from:

```text
30 → 29
```

---

## Duplicate Enrollment

Attempt to enroll `student1` in CS101 again.

Expected:

```text
You are already enrolled in this course.
```

---

## Faculty Grade Upload

Log in:

```text
faculty1
password123
```

Upload:

```text
Student: student1
Course: CS101
Grade: 92
```

Expected:

```text
Grade uploaded successfully.
```

---

## Student Grade Viewing

Log out of faculty.

Log in as:

```text
student1
password123
```

Open:

```text
My Grades
```

Expected:

```text
CS101
Introduction to Computing
92.00
```

---

## Invalid Grade Enrollment Test

Attempt to upload a grade for:

```text
student2
CS101
85
```

before student2 has enrolled in CS101.

Expected:

```text
Student is not enrolled in this course.
```

---

## Admin Test

Log in:

```text
admin123
password123
```

Create a new student.

Then log out and log in using the newly created student's credentials.

This verifies that the admin-created account is available to the backend authentication system.

---

# 16. Concurrency Protection

The backend uses mutex synchronization to protect shared data.

Protected operations include:

- Session management
- Course data
- Enrollment data
- Grade data
- Course slot updates

This is important because multiple clients can send requests to the gRPC server simultaneously.

For example, two students attempting to enroll in the last available slot should not both be allowed to consume the same slot.

The enrollment operation checks and updates the course data while holding the appropriate mutex.

---

# 17. Distributed Failure Test

The system also supports the required node-failure demonstration.

## Normal operation

```text
Browser
   ↓
PC1
   ↓
gRPC
   ↓
PC2
```

Both nodes are running.

---

## Stop PC2

Close:

```text
server.exe
```

on PC2.

PC1's web server continues running.

The website itself remains available, but operations requiring the backend cannot be completed.

For example:

```text
Login
Course retrieval
Enrollment
Grade operations
```

will fail because the backend node is unavailable.

---

## Restart PC2

Run:

```cmd
server.exe
```

again.

The backend starts listening on:

```text
192.168.100.22:50051
```

The web server on PC1 can communicate with it again.

This demonstrates that the application is composed of independently running network nodes rather than a single process.

---

# 18. Distributed System Characteristics

The system is distributed because:

1. The View/Web component runs on PC1.
2. The backend runs on PC2.
3. The computers have different IP addresses.
4. They communicate through the network.
5. gRPC is used for inter-node communication.
6. The nodes have different responsibilities.
7. The backend can fail independently from the web node.
8. The web node can remain running while the backend is unavailable.

Architecture:

```text
PC1
┌──────────────────────┐
│ Web / View           │
│ HTTP Server          │
│ Port 8080            │
└──────────┬───────────┘
           │
           │ gRPC
           │
           ▼
PC2
┌──────────────────────┐
│ Backend              │
│ Authentication       │
│ Courses              │
│ Enrollment           │
│ Grades               │
│ Sessions             │
│ Port 50051           │
└──────────────────────┘
```

The system is a **distributed client-server application with a centralized backend node**.

---

# 19. MVC Architecture

The application follows an MVC-oriented architecture.

### View

Located primarily on PC1.

The browser displays:

- Login
- Courses
- Enrollment
- Admin pages
- Faculty pages
- Grades

### Controller

The web server receives browser requests and determines which backend operation should be called.

For example:

```text
POST /enroll
        ↓
Web server
        ↓
gRPC Enroll()
        ↓
Backend
```

### Model / Business Logic

Located on PC2.

The backend manages:

- Students
- Faculty
- Courses
- Enrollments
- Grades
- Sessions
- Authentication

---

# 20. Security

The system uses JWT authentication.

JWT contains information including:

```text
sub
role
jti
iat
exp
```

The JWT is signed using HMAC-SHA256.

The server also maintains an active session registry.

Therefore, logging out removes the session from the server even though the JWT itself has not yet reached its expiration time.

---

# 21. Current Data Storage

The current implementation stores runtime data in memory.

This means:

```text
Server running
    ↓
Data exists in memory
    ↓
Server stops
    ↓
Runtime data is lost
```

The initial accounts and courses are recreated when the server starts.

Database persistence was investigated using SQLite but is not currently part of the implementation.

Database persistence and redundancy are considered optional/bonus functionality for the current assignment.

---

# 22. Optional Features Not Currently Implemented

The following are not required for the core implementation:

- Persistent database
- Database replication/redundancy
- Database connection pooling
- Multiple backend nodes
- Backend load balancing
- TLS-secured gRPC
- Faculty account administration
- Advanced course/faculty assignment

The core distributed enrollment requirements are already implemented.

---

# 23. Current Completion Status

## Required Features

- [x] Login
- [x] Logout
- [x] JWT authentication
- [x] Distributed session tracking
- [x] View courses
- [x] Student enrollment
- [x] Enrollment validation
- [x] Course slot management
- [x] View previous grades
- [x] Faculty grade upload
- [x] Admin student creation
- [x] Admin course creation
- [x] Web-based interface
- [x] Separate View node
- [x] Separate backend node
- [x] Network communication
- [x] gRPC communication
- [x] Concurrency protection
- [x] Node failure behavior
- [x] Node recovery

## Optional / Bonus

- [ ] Persistent database
- [ ] Database redundancy
- [ ] Connection pooling
- [ ] Multiple backend nodes
- [ ] TLS for gRPC
- [ ] Additional advanced features

---

# 24. Final Architecture Summary

The completed system separates the application into two networked physical nodes:

```text
                    CLIENT
                      │
                      │ HTTP
                      ▼
             ┌─────────────────┐
             │ PC1             │
             │ 192.168.100.4   │
             │                 │
             │ Web / View      │
             │ Port 8080       │
             └────────┬────────┘
                      │
                      │ gRPC
                      │
                      ▼
             ┌─────────────────┐
             │ PC2             │
             │ 192.168.100.22  │
             │                 │
             │ Backend         │
             │ Port 50051      │
             │                 │
             │ JWT             │
             │ Sessions        │
             │ Courses         │
             │ Enrollment      │
             │ Grades          │
             └─────────────────┘
```

The project demonstrates the fundamental characteristics of a distributed system through **physical node separation, network communication, remote procedure calls, independent node operation, authentication, concurrency control, and node failure handling**.