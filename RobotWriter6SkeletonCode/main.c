#include <stdio.h>
#include <stdlib.h>
//#include <conio.h>
//#include <windows.h>
#include "rs232.h"
#include "serial.h"

#define bdrate 115200               /* 115200 baud */
#define MAX_CHARACTERS 128
//Strucutre to store font data for each ASCII value
typedef struct
{
    int charCode;
    int numStrokes;
    int (*strokeData)[3];
}  CharacterData;

//Function declerations
void SendCommands (char *buffer );
double getTextHeight(); //Prompts user to input a height for the text between 4-10mm
double calculateScaleFactor(double textHeight); //Calculates the scale factor based on the text height
CharacterData* loadFontData(const char *filename, int *numCharacters);


int main()
{
    //Varible definition
    double textHeight,scaleFactor; 
    int numCharacters=0;

    const char *fontFile="SingleStrokeFont.txt"; //Name of font file
    
    //Load font data into system
    CharacterData *fontArray=loadFontData(fontFile, &numCharacters);
    if (!fontArray)
    {
        return 1; //If font loading fails, exit 
    }
    printf("Loaded %d characters from font file. \n", numCharacters);
    
    //char mode[]= {'8','N','1',0};
    char buffer[100];

    //Calls getTextHeight function
    textHeight=getTextHeight();

    //Calls calculateScaleFactor function
    scaleFactor=calculateScaleFactor(textHeight);
    printf("Scale Factor: %.4f\n", scaleFactor);

    

    // If we cannot open the port then give up immediately
    if ( CanRS232PortBeOpened() == -1 )
    {
        printf ("\nUnable to open the COM port (specified in serial.h) ");
        exit (0);
    }

    // Time to wake up the robot
    printf ("\nAbout to wake up the robot\n");

    // We do this by sending a new-line
    sprintf (buffer, "\n");
     // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    Sleep(100);

    // This is a special case - we wait  until we see a dollar ($)
    WaitForDollar();

    printf ("\nThe robot is now ready to draw\n");

        //These commands get the robot into 'ready to draw mode' and need to be sent before any writing commands
    sprintf (buffer, "G1 X0 Y0 F1000\n");
    SendCommands(buffer);
    sprintf (buffer, "M3\n");
    SendCommands(buffer);
    sprintf (buffer, "S0\n");
    SendCommands(buffer);


    // These are sample commands to draw out some information - these are the ones you will be generating.
    sprintf (buffer, "G0 X-13.41849 Y0.000\n");
    SendCommands(buffer);
    sprintf (buffer, "S1000\n");
    SendCommands(buffer);
    sprintf (buffer, "G1 X-13.41849 Y-4.28041\n");
    SendCommands(buffer);
    sprintf (buffer, "G1 X-13.41849 Y0.0000\n");
    SendCommands(buffer);
    sprintf (buffer, "G1 X-13.41089 Y4.28041\n");
    SendCommands(buffer);
    sprintf (buffer, "S0\n");
    SendCommands(buffer);
    sprintf (buffer, "G0 X-7.17524 Y0\n");
    SendCommands(buffer);
    sprintf (buffer, "S1000\n");
    SendCommands(buffer);
    sprintf (buffer, "G0 X0 Y0\n");
    SendCommands(buffer);

    // Before we exit the program we need to close the COM port
    CloseRS232Port();
    printf("Com port now closed\n");

    return (0);
}

// Send the data to the robot - note in 'PC' mode you need to hit space twice
// as the dummy 'WaitForReply' has a getch() within the function.
void SendCommands (char *buffer )
{
    // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    WaitForReply();
    Sleep(100); // Can omit this when using the writing robot but has minimal effect
    // getch(); // Omit this once basic testing with emulator has taken place
}


//Function Definitions
// Function to prompt and validate text height input
double getTextHeight()
{
    double textHeight;
    int valid=0;

    while (!valid) 
    {
        printf("Enter text height in mm (between 4 and 10 mm): "); //Prompts user for an input
        if (scanf("%lf", &textHeight)!=1) 
        {
            printf("Invalid input. Please enter a numeric value.\n");
            while (getchar()!='\n'); // Clear input buffer
        } else if (textHeight<4||textHeight>10) //Validates input is within 4-10mm range
        {
            printf("Invalid text height. It must be between 4 and 10 mm.\n");
        } else 
        {
            valid=1; // Input is valid
        }
    }
    return textHeight;
}
// Function to calculate the scale factor
double calculateScaleFactor(double textHeight) 
{
    return textHeight/18.0;
}

// Function to load font data from a file
CharacterData* loadFontData(const char *filename, int *numCharacters) 
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open font file %s\n", filename);
        return NULL;
    }

    CharacterData *fontArray=malloc(MAX_CHARACTERS*sizeof(CharacterData));
    if (!fontArray) {
        printf("Error: Memory allocation failed.\n");
        fclose(file);
        return NULL;
    }

    *numCharacters=0; // Initialize character count
    while (!feof(file)) {
        int marker, charCode, numStrokes;

        // Read marker, charCode, and numStrokes
        if (fscanf(file, "%d %d %d", &marker, &charCode, &numStrokes)!= 3||marker!=999) 
        {
            break;
        }

        CharacterData *currentChar=&fontArray[*numCharacters];
        currentChar->charCode=charCode;
        currentChar->numStrokes=numStrokes;
        currentChar->strokeData=malloc(numStrokes*sizeof(int[3]));
        if (!currentChar->strokeData) 
        {
            printf("Error: Memory allocation for strokes failed.\n");
            fclose(file);
            return NULL;
        }

        // Read strokes
        for (int i=0; i < numStrokes; i++) 
        {
            fscanf(file, "%d %d %d", &currentChar->strokeData[i][0],
                   &currentChar->strokeData[i][1],
                   &currentChar->strokeData[i][2]);
        }

        (*numCharacters)++;
    }

    fclose(file);
    return fontArray;
}
