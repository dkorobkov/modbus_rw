/*
 ============================================================================
 Name        : OwenShow.c
 Author      : 
 Version     :
 Copyright   : 
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <modbus/modbus.h>
#include <modbus/modbus-rtu.h>
#include <errno.h>
#include <getopt.h>

#define bool int
#define false 0
#define true 1

#define MAX_REG_ADDR 1000 // максимальный номер регистра

char gszDevice[32] = "/dev/ttyS0";
int gBaudrate = 9600;
int gAddress = -1;
int gSlaveId = -1;
int gFunction = 4; // используемая фуункция для чтения
// Специально такое значение, чтобы видеть, что не поставили из командной строки
unsigned int gNewValue = 0xffffffff;
int gDebug = 0;
int gRead = 0;
int gWrite = 0;
int gVerbose = 0;


void PrintHelp()
{
	fprintf(stderr, "Modbus reader/writer. Reads using Modbus function 3 or 4, or writes byte into given register "
			"of given slave using Modbus function 6\n"
			"Usage: modbus [-b baudrate] [-d serial_device] [-D<ebug>] [-v] [-f function] -R|W -s id_slave -a address -r new_value\n"
			"New value can be decimal or hex (0x-prepended)\n"
			"Function can be 3 or 4, valid for reading only\n"
			"v - be verbose\n"
			"\n");
}


// Parses command line, returns 0 on success.
int ParseCmdLine(int argc, char **argv)
{
//    int index;
	int c;
//    char* endptr = NULL;
//    extern int gOverrideDebugEventMask;

	int ret = 0;
//	int opterr = 0; //If the value of this variable is nonzero, then getopt prints an error message to the standard error stream if it encounters an unknown option character or an option with a missing required argument. This is the default behavior. If you set this variable to zero, getopt does not print any messages, but it still returns the character ? to indicate an error.
	// int optopt: When getopt encounters an unknown option character or an option with a missing required argument, it stores that option character in this variable. You can use this for providing your own diagnostic messages.
	// int optind: This variable is set by getopt to the index of the next element of the argv array to be processed. Once getopt has found all of the option arguments, you can use this variable to determine where the remaining non-option arguments begin. The initial value of this variable is 1.
	// char * optarg       This variable is set by getopt to point at the value of the option argument, for those options that accept arguments.
	// "abc:" The 3rd "options" argument is a string that specifies the option characters that are valid for this program. An option character in this string can be followed by a colon (‘:’) to indicate that it takes a required argument. If an option character is followed by two colons (‘::’), its argument is optional; this is a GNU extension.

	const char* szArgList = "hb:d:a:r:s:DRWf:";

	while ((c = getopt(argc, argv, szArgList)) != -1) //xyo:
	{
		bool bBadArg = false;
		char ch = c;
		unsigned short us;

		switch (ch)
		{
		case 'R':
			gRead = 1;
			break;
		case 'W':
			gWrite = 1;
			break;
		case 'D':
			gDebug = 1;
			break;
		case 'h':
			PrintHelp();
			return -2;
		case 'b':
			c = atoi(optarg); //strtol(optarg, (char**)NULL, 10);
			if (c == 2400 || c == 4800 || c == 9600 || c == 19200 || c == 38400 || c == 57600 || c == 115200)
				gBaudrate = c;
			else
				bBadArg = true;
			break;
		case 'd':
			strncpy(gszDevice, optarg, sizeof(gszDevice) - 1);
			break;
		case 'a':
			c = atoi(optarg); //strtol(optarg, (char**)NULL, 10);
			if (c >= 0 && c < MAX_REG_ADDR)
				gAddress = c;
			else
				bBadArg = true;
			break;
		case 'f':
			c = atoi(optarg); //strtol(optarg, (char**)NULL, 10);
			if (c == 3 || c == 4)
				gFunction = c;
			else
				bBadArg = true;
			break;
		case 's':
			c = atoi(optarg); //strtol(optarg, (char**)NULL, 10);
			if (c > 0 && c < 255)
				gSlaveId = c;
			else
				bBadArg = true;
			break;
		case 'r':
			us = strtol(optarg, (char**)NULL, 0); // auto detect: 0x/dec
			if (us >= 0 && us < 65536)
				gNewValue = us;
			else
				bBadArg = true;
			break;
		default:
			return -1;
//          abort ();
		}

		if (bBadArg == true)
		{
			fprintf(stderr, "Bad argument %c (try -h)\n", ch);
			return -3;
		}

	}

	return ret;
}

/*
// Неявно используем байт с терминирующим нулём
void StringToUnsignedShorts(char* s)
{
	while(*s != 0)
	{
		char* c = *(s+1);
		*(s+1) = *s;
		*s = c;
		if(c == 0) // *(s+1) == 0
		{
			*s = ' ';
			break;
		}
		s += 2;
	}
}*/

