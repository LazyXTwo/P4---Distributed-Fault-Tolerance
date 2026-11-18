# Distributed Enrollment System — Current Status

## Important Current Status

The project is currently **between the working in-memory version and the SQLite version**.

The original in-memory backend was working successfully.

We then started migrating the backend to SQLite to provide persistent storage and better database-level concurrency handling.

However, the SQLite migration has **not been successfully completed yet** because the project is currently stuck on the CMake/static-linking and long compilation stage.

Therefore:

> **SQLite is installed, but the SQLite-based application is NOT currently confirmed working.**

Do not treat the SQLite functionality described below as completed functionality yet.

---

# 1. What Was Working Before the SQLite Migration

The original backend used in-memory C++ data structures.

That version successfully supported:

- Student login
- Admin login
- Logout
- Course listing
- Student enrollment
- Admin adding students
- Admin adding courses
- gRPC communication
- Web browser communication
- PC1 → PC2 communication
- Web server on PC1
- Backend server on PC2

The working architecture was:

```text
Browser
   ↓
PC1 Web Server
192.168.100.4:8080
   ↓
gRPC
   ↓
PC2 Backend
192.168.100.22:50051
   ↓
In-memory data
```

---

# 2. Current Architecture

The intended architecture is still:

```text
                 PC1
        ┌───────────────────┐
        │      Browser      │
        └─────────┬─────────┘
                  │
                HTTP
                  │
                  ▼
        ┌───────────────────┐
        │    Web Server     │
        │ 192.168.100.4     │
        │      :8080        │
        └─────────┬─────────┘
                  │
                gRPC
                  │
                  ▼
        ┌───────────────────┐
        │  Backend Server   │
        │ 192.168.100.22    │
        │      :50051       │
        └─────────┬─────────┘
                  │
                SQLite
                  │
                  ▼
        ┌───────────────────┐
        │   enrollment.db   │
        └───────────────────┘
                  PC2
```

The architecture itself is established.

The problem is currently the **build**, not the architecture.

---

# 3. PC Configuration

## PC1 — Web Node

IP:

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

---

## PC2 — Backend Node

IP:

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

SQLite is intended to run on this PC.

---

# 4. Project Files

Project:

```text
D:\School\STDISCIM\GRPC Test
```

Important files:

```text
CMakeLists.txt
enrollment.proto
server.cpp
client.cpp
web_server.cpp
```

Build directory:

```text
D:\School\STDISCIM\GRPC Test\build
```

---

# 5. gRPC

gRPC has already been installed and configured.

Source:

```text
D:\grpc\grpc
```

Installation:

```text
D:\grpc\install
```

The project successfully reached the point where gRPC communication between the two PCs worked.

The backend listens on:

```text
0.0.0.0:50051
```

The web server connects to:

```text
192.168.100.22:50051
```

---

# 6. SQLite Installation

SQLite itself **is installed**.

Confirmed:

```text
D:\MSYS2\ucrt64\include\sqlite3.h
```

and:

```text
D:\MSYS2\ucrt64\lib\libsqlite3.a
```

Installed package:

```text
mingw-w64-ucrt-x86_64-sqlite3
```

Version:

```text
3.53.4-1
```

The SQLite command-line executable is not installed, but that is not required for the C++ application.

---

# 7. IMPORTANT — SQLite Application Status

Although SQLite is installed, the application has **not successfully completed the SQLite migration**.

The following was implemented in `server.cpp`:

- SQLite database initialization
- `students` table
- `courses` table
- `enrollments` table
- Initial data seeding
- SQLite queries
- SQLite transactions
- WAL mode
- Busy timeout
- Atomic enrollment
- Duplicate-enrollment protection
- Course slot protection

However, this code has **not been successfully compiled and tested as the final working server**.

Therefore these should currently be considered:

```text
SQLite installation       ✓
SQLite code written       ✓
SQLite database design    ✓
SQLite build              ✗
SQLite runtime testing    ✗
SQLite persistence test   ✗
```

---

# 8. Current Major Problem — Build Time

The biggest problem currently is the build.

The generated gRPC source contains:

```text
enrollment.pb.cc
enrollment.grpc.pb.cc
```

The generated gRPC file is very large and requires many gRPC, protobuf, and Abseil headers/libraries.

During a clean build, the process reached approximately:

```text
76%
```

while compiling:

```text
enrollment.grpc.pb.cc
```

The compiler produced many warnings, including gRPC deprecation warnings.

The warnings themselves are not the main problem.

The problem is that the generated gRPC code takes a very long time to compile/link.

---

# 9. Why We Changed CMake

Originally, the generated gRPC files could be compiled separately for:

```text
server
client
web_server
```

This meant the same expensive generated source could be compiled multiple times.

We changed the CMake design to create:

```text
enrollment_proto
```

as a static library.

The intended structure is:

```text
enrollment.pb.cc
enrollment.grpc.pb.cc
           │
           ▼
   enrollment_proto
       /    |    \
      /     |     \
 server  client  web_server
```

This should allow the generated source to be compiled once and reused.

---

# 10. CMake Problem

The first attempt manually linked a few Abseil libraries.

That failed because protobuf requires many more Abseil libraries.

Errors included:

```text
undefined reference to
absl::lts_20260526::log_internal::...
```

A second attempt used the installed CMake targets:

```text
gRPC::grpc++
protobuf::libprotobuf
```

CMake then failed with:

```text
IMPORTED_LOCATION not set for imported target "gRPC::grpc++".
```

