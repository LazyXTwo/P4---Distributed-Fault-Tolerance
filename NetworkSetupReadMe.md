# Distributed Enrollment System
## New Network IP Address Setup Guide

This guide explains how to move the Distributed Enrollment System to a **different network** with different IP addresses.

The IP addresses used during development are **not required**.

The system is designed so that the IP addresses can be changed through `config.ini`.

---

# 1. Network Architecture

The system uses four logical nodes:

```text
PC1
Web / View Server
        │
        │
        ├──────────────► Admin/Core Server
        │
        ├──────────────► Student Server
        │
        └──────────────► Faculty Server
```

The services are:

| Node | Purpose | Default Port |
|---|---|---:|
| Web Server | Browser/View | 8080 |
| Admin/Core | Shared backend | 50051 |
| Student | Student features | 50052 |
| Faculty | Faculty features | 50053 |

The ports do **not** have to change when moving to another network.

Only the IP addresses normally need to change.

---

# 2. Example New Network

For this guide, assume the new network gives the computers these addresses:

```text
PC1 Web:
192.168.1.10

PC2 Admin/Core:
192.168.1.20

PC3 Student:
192.168.1.30

PC4 Faculty:
192.168.1.40
```

These addresses are only examples.

Your actual network may use something completely different, such as:

```text
10.0.0.x
```

or:

```text
172.16.0.x
```

Use the IP addresses actually assigned by your new network.

---

# 3. Determine Each Computer's IP Address

On **each computer**, open Command Prompt:

```cmd
ipconfig
```

Look for:

```text
IPv4 Address
```

Example:

```text
Wireless LAN adapter Wi-Fi:

   IPv4 Address. . . . . . . . . . . : 192.168.1.20
```

Record the IPv4 address of every computer.

Create a table like this:

```text
Web PC:
____________________

Admin/Core PC:
____________________

Student PC:
____________________

Faculty PC:
____________________
```

Do not use the IPv4 address from a virtual adapter such as:

```text
VirtualBox
VMware
Hyper-V
WSL
```

unless that is intentionally the network being used by the project.

Use the address that allows the computers to communicate with each other.

---

# 4. Decide Which PC Runs Each Service

The recommended deployment is:

```text
PC1
Web Server
Port 8080
```

```text
PC2
Admin/Core Server
Port 50051
```

```text
PC3
Student Server
Port 50052
```

```text
PC4
Faculty Server
Port 50053
```

If fewer computers are available, multiple services can run on the same computer.

For example:

```text
PC1
Web

PC2
Admin/Core
Student
Faculty
```

The architecture still works.

---

# 5. Configure `config.ini`

The most important file for network setup is:

```text
config.ini
```

The file contains:

```ini
[NETWORK]

WEB_SERVER_IP=...
WEB_SERVER_PORT=8080

ADMIN_SERVER_IP=...
ADMIN_SERVER_PORT=50051

STUDENT_SERVER_IP=...
STUDENT_SERVER_PORT=50052

FACULTY_SERVER_IP=...
FACULTY_SERVER_PORT=50053
```

---

# 6. Example: Four Separate PCs

Suppose the new network is:

```text
Web PC:
192.168.1.10

Admin/Core PC:
192.168.1.20

Student PC:
192.168.1.30

Faculty PC:
192.168.1.40
```

The `config.ini` used by the system should be:

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

---

# 7. Important: The Same Configuration File

The easiest deployment method is to give each executable a copy of the same `config.ini`.

For example:

```text
PC1
├── web_server.exe
└── config.ini

PC2
├── admin_server.exe
└── config.ini

PC3
├── student_server.exe
└── config.ini

PC4
├── faculty_server.exe
└── config.ini
```

The same IP addresses can therefore be used by all services.

---

# 8. What Each Computer Does With the Configuration

## PC1 — Web Server

The Web Server needs to know where all backend services are:

```text
Admin/Core IP
Student IP
Faculty IP
```

It also uses:

```text
WEB_SERVER_PORT=8080
```

---

## PC2 — Admin/Core Server

The Admin/Core server needs to know its own listening port.

It uses:

```text
ADMIN_SERVER_PORT=50051
```

---

## PC3 — Student Server

The Student server needs to know where the Admin/Core server is located.

For example:

```text
ADMIN_SERVER_IP=192.168.1.20
ADMIN_SERVER_PORT=50051
```

It also listens on:

```text
STUDENT_SERVER_PORT=50052
```

---

## PC4 — Faculty Server

The Faculty server also needs to know where the Admin/Core server is located.

For example:

```text
ADMIN_SERVER_IP=192.168.1.20
ADMIN_SERVER_PORT=50051
```

It listens on:

```text
FACULTY_SERVER_PORT=50053
```

---

# 9. Do Not Change the Service Ports Unless Necessary