int main(int argc, char** argv)
{
	modbus_t* hModbus = NULL;

	if(ParseCmdLine(argc, argv) < 0)
		return -1;

	printf("\n");

	const char* szCannot = "not set, cannot continue\n\n";

	if(gAddress > MAX_REG_ADDR || gAddress < 0)
	{
		fprintf(stderr, "Register address %s", szCannot);
		PrintHelp();
		return -1;
	}

	if(gSlaveId > 247 || gSlaveId < 1)
	{
		fprintf(stderr, "Slave id %s", szCannot);
		PrintHelp();
		return -1;
	}

	if(gRead == 0 && gWrite == 0)
	{
		fprintf(stderr, "Read/Write %s", szCannot);
		PrintHelp();
		return -1;
	}

	if(gRead == 1 && gWrite == 1)
	{
		fprintf(stderr, "Read/Write mode cannot be understood\n");
		PrintHelp();
		return -1;
	}

	if(gNewValue > 65535 && gWrite == 1)
	{
		fprintf(stderr, "Output value %s", szCannot);
		PrintHelp();
		return -1;
	}

	if(gVerbose == 1)
	{
		if(gWrite == 1)
			printf("Writing %d(0x%x) to Modbus slave at address %d: %d-8-n-1 through %s\n",
					gNewValue, gNewValue, gAddress, gBaudrate, gszDevice);//, LIBMODBUS_VERSION_STRING);
		else
			printf("Reading from Modbus slave at address %d: %d-8-n-1 through %s\n",
					gAddress, gBaudrate, gszDevice);//, LIBMODBUS_VERSION_STRING);
	}

	hModbus = modbus_new_rtu(gszDevice, gBaudrate, 'N', 8, 1);
	if(modbus_connect(hModbus) < 0)
	{
		fprintf(stderr, "modbus_connect(): %s\r\n", modbus_strerror(errno));
		goto error;
	}

	if(gDebug)
		modbus_set_debug(hModbus, true);

	if( modbus_set_slave(hModbus, gSlaveId) < 0)
	{
		fprintf(stderr, "modbus_set_slave(): %s\r\n", modbus_strerror(errno));
		goto error;
	}

	if(gWrite == 1)
	{
		uint16_t u16 = gNewValue & 0xffff;

		if( modbus_write_register(hModbus, gAddress, u16) < 0)
		{
			fprintf(stderr, "modbus_write_register(): %s\r\n", modbus_strerror(errno));
			goto error;
		}
	}

	if(gRead == 1)
	{
		if(gFunction == 3)
		{
			uint16_t u16;

			if( modbus_read_registers(hModbus, gAddress, 1, &u16) < 0)
			{
				fprintf(stderr, "modbus_write_register(): %s\r\n", modbus_strerror(errno));
				goto error;
			}
			else
				printf("%d\n", u16);
		}
		else if(gFunction == 4)
		{
			uint16_t u16;
			if( modbus_read_input_registers(hModbus, gAddress, 1, &u16) < 0)
			{
				fprintf(stderr, "modbus_write_register(): %s\r\n", modbus_strerror(errno));
				goto error;
			}
			else
				printf("%d\n", u16);
		}
		else
		{
			fprintf(stderr, "Bad read function\n");
			goto error;
		}

	}

	modbus_close(hModbus);
	modbus_free(hModbus);

	return EXIT_SUCCESS;

error:
	modbus_close(hModbus);
	modbus_free(hModbus);

	return EXIT_FAILURE;
}
