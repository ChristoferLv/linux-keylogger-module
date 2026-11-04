# Linux Kernel Keylogger Module (`keylogger3`)

A Loadable Kernel Module (LKM) for Linux written in C that demonstrates low-level hardware interrupt handling, kernel threading, and kernel-space virtual filesystem (VFS) operations by capturing and logging keyboard input.

---

## 📖 About / What It Is

`keylogger3` is an educational Linux Loadable Kernel Module (LKM). Operating directly within kernel space (Ring 0), it registers a shared Interrupt Service Routine (ISR) on the hardware keyboard interrupt line (`IRQ 1`) using the standard x86 `i8042` keyboard controller interface. 

By operating at the kernel level rather than user space, it intercepts keyboard keystrokes directly from hardware I/O ports before they reach higher-level display servers (such as X11 or Wayland) or user-space applications.

---

## 🎯 Purpose / Why It Was Created

This project was developed as an academic project for an **Operating Systems** course as part of a **Computer Science** degree program. Its purpose was to explore low-level operating system concepts, including developing Loadable Kernel Modules (LKMs), intercepting hardware interrupts, managing kernel worker threads, and interacting with kernel-level subsystems.

For further reading and reference on developing Linux kernel modules:
- 📖 **Local Copy:** [The Linux Kernel Module Programming Guide (PDF)](The%20Linux%20Kernel%20Module%20Programming%20Guide.pdf) — the version referenced and used during the project's development.
- 🌐 **Up-to-Date Version:** The latest, actively maintained version of the guide can be found on GitHub at [sysprog21/lkmpg](https://github.com/sysprog21/lkmpg/tree/gh-pages).

---

## ⚙️ Features / What It Does

- **Hardware Interrupt Interception (`IRQ 1`):** Hooks into IRQ 1 via `request_irq()` with `IRQF_SHARED`, listening to keyboard input alongside existing system drivers.
- **Direct I/O Port Access:** Reads raw scancodes from keyboard controller port `0x60` (`KBD_DATA_REG`) using `inb()`.
- **Scancode Translation:** Filters press vs. release events using status bitmasks (`KBD_STATUS_MASK`) and translates US keyboard scancodes to standard ASCII characters through a lookup array (`kbd_US`).
- **Control Key Mapping:** Represents selected non-printable control keys as uppercase indicator characters:
  - `Tab` $\rightarrow$ `T`
  - `Backspace` $\rightarrow$ `B`
  - `Shift` $\rightarrow$ `S`
  - `Enter` (`\n`) $\rightarrow$ `N`
  - `CapsLock` $\rightarrow$ `C`
- **Asynchronous Buffering & Kernel Worker Thread:** Keystrokes are temporarily stored in an in-memory buffer (`buf2`) inside the ISR. A dedicated kernel thread (`etx_thread`) wakes periodically to flush buffered keystrokes into the log file in chunks of at least 30 characters.
- **Kernel-Level File Output:** Writes the captured keystrokes directly to a specified output file using VFS write primitives (`filp_open()`, `vfs_write()`).
- **Graceful Cleanup:** Upon unloading (`rmmod`), the module deregisters the IRQ handler, stops the kernel thread, and flushes any remaining buffered keystrokes before unloading.

---

## ⚠️ Important: Kernel Version Compatibility

> [!WARNING]
> **Strict Kernel Requirement: Linux Kernel `< 5.10`**
>
> This module requires a Linux kernel version **strictly lower than 5.10** (e.g., 5.4 LTS or 4.x).

### Why this requirement exists:
The module performs direct file writes from kernel space using `vfs_write()`, which relies on `set_fs(KERNEL_DS)` and `get_fs()` to override the memory segment boundary between user space and kernel space. 

Starting in **Linux kernel version 5.10**, the Linux kernel developers deprecated and removed `set_fs()` and `KERNEL_DS` across architectures for security and memory-safety reasons. As a result, compiling this module on kernel version 5.10 or newer will fail with errors indicating that `set_fs` or `KERNEL_DS` are undefined.

* **Tested & Recommended Environment:** **Linux Mint 20.3 (MATE Edition)** or Ubuntu 20.04 LTS (running Linux Kernel `5.4.x`).

---

## 🚀 How to Build and Execute

### 1. Prerequisites
Ensure you are running an eligible Linux kernel (`uname -r < 5.10`) and install the necessary build tools and kernel headers:

```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)
```

### 2. Configuration
Before compiling, specify the desired absolute path for the output log file:
1. Open `keylogger3.c`.
2. Locate line 21:
   ```c
   #define FILE_PATH "/home/user_example/Documentos/kernel_file"
   ```
3. Change `/home/user_example/Documentos/kernel_file` to your actual user path (e.g., `/home/yourusername/kernel_file`). Ensure the target directory exists and has write permissions.

### 3. Compilation
Build the kernel module using `make`:

```bash
make
```

This invokes the kernel build system and generates the compiled kernel object file: `keylogger3.ko`.

### 4. Loading the Module
Insert the compiled module into the running kernel using `insmod`:

```bash
sudo insmod keylogger3.ko
```

Check the kernel message buffer with `dmesg` to verify successful insertion:

```bash
sudo dmesg | tail -n 10
```
*Expected log output:* `Modulo instalado com sucesso.`

### 5. Capturing & Viewing Logs
- Type on your keyboard. 
- After every 30 characters typed, the background kernel thread flushes the buffer to the file configured in `FILE_PATH`.
- You can inspect the written output in real-time or using `cat`:
  ```bash
  cat /path/to/your/kernel_file
  ```

### 6. Unloading the Module
Remove the module from the kernel using `rmmod`:

```bash
sudo rmmod keylogger3
```

Check the kernel ring buffer to confirm that the thread was stopped, remaining buffered keys were saved, and the module was unloaded:

```bash
sudo dmesg | tail -n 10
```
*Expected log output:*
```text
Thread parada
Modulo removido.
```

### 7. Cleaning Build Files
To remove intermediate compile files and the `.ko` binary:

```bash
make clean
```

---

## ⚖️ Disclaimer

This software was developed exclusively for **educational, academic research, and computer systems training purposes** as part of an undergraduate Operating Systems university curriculum. Using keystroke logging mechanisms on systems without explicit authorization is illegal and unethical.
