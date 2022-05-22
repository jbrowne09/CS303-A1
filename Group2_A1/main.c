//COMPSYS 303 Assignment #1
//GROUP 2

//author(s): Jonathan Browne, Tom Corne
//See README for instructions on running on DE2 Board

#include <system.h>
#include <altera_avalon_pio_regs.h>
#include <stdio.h>
#include <sys/alt_irq.h>
#include "sys/alt_alarm.h"

//macros
#define RR 36
#define RY 34
#define RG 33
#define GR 12
#define YR 20
#define RG_PNS 97
#define GR_PEW 140
#define PEW 2
#define PNS 1
#define ESC 27
#define CLEAR_LCD_STRING "[2J"

//function declarations
void lcd_set_mode(FILE* lcd, int mode);
void simple_tlc();
void pedestrian_tlc();
void configurable_tlc();
void camera_tlc();
void init_buttons_pio();
void timeout_data_handler();
void handle_vehicle_button(int entry, int snapshot, int time);

//Global variables
unsigned int switchesPrevious, switchValue = 0;
unsigned int state = RR;
unsigned int prevState = RR;
unsigned int timeOutValue = 0;
unsigned int mode = 0;
unsigned int prevMode = 5;
unsigned int timeOutValues[6] = {500,6000,2000,500,6000,2000};
unsigned int intersectionTime = 0;

//Global flags
unsigned int pedNS = 0;
unsigned int pedEW = 0;
unsigned int modeChange = 5;
unsigned int reconfigure = 0;
unsigned int entered = 0;
unsigned int takeSnapshot = 0;
unsigned int printEntered = 0;
unsigned int printExited = 0;

FILE* lcd;
FILE* uart;
alt_alarm timer;
alt_alarm cameraTimer;


//timer interrupt to execute the appropriate traffic controller based on mode
alt_u32 tlc_timer_isr(void* context)
{
	switch (mode) {
	case 1:
		simple_tlc();
		break;
	case 2:
		pedestrian_tlc();
		break;
	case 3:
		configurable_tlc();
		break;
	case 4:
		camera_tlc();
		break;
	default:
		break;
	}

	return timeOutValue;
}

//timer interrupt for counting the time a vehicle remains in the intersection,
//increments in 10ms periods and sets a flag to trigger a snapshot after 200
//iterations (2000ms/2s)
alt_u32 camera_timer_isr(void* context)
{
	intersectionTime++;

	if (intersectionTime == 200) {
		takeSnapshot = 1;
	}

	return 10;
}

//button interrupt for triggering pedestrian lights and camera/snapshots
void NSEW_ped_isr(void* context, alt_u32 id)
{
	//read the current button value and red LEDs that are on
	unsigned int buttonValue = IORD_ALTERA_AVALON_PIO_EDGE_CAP(KEYS_BASE);
	unsigned int redValue = IORD_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE);

	//only read the bit appropriate to each button
	unsigned int key0 = buttonValue & 1; //0001
	unsigned int key1 = buttonValue & 2; //0010
	unsigned int key2 = buttonValue & 4; //0100

	if (key0 == 1 && state != RG_PNS) {
		pedNS = 1;
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE, (PNS | redValue));
	}

	if (key1 == 2 && state != GR_PEW) {
		pedEW = 1;
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE, (PEW | redValue));
	}

	//if key 2 is pressed check current status of vehicle in intersection and start/stop
	//appropriate timers and set flags to print status to user
	if (key2 == 4 && mode == 4) {
		if (entered == 0 && state == RR) {
			takeSnapshot = 1;
		} else if (entered == 0 && (state == RY || state == YR)) {
			entered = 1;
			intersectionTime = 0;
			printEntered = 1;
			alt_alarm_start(&cameraTimer, 10, camera_timer_isr, NULL);
		} else if (entered == 1){
			entered = 0;
			printExited = 1;
			alt_alarm_stop(&cameraTimer);
		}
	}

	//clear the edge capture register
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(KEYS_BASE, 0);
}

