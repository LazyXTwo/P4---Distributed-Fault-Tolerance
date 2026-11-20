# Quick Network Setup

## Minimum Files Required

You only need these files to run the system:

### Web PC

```text
web_server.exe
config.ini
```

### Admin/Core PC

```text
admin_server.exe
config.ini
```

### Student PC

```text
student_server.exe
config.ini
```

### Faculty PC

```text
faculty_server.exe
config.ini
```

### Required DLLs

If the DLLs are not already installed on the PC, also copy:

```text
libgcc_s_seh-1.dll
libstdc++-6.dll
libwinpthread-1.dll
```

Put the DLLs in the **same folder as the `.exe`**.

---

# 1. Connect All PCs

Connect all computers to the **same Wi-Fi or LAN network**.

---

# 2. Find Each PC's IP Address

On every PC, open CMD:

```cmd
ipconfig
```

Find:

```text
IPv4 Address
```

Write them down.

Example:

```text
Web PC      → 192.168.1.10
Admin PC    → 192.168.1.20
Student PC  → 192.168.1.30
Faculty PC  → 192.168.1.40
```

---

# 3. Test the Network

From each PC, test the other PCs:

```cmd
ping <OTHER_PC_IP>
```

Example:

```cmd
ping 192.168.1.20
```

Make sure the computers can communicate.

---

# 4. Update `config.ini`

Use the new IP addresses:

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

Replace the example IPs with your actual IPs.

---

# 5. Copy `config.ini`

Put the updated `config.ini` beside every executable:

```text
Web PC:
web_server.exe
config.ini

Admin PC:
admin_server.exe
config.ini

Student PC:
student_server.exe
config.ini

Faculty PC:
faculty_server.exe
config.ini
```

---

# 6. Allow the TCP Ports Through Windows Firewall

Open **PowerShell as Administrator**.

## Web PC

Run:

```powershell
New-NetFirewallRule -DisplayName "Enrollment Web Server 8080" -Direction Inbound -Protocol TCP -LocalPort 8080 -Action Allow
```

## Admin/Core PC

Run:

```powershell
New-NetFirewallRule -DisplayName "Enrollment Admin Server 50051" -Direction Inbound -Protocol TCP -LocalPort 50051 -Action Allow
```

## Student PC

Run:

```powershell
New-NetFirewallRule -DisplayName "Enrollment Student Server 50052" -Direction Inbound -Protocol TCP -LocalPort 50052 -Action Allow
```

## Faculty PC

Run:

```powershell
New-NetFirewallRule -DisplayName "Enrollment Faculty Server 50053" -Direction Inbound -Protocol TCP -LocalPort 50053 -Action Allow
```

---

# 7. CMD Alternative

If you prefer **CMD as Administrator**, use:

### Web PC

```cmd
netsh advfirewall firewall add rule name="Enrollment Web Server 8080" dir=in action=allow protocol=TCP localport=8080
```

### Admin/Core PC

```cmd
netsh advfirewall firewall add rule name="Enrollment Admin Server 50051" dir=in action=allow protocol=TCP localport=50051
```

### Student PC

```cmd
netsh advfirewall firewall add rule name="Enrollment Student Server 50052" dir=in action=allow protocol=TCP localport=50052
```

### Faculty PC

```cmd
netsh advfirewall firewall add rule name="Enrollment Faculty Server 50053" dir=in action=allow protocol=TCP localport=50053
```

---

# 8. Test the Ports

From the Web PC, open PowerShell:

```powershell
Test-NetConnection <ADMIN_PC_IP> -Port 50051
```

```powershell
Test-NetConnection <STUDENT_PC_IP> -Port 50052
```

```powershell
Test-NetConnection <FACULTY_PC_IP> -Port 50053
```

They should show:

```text
TcpTestSucceeded : True
```

---

# 9. Start the Servers

Start them in this order:

```text
1. admin_server.exe
2. student_server.exe
3. faculty_server.exe
4. web_server.exe
```

---

# 10. Open the Website

On a browser:

```text
http://<WEB_PC_IP>:8080
```

Example:

```text
http://192.168.1.10:8080
```

---

# 11. Test

Test:

```text
Login
Courses
Enrollment
Faculty grade upload
Student grades
Admin
Logout
```

Done.
