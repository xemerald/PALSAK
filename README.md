# Diagnosing Tool for P-alert@u7186EX

It just like a **S**wiss **A**rmy **K**nife for *P-alert* so we call it **PALSAK**.

***

## Requirement

### Software
* DOSBox
* Borland C++
* MiniOS7 Utility

### Hardware
* Obsoleted *P-alert* (u7186EX) **x1**
* Small button 7mm (NO) **x2**
* 18650 Li-ion battery **x3**
* 18650 battery box **x1**
* Power switch (NO) **x1**
* Ethernet cable w/ vertical connector 1.5m **x1**
* Power cable (2 core) 1.5m **x1**

## Build

1. First, download **DOSBox** & **Borland C++** from the internet (Recommend versions are **v0.74-3** & **v3.1** respectively).
2. Run the DOSBox with a new terminal.
3. Mount **C** to the disk where you install the **Borland C++**. For example, if you install that under disk D, you can execute this command: `mount c D:\`.
4. Change directory to the C:\ by execute this command: `C:\`.
5. And then execute the Borland C++ by this command: `BC\BIN\BC.EXE`.
6. F10->Project->Open project->Go to where you put the source of PALSAK->PALSAK.PRJ->Enter
7. F10->Window->Directories:
	* Include Directories: `C:\BC\INCLUDE`
	* Library Directories: `C:\BC\LIB`
8. F10->Options->Compiler->Code generation:
	* Model->Large
	* Options->Treat enum as ints
	* Options->Pre-compiled headers
	* Assume SS Equals DS->Default for memory model
9. F10->Options->Compiler->Advanced code generation:
	* Floating Point->None
	* Instruction Set->80186
	* Options->Generate underbars
	* Options->Fast floating point
	* Far Data Threshold: `32767`
10. F10->Options->Compiler->Optimizations:
	* Optimizations->Global register allocation
	* Optimizations->Suppress redundant loads
	* Optimizations->Dead code elimination
	* Optimizations->Jump optimization
	* Register Variables->Automatic
	* Common Subexpressions->No optimization
	* Optimize For->Size
11. F10->Options->Debugger
	* Source Debugging->None
	* Display Swapping->None
	* Inspectors->Show inherited
	* Inspectors->Show methods
	* Inspectors->Show decimal
	* Program Heap Size: `64`
12. Ins->add items:
	* 7186EL.LIB
	* TCP_DM32.LIB
	* PALSAK.C
	* BUTTONS.C
	* FILE.C
	* FTP.C
	* SPTIME.C
	* LEDINFO.C
13. Press F9 to build the program PALSAK.EXE.
14. Open another project, AGENT.PRJ, and do step 7. to 11. again.
15. Ins->add items:
	* 7186EL.LIB
	* TCP_DM32.LIB
	* AGENT.C
	* LEDINFO.C
16. Press F9 to build the program AGENT.EXE.
17. Finish the building process.

## Usage

1. First, download the **MiniOS7 Utility** from [here](http://ftp.icpdas.com/pub/cd/8000cd/napdos/minios7/utility/minios7_utility/MiniOS7_Utility_setup_v46_20200228.exe) and install it.
2. After install, open the program.
3. Connect the ethernet cable to the device, and press the init button & turn on the power. It will start to count seconds on the display.
4. Connect(F2)->connection->UDP:
	* IP: `192.168.137.123`
	* Port: `23`
5. Press OK to connect the device, and then upload files like following instructions:
```
Disk A:
0. AGENT.EXE
1. block_0.ini
2. ftp_info.ini
3. PALSAK.EXE
4. autoexec.bat
```
```
Disk B:
0. plt00XXX.exe <- The latest P-alert firmware.
```
6. After finish all the uploadings, just disconnect the device with computer.
7. Connect the ethernet cable w/ vertical connector back.
8. Connect the other side to a *P-alert* and turn on the power.
9. Init button (red) for switching, and CTS button (black) for comfirming.

## License

Under construction...