int main()
{
	//Initialise Variables
	IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, RR);

	//open write for lcd
	lcd = fopen(LCD_NAME, "w");
	lcd_set_mode(lcd, 0);


	while(1)
	{
		//grab switch value for first 4 switches only (000000000000001111 or 15) and
		//set update flag for a mode change if the switch value has changed from the
		//previous iteration
		switchValue = IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE) & 15;
		if (switchValue != switchesPrevious)
		{
			switch (switchValue) {
			case 1:
				modeChange = 1;
				break;
			case 2:
				modeChange = 2;
				break;
			case 4:
				modeChange = 3;
				break;
			case 8:
				modeChange = 4;
				break;
			default:
				modeChange = 0;
				break;
			}
		}
		switchesPrevious = switchValue;

		//update mode in RR state only
		if (modeChange != 5 && state == RR) {
			//stop running timer tlc timer
			alt_alarm_stop(&timer);

			//update mode variables for mode change
			prevMode = mode;
			mode = modeChange;
			modeChange = 5;

			//reset any triggered pedestrian flags/lights and disable button interrupts
			pedNS = 0;
			pedEW = 0;
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE, 0); //turn off red leds.
			IOWR_ALTERA_AVALON_PIO_IRQ_MASK(KEYS_BASE, 0); //disable interrupts.

			//reset timeOut values for Mode 3 back to default
			timeOutValues[0] = 500;
			timeOutValues[1] = 6000;
			timeOutValues[2] = 2000;
			timeOutValues[3] = 500;
			timeOutValues[4] = 6000;
			timeOutValues[5] = 2000;

			//write the current mode to the lcd
			lcd_set_mode(lcd, mode);
		}

		//update appropriate variables, start timers etc. appropriate for new mode
		if (mode != prevMode) {
			switch (mode) {
			case 1:
				state = RR;
				alt_alarm_start(&timer, 500, tlc_timer_isr, NULL);
				break;
			case 2:
				state = RR;
				init_buttons_pio();
				alt_alarm_start(&timer, 500, tlc_timer_isr, NULL);
				break;
			case 3:
				state = RR;
				init_buttons_pio();
				alt_alarm_start(&timer, 500, tlc_timer_isr, NULL);
				break;
			case 4:
				state = RR;
				init_buttons_pio();
				alt_alarm_start(&timer, 500, tlc_timer_isr, NULL);
				break;
			default:
				state = RR;

				//disable button interrupts
				IOWR_ALTERA_AVALON_PIO_IRQ_MASK(KEYS_BASE, 0);
				break;
			}
			prevMode = mode;
		}

		//if flag to reconfigure has been set; stop tlc timer and call data packet handler
		if (reconfigure == 1) {
			reconfigure = 0;
			alt_alarm_stop(&timer);
			timeout_data_handler();
			alt_alarm_start(&timer, timeOutValues[0], tlc_timer_isr, NULL);
		}

		//handle printing vehicle enter/leave status based on flags set in interrupt, calls
		//handle_vehicle_button to print via uart and onto the lcd
		if (printEntered == 1) {
			printEntered = 0;
			handle_vehicle_button(1, 0, 0);
		} else if (printExited == 1) {
			printExited = 0;
			handle_vehicle_button(0, 0, intersectionTime*10);
		}

		//printing for taking a snapshot
		if (takeSnapshot == 1) {
			takeSnapshot = 0;
			handle_vehicle_button(2, 1, 0);
		}
	}

	fclose(lcd);
	return 0;
}

//simple_tlc function to implement base light controller for mode 1
void simple_tlc()
{
	switch (state)
	{
	case RR:
		if (prevState == RY) {
			state = GR;
		} else {
			state = RG;
		}

		timeOutValue = 6000;
		break;
	case RG:
		state = RY;
		timeOutValue = 2000;
		break;
	case RY:
		prevState = RY;
		state = RR;
		timeOutValue = 500;
		break;
	case GR:
		state = YR;
		timeOutValue = 2000;
		break;
	case YR:
		prevState = YR;
		state = RR;
		timeOutValue = 500;
		break;
	}

	//update leds based on current state
	IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, state);
}

//pedestrian_tlc function implements base light controller with additional
//pedestrian button functionality for mode 2
void pedestrian_tlc()
{
	unsigned int redValue = IORD_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE);

	switch (state)
	{
	case RR:
		//implement new states for if a pedestrian button has been pressed
		if (prevState == RY) {
			if (pedEW == 1) {
				prevState = GR_PEW;
				state = GR_PEW;
				IOWR_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE, (~PEW & redValue));
			} else {
				state = GR;
			}
		} else {
			if (pedNS == 1) {
				prevState = RG_PNS;
				state = RG_PNS;
				IOWR_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE, (~PNS & redValue));
			} else {
				state = RG;
			}
		}

		timeOutValue = 6000;
		break;
	case RG:
		state = RY;
		timeOutValue = 2000;
		break;
	case RY:
		prevState = RY;
		state = RR;
		timeOutValue = 500;
		break;
	case GR:
		state = YR;
		timeOutValue = 2000;
		break;
	case YR:
		prevState = YR;
		state = RR;
		timeOutValue = 500;
		break;
	case RG_PNS:
		state = RY;
		pedNS = 0;
		timeOutValue = 2000;
		break;
	case GR_PEW:
		state = YR;
		pedEW = 0;
		timeOutValue = 2000;
		break;
	}

	//update leds based on current state
	IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, state);
}