Normally, keep:

```text
Admin/Core = 50051
Student    = 50052
Faculty    = 50053
Web        = 8080
```

The network IP addresses are what normally change.

For example:

```text
OLD:
192.168.100.22:50051

NEW:
192.168.1.20:50051
```

The port stays:

```text
50051
```

---

# 10. Windows Firewall

Windows Firewall can prevent other computers from connecting even when the IP address is correct.

The following ports need to be allowed for incoming connections:

```text
50051
50052
50053
8080
```

The backend ports:

```text
50051
50052
50053
```

are required on the appropriate backend computers.

The web port:

```text
8080
```

is required on the Web Server computer.

---

# 11. Allowing the Ports Through Windows Firewall

On the appropriate computer, open **Command Prompt as Administrator**.

For the Admin/Core server:

```cmd
netsh advfirewall firewall add rule name="Enrollment Admin Core 50051" dir=in action=allow protocol=TCP localport=50051
```

For the Student server:

```cmd
netsh advfirewall firewall add rule name="Enrollment Student 50052" dir=in action=allow protocol=TCP localport=50052
```

For the Faculty server:

```cmd
netsh advfirewall firewall add rule name="Enrollment Faculty 50053" dir=in action=allow protocol=TCP localport=50053
```

For the Web Server:

```cmd
netsh advfirewall firewall add rule name="Enrollment Web 8080" dir=in action=allow protocol=TCP localport=8080
```

Only run the rule for the port actually used by that computer.

---

# 12. Test Basic Network Connectivity

Before running the application, test whether the computers can communicate.

From PC1, test the Admin/Core PC:

```powershell
Test-NetConnection <ADMIN_IP> -Port 50051
```

Example:

```powershell
Test-NetConnection 192.168.1.20 -Port 50051
```

You want:

```text
TcpTestSucceeded : True
```

---

# 13. Test Student Server

From the Web PC:

```powershell
Test-NetConnection <STUDENT_IP> -Port 50052
```

Example:

```powershell
Test-NetConnection 192.168.1.30 -Port 50052
```

Expected:

```text
TcpTestSucceeded : True
```

---

# 14. Test Faculty Server

From the Web PC:

```powershell
Test-NetConnection <FACULTY_IP> -Port 50053
```

Example:

```powershell
Test-NetConnection 192.168.1.40 -Port 50053
```

Expected:

```text
TcpTestSucceeded : True
```

---

# 15. Test Admin/Core From Student PC

This is important because the Student server communicates directly with CoreService.

On the Student PC:

```powershell
Test-NetConnection <ADMIN_IP> -Port 50051
```

Example:

```powershell
Test-NetConnection 192.168.1.20 -Port 50051
```

Expected:

```text
TcpTestSucceeded : True
```

---

# 16. Test Admin/Core From Faculty PC

On the Faculty PC:

```powershell
Test-NetConnection <ADMIN_IP> -Port 50051
```

Example:

```powershell
Test-NetConnection 192.168.1.20 -Port 50051
```

Expected:

```text
TcpTestSucceeded : True
```

This confirms that both proxy servers can reach the Core server.

---

# 17. Verify the Server Is Listening

On the Admin/Core PC:

```cmd
netstat -ano | findstr :50051
```

You should see a listening entry.

On the Student PC:

```cmd
netstat -ano | findstr :50052
```

On the Faculty PC:

```cmd
netstat -ano | findstr :50053
```

On the Web PC:

```cmd
netstat -ano | findstr :8080
```

---

# 18. Start Order

For a new deployment, start the services in this order.

## Step 1 — Admin/Core

On the Admin/Core PC:

```cmd
admin_server.exe
```

Wait until it reports that the server is listening.

---

## Step 2 — Student

On the Student PC:

```cmd
student_server.exe
```

The Student server should connect to the Admin/Core server.

---

## Step 3 — Faculty

On the Faculty PC:

```cmd
faculty_server.exe
```

The Faculty server should connect to the Admin/Core server.

---

## Step 4 — Web

On the Web PC:

```cmd
web_server.exe
```

The browser can then connect to the Web server.

---

# 19. Opening the Website

If the Web PC's address is:

```text
192.168.1.10
```

and the web port is:

```text
8080
```

open:

```text
http://192.168.1.10:8080
```

from a browser on another computer.

The Web Server is the only component the normal user needs to access through the browser.

---

# 20. If the Web Server Cannot Connect

Check:

```text
1. Is Admin/Core running?
2. Is Student server running?
3. Is Faculty server running?
4. Are the IP addresses correct?
5. Are the ports correct?
6. Is Windows Firewall allowing the ports?
7. Can Test-NetConnection reach each port?
8. Is config.ini beside the executable?
```

---

# 21. If Student Server Cannot Connect to Core

Check the Student PC's:

```ini
ADMIN_SERVER_IP=...
ADMIN_SERVER_PORT=50051
```

Make sure the IP is the **Admin/Core computer's IP**.

For example:

```ini
ADMIN_SERVER_IP=192.168.1.20
ADMIN_SERVER_PORT=50051
```

Then test:

```powershell
Test-NetConnection 192.168.1.20 -Port 50051
```

---

# 22. If Faculty Server Cannot Connect to Core

Check:

```ini
ADMIN_SERVER_IP=...
ADMIN_SERVER_PORT=50051
```

Then test:

```powershell
Test-NetConnection <ADMIN_IP> -Port 50051
```

---

# 23. If the Browser Cannot Connect

First check the Web Server PC itself:

```cmd
netstat -ano | findstr :8080
```

Then from another computer:

```powershell
Test-NetConnection <WEB_IP> -Port 8080
```

Example:

```powershell
Test-NetConnection 192.168.1.10 -Port 8080
```

Expected:

```text
TcpTestSucceeded : True
```

Then open:

```text
http://192.168.1.10:8080
```

---

# 24. Changing Networks Again

If the entire system is moved to another network later, repeat the same process.

For example, suppose the new network gives:

```text
Web:
10.0.0.10

Admin:
10.0.0.20

Student:
10.0.0.30

Faculty:
10.0.0.40
```

Change `config.ini` to:

```ini
[NETWORK]

WEB_SERVER_IP=10.0.0.10
WEB_SERVER_PORT=8080

ADMIN_SERVER_IP=10.0.0.20
ADMIN_SERVER_PORT=50051

STUDENT_SERVER_IP=10.0.0.30
STUDENT_SERVER_PORT=50052

FACULTY_SERVER_IP=10.0.0.40
FACULTY_SERVER_PORT=50053
```

No C++ source-code changes are required.

---

# 25. Important Rule

Do **not** use the IP address of the computer you are currently sitting at for every setting.

Each setting must point to the correct computer.

For example:

```ini
WEB_SERVER_IP
```

must be the Web Server computer.

```ini
ADMIN_SERVER_IP
```

must be the Admin/Core computer.

```ini
STUDENT_SERVER_IP
```

must be the Student computer.

```ini
FACULTY_SERVER_IP
```

must be the Faculty computer.

---

# 26. Example Final Configuration

For four PCs:

```text
┌───────────────────────────────┐
│ PC1 — Web                     │
│ 192.168.1.10                  │
│ TCP 8080                      │
└───────────────────────────────┘

┌───────────────────────────────┐
│ PC2 — Admin/Core              │
│ 192.168.1.20                  │
│ TCP 50051                     │
└───────────────────────────────┘

┌───────────────────────────────┐
│ PC3 — Student                 │
│ 192.168.1.30                  │
│ TCP 50052                     │
└───────────────────────────────┘

┌───────────────────────────────┐
│ PC4 — Faculty                 │
│ 192.168.1.40                  │
│ TCP 50053                     │
└───────────────────────────────┘
```

Configuration:

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

---

# 27. Final Deployment Checklist

Before running the system on a new network:

```text
[ ] Find the IPv4 address of every PC
[ ] Decide which PC runs each service
[ ] Update config.ini
[ ] Copy the updated config.ini to every PC
[ ] Allow required Windows Firewall ports
[ ] Start Admin/Core
[ ] Start Student
[ ] Start Faculty
[ ] Start Web Server
[ ] Test port 50051
[ ] Test port 50052
[ ] Test port 50053
[ ] Test port 8080
[ ] Open the Web Server in a browser
[ ] Test student login
[ ] Test course viewing
[ ] Test enrollment
[ ] Test faculty login
[ ] Test grade upload
[ ] Test student grade viewing
[ ] Test admin functions
```

---

# 28. Quick Setup Summary

For a completely new network:

```text
1. Run ipconfig on every PC.

2. Record each PC's IPv4 address.

3. Assign:
   Web     → port 8080
   Admin   → port 50051
   Student → port 50052
   Faculty → port 50053

4. Put those IP addresses into config.ini.

5. Copy config.ini to every machine.

6. Configure Windows Firewall.

7. Test connectivity with Test-NetConnection.

8. Start:
   admin_server.exe
   student_server.exe
   faculty_server.exe
   web_server.exe

9. Open:
   http://<WEB_IP>:8080

10. Test the application.
```

---

# 29. Key Point

The project is **not tied to the original development network**.

The addresses used during development are simply configuration values.

When moving the project to another network, the main requirement is to update:

```text
WEB_SERVER_IP
ADMIN_SERVER_IP
STUDENT_SERVER_IP
FACULTY_SERVER_IP
```

in:

```text
config.ini
```

The service ports can remain:

```text
8080
50051
50052
50053
```

as long as those ports are available and allowed through the firewall.