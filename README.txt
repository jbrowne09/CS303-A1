~GROUP #2 READ ME~
Authors: Jonathan Browne, Tom Corne

This README details operational instructions for the traffic light controller running on a Altera DE2-115 Development Board.

Note: - This project was completed in 2019, software used may be outdated/incompatible with current Operating system(s)
      - The original assignment brief is included: 'trafficControllerBrief.pdf'
      - Project was coded in C; only the code inside the 'main.c' file in the folder 'Group2_A1' was completed by the group 
        all other files are auto-generated or provided as apart of the assignment 

PROJECT CREATION/DEVICE PROGRAMMING:
To Create the project:
1: Open Nios II 16.1 Software Build tools for eclipse
2: File->Switch Workspace->Other, Browse to the 'group2_Workspace' folder
2: File->Import->General->Existing Projects into workspace
3: Select 'group2_Workspace' as root directory
4: Ensure Group2_A1 and Group2_A1_bsp are selected (ticked)

To Program the board:
1: Nios II->Quartus Prime Programmer
2: Click Add File and browse to 'group2.sof' select and click open
3: Click the file from the list and click Start
4: Open the project in Nios II 16.1 Build tools
5: Run->Run Configurations
6: Double Click Nios II Hardware
7: Select Group2_A1 as Project name and ensure correct elf (Group2_A1.elf) is selected
8: Goto the Target Connection tab select USB-Blaser in the table, if there are no entries in the table
Ensure the USB Blaster cable is connected, steps 1-3 have been completed and click Refresh connections
9: Click Apply, then click Run (If Run is greyed out tick 'Ignore mismatched system ID' and 'Ignore mismatched
system timestamp' under System ID check in the Target Connection tab)
10: The Device should successfully program and the board can be operated as described below


EXTERNAL INPUT ALLOCATIONS:
KEY0 – NS pedestrian button 	
KEY1 – EW pedestrian button
KEY2 – Indicates when a vehicle enters & leaves the intersection
SW0 – mode 1		
SW1 - mode 2 
SW2 - mode 3 		
SW3 - mode 4  
SW17 – Initiates the system to receive traffic light timing instructions from UART

KEY3 - Device Global reset (this tends to cause an error in the board which will require 
it to be reprogrammed; recommended not to press this button)


EXTERNAL OUTPUTS ALLOCATIONS:
LEDG0 – NS green light	 	
LEDG1- NS yellow light		
LEDG2 – NS Red light
LEDG3- EW green light	 	
LEDG4 – EW yellow light		
LEDG5 – EW Red light
LEDR0 – NS pedestrian waiting light	 
LEDR1 – EW pedestrian waiting light
LEDG6 – NS pedestrian walk light	 
LEDG7 – EW pedestrian walk light


OPERATION INTRUCTIONS:
Each mode can be changed using the corresponding switches: SW0, SW1, SW2, SW3.
The mode will only be changed when one of the switches are active, if more than one, or none are active, 
the device will stay in mode 0. Mode switches only occur when the device is in a RR (safe) state 
(when LEDG2 & LEDG5 are on at the same time), Current mode is displayed on the on-board lcd.


MODE 0:
Mode 0 is active when no other mode is active, or if more than one mode is selected at the same time. 
Lights will remain in its RR state


MODE 1:
Mode 1 is activated when SW0 is turned on. Mode 1 alternates states at fixed, predetermined times. 
No external switches are active in this mode


MODE 2:
Mode 2 is activated when SW1 is turned on. Mode 2 acts identically to mode 1, but with additional 
external pedestrian inputs. A KEY0 press indicates a NS pedestrian button press. When LEDR0 is active 
this indicates that the pedestrian is waiting to cross, when LEDR0 turns off and LEDG6 turns on, this 
indicates that the pedestrian can safely cross in the NS direction. The same is true for the EW direction 
with KEY1, LEDR1 & LEDG7 respectively.


MODE 3:
Mode 3 is activated when SW2 is turned on. Mode 3 implements both previous modes but now allows custom 
state change timings through UART.
 
Where:
t1-t6 are the times between state changes 
States are read in the order of NS light then EW light
t1: R,R -> G,R	
t2: G,R -> Y,R	
t3: Y,R->R,R	
t4: R,R->R,G	
t5: R,G->R,Y	
t6: R,Y->R,R

timing instructions should be sent in the order of t1,t2,t3,t4,t5,t6. These timings should be sent in ms 
separated by a comma and executed using the enter key. Use SW17 to tell the device to wait for input. 
When the screen prompts ‘WAITING FOR INPUT’ SW17 can be turned off and the input can be entered
e.g. to change each transition to 1 second intervals the input command should be as follows when the 
device prompts ‘WAITING FOR INPUT’ enter: 1000,1000,1000,1000,1000,1000
Inputs not in this format will not be stored and 'INVALID PACKET' will be printed.


MODE 4:
Mode 4 implements all previous modes, but also allows for a tlc camera function. 
When the device is in R,Y or Y,R state and KEY2 is pressed a “Camera activated” message will be printed. 
KEY2 is pressed a second time to indicate a vehicle leaving the intersection. If a vehicle is in the 
intersection for longer than 2 seconds a “snapshot taken” message will be printed. Every time the camera 
timer is initiated and a vehicle leaves the intersection, the time the vehicle was in the intersection 
will be printed in ms. 


NOTE: For modes 3 & 4 status and values will be printed through uart to PuTTY (or any connected terminal 
program) as well as to the on-board lcd screen.