//configurable_tlc function implements previous mode controller functionality as
//well as reconfigurable timeout values via packets sent over uart for mode 3
void configurable_tlc()
{
	unsigned int redValue = IORD_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE);

	switch (state)
	{
	case RR:
		if (prevState == RY) {
			if (pedEW == 1) {
				prevState = GR_PEW;
				state = GR_PEW;
				IOWR_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE, (~PEW & redValue));
			} else {
				state = GR;
			}
			timeOutValue = timeOutValues[1];

		} else {
			if (pedNS == 1) {
				prevState = RG_PNS;
				state = RG_PNS;
				IOWR_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE, (~PNS & redValue));
			} else {
				state = RG;
			}
			timeOutValue = timeOutValues[4];

		}

		break;
	case RG:
		state = RY;
		timeOutValue = timeOutValues[5];
		break;
	case RY:
		prevState = RY;
		state = RR;
		timeOutValue = timeOutValues[0];
		break;
	case GR:
		state = YR;
		timeOutValue = timeOutValues[2];
		break;
	case YR:
		prevState = YR;
		state = RR;
		timeOutValue = timeOutValues[3];
		break;
	case RG_PNS:
		state = RY;
		pedNS = 0;
		timeOutValue = timeOutValues[5];
		break;
	case GR_PEW:
		state = YR;
		pedEW = 0;
		timeOutValue = timeOutValues[2];
		break;
	}

	//update leds based on current state
	IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, state);

	//if in state RR and switch is on; set flag to wait for new valid timeout values to be sent through uart
	int configSwitchValue = IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE) & 131072;
	if (state == RR) {
		if (configSwitchValue == 131072) {
			reconfigure = 1;
		}
	}
}

//camera_tlc function implements previous mode controller functionality, this
//function implements no new functionality but is used for clarity and for
//potential future additional functionality for mode 4
void camera_tlc() {
	unsigned int redValue = IORD_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE);

	switch (state)
	{
	case RR:
		if (prevState == RY) {
			if (pedEW == 1) {
				prevState = GR_PEW;
				state = GR_PEW;
				IOWR_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE, (~PEW & redValue));
			} else {
				state = GR;
			}
			timeOutValue = timeOutValues[1];

		} else {
			if (pedNS == 1) {
				prevState = RG_PNS;
				state = RG_PNS;
				IOWR_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE, (~PNS & redValue));
			} else {
				state = RG;
			}
			timeOutValue = timeOutValues[4];

		}

		break;
	case RG:
		state = RY;
		timeOutValue = timeOutValues[5];
		break;
	case RY:
		prevState = RY;
		state = RR;
		timeOutValue = timeOutValues[0];
		break;
	case GR:
		state = YR;
		timeOutValue = timeOutValues[2];
		break;
	case YR:
		prevState = YR;
		state = RR;
		timeOutValue = timeOutValues[3];
		break;
	case RG_PNS:
		state = RY;
		pedNS = 0;
		timeOutValue = timeOutValues[5];
		break;
	case GR_PEW:
		state = YR;
		pedEW = 0;
		timeOutValue = timeOutValues[2];
		break;
	}

	//update leds based on current state
	IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, state);

	//if in state RR and switch is on; set flag to wait for new valid timeout values to be sent through uart
	int configSwitchValue = IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE) & 131072;
	if (state == RR) {
		if (configSwitchValue == 131072) {
			reconfigure = 1;
		}
	}
}

//function to handle printing status of vehicle entering/leaving the intersection, prints to
//both uart and the board lcd
void handle_vehicle_button(int entry, int snapshot, int time) {
	uart = fopen(UART_NAME, "w");

	if (uart != NULL) {
		if (entry == 1) {

			fprintf(uart, "Camera Activated\r\n");
			if (lcd != NULL) {
				fprintf(lcd, "%c%s", ESC, CLEAR_LCD_STRING);
				fprintf(lcd, "MODE: %d\n", mode);
				fprintf(lcd, "Camera Activated\n");
			}
		} else if (entry == 0) {

			fprintf(uart, "Vehicle Left\r\n");
			fprintf(uart, "Time: %d ms\r\n", time);
			if (lcd != NULL) {
				fprintf(lcd, "%c%s", ESC, CLEAR_LCD_STRING);
				fprintf(lcd, "MODE: %d\n", mode);
				fprintf(lcd, "Vehicle Left, Time: %d ms\n", time);
			}
		}

		if (snapshot == 1) {
			fprintf(uart, "Snapshot taken\r\n");
			if (lcd != NULL) {
				fprintf(lcd, "%c%s", ESC, CLEAR_LCD_STRING);
				fprintf(lcd, "MODE: %d\n", mode);
				fprintf(lcd, "Snapshot Taken\n");
			}
		}
		fclose(uart);
	}
}