and:

```text
IMPORTED_LOCATION not set for imported target "protobuf::libprotobuf".
```

Therefore we returned to manual static linking.

The current approach attempts to collect the installed Abseil libraries automatically:

```cmake
file(
    GLOB
    ABSL_LIBRARIES
    "${GRPC_LIBRARY_DIR}/libabsl_*.a"
)
```

This approach has **not yet been successfully built**.

---

# 11. Current Build Status

At this exact checkpoint:

```text
CMake configuration       ← CURRENT PROBLEM
Static gRPC linking        ← CURRENT PROBLEM
Long compilation time      ← CURRENT PROBLEM

server.exe                 ← NOT CURRENTLY CONFIRMED
client.exe                 ← NOT CURRENTLY CONFIRMED
web_server.exe             ← NOT CURRENTLY CONFIRMED

SQLite server              ← NOT CONFIRMED
SQLite persistence         ← NOT CONFIRMED
SQLite enrollment          ← NOT CONFIRMED
```

---

# 12. What NOT to Assume

Do not currently assume:

```text
enrollment.db
```

is being successfully used by the running application.

Do not currently assume:

```text
SQLite enrollment transactions
```

are working.

Do not currently assume:

```text
SQLite persistence
```

has been tested.

The old **in-memory version** is the last version that was fully confirmed working.

---

# 13. Last Known Working System

The last fully working system was:

```text
PC1
192.168.100.4
    │
    ▼
web_server.exe
    │
    │ gRPC
    ▼
PC2
192.168.100.22
    │
    ▼
server.exe
    │
    ▼
In-memory C++ data
```

It successfully handled:

```text
Login
Logout
Get Courses
Enroll
Add Student
Add Course
```

The browser interface also worked.

---

# 14. How the Working Version Was Run

On PC2:

```bat
cd /d "D:\School\STDISCIM\GRPC Test\build"
server.exe
```

On PC1:

```bat
cd /d "D:\School\STDISCIM\GRPC Test\build"
web_server.exe
```

Then from a browser:

```text
http://192.168.100.4:8080
```

---

# 15. Current Goal

The immediate goal is **NOT to add new assignment features yet**.

The immediate goal is:

```text
Fix CMake
   ↓
Finish compilation
   ↓
Produce server.exe
client.exe
web_server.exe
   ↓
Run backend
   ↓
Run web server
   ↓
Test browser
   ↓
Test SQLite
```

Only after this is working should development continue.

---

# 16. Next Build Attempt

From:

```text
D:\School\STDISCIM\GRPC Test\build
```

run:

```bat
cmake .. -G "MinGW Makefiles"
```

If configuration succeeds:

```bat
cmake --build . --parallel 8
```

If the build fails, **do not immediately make another CMake change**.

Check the final error.

In particular, distinguish between:

```text
undefined reference
```

which indicates a linking/dependency problem,

and:

```text
Building CXX object ... enrollment.grpc.pb.cc.obj
```

which indicates the long compilation stage.

---

# 17. Once CMake Finally Works

After a successful build, test in this order.

### 1. PC2 backend

```bat
cd /d "D:\School\STDISCIM\GRPC Test\build"
server.exe
```

### 2. PC1 web server

```bat
cd /d "D:\School\STDISCIM\GRPC Test\build"
web_server.exe
```

### 3. Browser

```text
http://192.168.100.4:8080
```

### 4. Test student

```text
student1
password123
```

### 5. Test admin

```text
admin123
password123
```

### 6. Test SQLite

After the server successfully starts with the SQLite version:

- Add a student.
- Add a course.
- Enroll a student.
- Stop the server.
- Start the server again.
- Confirm the data remains.

Only after this test passes can we say the SQLite migration is complete.

---

# 18. Remaining Assignment Work

After the build and SQLite migration are confirmed:

## Required

- Faculty role
- Faculty grade upload
- Student previous grades
- OAuth/JWT if required by the assignment
- More complete authentication/security
- Concurrency testing
- Node failure testing

## Possible bonus

- Database redundancy
- Database connection pooling
- Additional node functionality

---

# 19. Recommended Order From Here

```text
CURRENT
   │
   ▼
Fix CMake
   │
   ▼
Finish build
   │
   ▼
Confirm server/client/web_server
   │
   ▼
Confirm browser still works
   │
   ▼
Confirm SQLite works
   │
   ▼
Confirm persistence
   │
   ▼
Add faculty role
   │
   ▼
Add grades
   │
   ▼
Add faculty grade upload
   │
   ▼
Add student grade viewing
   │
   ▼
Authentication improvements
   │
   ▼
OAuth/JWT
   │
   ▼
Concurrency testing
   │
   ▼
Node failure testing
   │
   ▼
UI cleanup
   │
   ▼
Bonus features
```

---

# 20. Final Current-State Summary

The project is **not currently in a finished working state**.

The important distinction is:

```text
                WORKING
                   │
                   ▼
        Original in-memory system
                   │
                   │
                   ▼
          SQLite migration started
                   │
                   ▼
       CMake/static linking problem
                   │
                   ▼
       Very long generated gRPC build
                   │
                   ▼
             CURRENT POINT
```

The distributed architecture, gRPC communication, web server, database design, and much of the backend logic have already been developed.

The current blocker is getting the **new SQLite-based build through CMake and compilation**.

Once that is solved, the first priority is to verify that the SQLite version actually runs before continuing with the remaining assignment features.