//function to handle waiting for recieving and parsing character values sent over uart, to
//be used as new timeout values for the controller
void timeout_data_handler()
{
	uart = fopen(UART_NAME, "r+");
	fprintf(uart, "WAITING FOR INPUT\r\n");
	if (lcd != NULL) {
		fprintf(lcd, "%c%s", ESC, CLEAR_LCD_STRING);
		fprintf(lcd, "Waiting for\n");
		fprintf(lcd, "Input Packet\n");
	}

	if (uart != NULL) {
		int valid = 0;
		int values[6];

		//this loop will exit (valid == 1) after a valid packet is successfully read
		while (valid == 0)
		{
			int packet[30];
			int i = 0;
			int readChar = getc(uart);
			valid = 1;

			//loop through saving input characters into a full packet array
			while ((char)readChar != '\n' && (char)readChar != '\r')
			{
				if (i < 30) {
					packet[i] = readChar;
					i++;
				}
				readChar = getc(uart);
			}

			//debug loop to print what the controller has recieved back to the user
			fprintf(uart, "RECIEVED: ");
			for (int p = 0; p < i; p++) {
				fprintf(uart, "%c", (char)packet[p]);
			}
			fprintf(uart, "\r\n");

			int vc = 0;
			int k = 0;
			int value[4] = {0,0,0,0};

			//scan through read in string and separate values, determine if valid values
			for (int j = 0; j <= i; j++)
			{
				int currentChar = packet[j];

				if ((char)currentChar == ',' || j == i) {

					//no value present invalid packet
					if (vc <= 0) {
						valid = 0;
						break;
					}

					int valuesOne = 0;
					int valuesTen = 0;
					int valuesHundred = 0;
					int valuesThousand = 0;

					//parse character representations into their integer form with appropriate digit values
					if (vc == 4) {
						valuesOne = value[3] - 48;
						valuesTen = (value[2] - 48)*10;
						valuesHundred = (value[1] - 48)*100;
						valuesThousand = (value[0] - 48)*1000;
					} else if (vc == 3) {
						valuesOne = value[2] - 48;
						valuesTen = (value[1] - 48)*10;
						valuesHundred = (value[0] - 48)*100;
					} else if (vc == 2) {
						valuesOne = value[1] - 48;
						valuesTen = (value[0] - 48)*10;
					} else {
						valuesOne = value[0] - 48;
					}

					if (k < 6) {
						values[k] = valuesOne + valuesTen + valuesHundred + valuesThousand;
					}
					k++;
					vc = 0;
				} else {
					//if character does not represent a number: invalid packet
					if (currentChar > 47 && currentChar < 58) {
						if (vc < 4) {
							value[vc] = currentChar;
						}
						vc++;
					} else {
						valid = 0;
						break;
					}
				}

				//number with more than 4 digits is invalid or invalid packet with more than 6 values
				if (vc > 4 || k > 6)
				{
					valid = 0;
					break;
				}
			}

			//if a value was invalid or there are not 5 values exactly packet is invalid, print to user
			if (valid != 1 || k != 6) {
				valid = 0;


				fprintf(uart, "INVALID PACKET\r\n");
				if (lcd != NULL) {
					fprintf(lcd, "%c%s", ESC, CLEAR_LCD_STRING);
					fprintf(lcd, "MODE: %d\n", mode);
					fprintf(lcd, "INVALID PACKET\n");
					fprintf(lcd, "Enter new Packet\n");
				}
			} else {
				//copy successfully read values into global timeout array
				for (int i = 0; i < 6; i++) {
					timeOutValues[i] = values[i];
				}
			}
		}
	}
	fclose(uart);
	fprintf(uart, "WAITING FOR INPUT\r\n");
	if (lcd != NULL) {
		fprintf(lcd, "%c%s", ESC, CLEAR_LCD_STRING);
		fprintf(lcd, "MODE: %d\n", mode);
		fprintf(lcd, "Updated Timeouts\n");
	}
}

void init_buttons_pio()
{
	//clear edge capture register
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(KEYS_BASE, 0);

	//enable interrupts on key0 and key1
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(KEYS_BASE, 7);

	//register the isr
	alt_irq_register(KEYS_IRQ, NULL, NSEW_ped_isr);
}

//simple function to print one line representing the current mode to the user
void lcd_set_mode(FILE* lcd, int mode)
{
	if (lcd != NULL) {
		fprintf(lcd, "%c%s", ESC, CLEAR_LCD_STRING);
		fprintf(lcd, "MODE: %d\n", mode);
	}
}
